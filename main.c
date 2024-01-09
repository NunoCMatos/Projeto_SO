#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

typedef struct {
	pthread_t thread_id;
	int thread_num;
	int num_threads;
	int input_fd, output_fd;
	int num_lines;
} ThreadData;


void *thread_function(void *args) {

	ThreadData *data = (ThreadData *) args;
	int input_fd = data->input_fd;
	int output_fd = data->output_fd;
	int thread_num = (int) data->thread_num;
	int num_threads = (int) data->num_threads;
	int num_lines = data->num_lines;
	int *status = malloc(sizeof(int));
	

	while (1) {
		unsigned int event_id, delay, thread_id;
		size_t num_rows, num_columns, num_coords;
		size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
		enum Command cmd;

		switch (cmd = get_next(input_fd)) {
			case CMD_CREATE:
				if (num_lines % num_threads == thread_num % num_threads) {
					printf("Thread %d ran %u\n", thread_num, cmd);
					if (parse_create(input_fd, &event_id, &num_rows, &num_columns) != 0) {
						fprintf(stderr, "Invalid command. See HELP for usage\n");
						continue;
					}

					if (ems_create(event_id, num_rows, num_columns))
						fprintf(stderr, "Failed to create event\n");
				}
				
				break;

			case CMD_RESERVE:
				if (num_lines % num_threads == thread_num % num_threads) {
					printf("Thread %d ran %u\n", thread_num, cmd);
					num_coords = parse_reserve(input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

					if (num_coords == 0) {
						fprintf(stderr, "Invalid command. See HELP for usage\n");
						continue;
					}

					if (ems_reserve(event_id, num_coords, xs, ys))
						fprintf(stderr, "Failed to reserve seats\n");
				}

				break;

			case CMD_SHOW:
				if (num_lines % num_threads == thread_num % num_threads) {
					printf("Thread %d ran %u\n", thread_num, cmd);
					if (parse_show(input_fd, &event_id) != 0) {
						fprintf(stderr, "Invalid command. See HELP for usage\n");
						continue;
					}

					if (ems_show(output_fd, event_id)) 
						fprintf(stderr, "Failed to show event\n");
				}

				break;

			case CMD_LIST_EVENTS:
				if (num_lines % num_threads == thread_num % num_threads) {
					printf("Thread %d ran %u\n", thread_num, cmd);
					if (ems_list_events(output_fd))
						fprintf(stderr, "Failed to list events\n");
				}

				break;

			case CMD_WAIT:
				if (parse_wait(input_fd, &delay, &thread_id) == -1) {  // thread_id is not implemented
					fprintf(stderr, "Invalid command. See HELP for usage\n");
					continue;
				}

				if (((int) thread_id == thread_num || (int) thread_id == 0) && delay > 0) {
					printf("Waiting...\n");
					ems_wait(delay);
				}

				break;

			case CMD_INVALID:
				fprintf(stderr, "Invalid command. See HELP for usage\n");
				break;

			case CMD_HELP:
				printf(
					"Available commands:\n"
					"  CREATE <event_id> <num_rows> <num_columns>\n"
					"  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
					"  SHOW <event_id>\n"
					"  LIST\n"
					"  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
					"  BARRIER\n"                      // Not implemented
					"  HELP\n");

				break;

			case CMD_BARRIER:  // Not implemented
				num_lines++;
				*status = 1;
				return (void *) status;

			case CMD_EMPTY:
				break;

			case EOC:
				close(input_fd);
				*status = 0;
				return (void *) status;
		}

		num_lines++;
		printf("Thread %d on line %d with command %u\n", thread_num, num_lines, cmd);
		cleanup(input_fd);
	}
}

int main(int argc, char *argv[]) {
    unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

    // Add delay if specified
    if (argc > 4) {
        char *endptr;
        unsigned long int delay = strtoul(argv[4], &endptr, 10);

        if (*endptr != '\0' || delay > UINT_MAX) {
        fprintf(stderr, "Invalid delay value or value too large\n");
        return 1;
        }

        state_access_delay_ms = (unsigned int)delay;
    }

    // Initialize EMS
    if (ems_init(state_access_delay_ms)) {
        fprintf(stderr, "Failed to initialize EMS\n");
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr, "Error: Missing arguments\n");
        return 1;
    }

	int MAX_THREADS = atoi(argv[3]);
	if (MAX_THREADS <= 0) {
		fprintf(stderr, "Invalid number of files\n");
		return 1;
	}

    int MAX_PROC = atoi(argv[2]);
    if (MAX_PROC <= 0) {
        fprintf(stderr, "Invalid number of files\n");
        return 1;
    }

    // Open directory
    struct dirent *dp;
    
    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open directory\n");
        return 1;
    }

    int status, running_children = 0;
    pid_t pid = 1;
    while(1) {
        dp = readdir(dir);
        if (dp == NULL)
            break;
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strstr(dp->d_name, ".out") != NULL)
            continue; /* Skip . and .. and *.out */
        
        if (running_children < MAX_PROC) {
            running_children++;
        } else {
            wait(&status);
            printf("%d\n", status);
        }

        pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Failed to fork\n");
            return 1;
        }
		
        if (pid == 0) {
            // Prepare file path
            char input_file_path[PATH_MAX], output_file_path[PATH_MAX];
            snprintf(input_file_path, PATH_MAX, "%s/%s", argv[1], dp->d_name);
			printf("%s\n", input_file_path);

			strcpy(output_file_path, input_file_path);
			char *extension = strrchr(output_file_path, '.');  // Find the last occurrence of '.'
			if (extension != NULL) {
				// Replace everything after the dot with the new extension
				strcpy(extension + 1, "out");
			}

			// Open file
            int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
            mode_t filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

            int output_fd = open(output_file_path, openFlags, filePerms); 
            if (output_fd == -1)
                fprintf(stderr, "opening file %s", argv[2]);

            ThreadData **threads_data = malloc((unsigned) MAX_THREADS * sizeof(ThreadData *));
			if (threads_data == NULL) {
				fprintf(stderr, "Failed to allocate memory for threads_data\n");
				return 1;
			}

			for (int i = 0; i < MAX_THREADS; i++) {
				threads_data[i] = malloc(sizeof(ThreadData));
				if (threads_data[i] == NULL) {
					fprintf(stderr, "Failed to allocate memory for threads_data[%d]\n", i);
					return 1;
				}

				threads_data[i]->thread_num = i + 1;
				threads_data[i]->num_threads = MAX_THREADS;
				threads_data[i]->input_fd = open(input_file_path, O_RDONLY);
				if (threads_data[i]->input_fd == -1) {
					fprintf(stderr, "Failed to open file\n");
					return 1;
				}
				threads_data[i]->output_fd = output_fd;
				threads_data[i]->num_lines = 0;

				if (pthread_create(&threads_data[i]->thread_id, NULL, thread_function, (void *) threads_data[i]) != 0) {
					fprintf(stderr, "Failed to create thread\n");
					return 1;
				}
			}

			int *threads_return_value[MAX_THREADS];
			int barrier_found = 0;
			while (1) {
				for (int i = 0; i < MAX_THREADS; i++) {
					if (pthread_join(threads_data[i]->thread_id, (void **) &threads_return_value[i]) != 0) {
						fprintf(stderr, "Failed to join thread\n");
						return 1;
					}

					printf("Thread %d returned %d\n", i, *threads_return_value[i]);
					if (*threads_return_value[i] == 1) {
						barrier_found = 1;
						continue;
					} else
						barrier_found = 0;
					
					free(threads_return_value[i]);
				}

				if (!barrier_found) {
					for (int i = 0; i < MAX_THREADS; i++)
						free(threads_data[i]);
					break;

				} else {
					for (int i = 0; i < MAX_THREADS; i++) {
						if (pthread_create(&threads_data[i]->thread_id, NULL, thread_function, (void *) threads_data[i]) != 0) {
							fprintf(stderr, "Failed to create thread\n");
							return 1;
						}
					}

					barrier_found = 0;
				}
			}

			if (close(output_fd) == -1) {
                fprintf(stderr, "Failed to close file\n");
                return 1;
            }
			
            exit(0);
        }
	}

    if (pid > 0) {
        while (running_children > 0) {
            wait(&status);
            printf("%d\n", status);
            running_children--;
        }
    }

    // Close directory
    if (closedir(dir) == -1) {
        fprintf(stderr, "Failed to close directory\n");
        return 1;
    }

	// Free EMS
	if (ems_terminate()) {
		fprintf(stderr, "Failed to destroy EMS\n");
		return 1;
	}
	
	return 0;
}
