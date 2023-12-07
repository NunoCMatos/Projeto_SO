#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
    unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

    // Add delay if specified
    if (argc > 3) {
        char *endptr;
        unsigned long int delay = strtoul(argv[3], &endptr, 10);

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

    // Open directory
    struct dirent *dp;
    
    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open directory\n");
        return 1;
    }

    long int MAX_PROC = strtol(argv[2], NULL, 10);
    if (MAX_PROC <= 0) {
        fprintf(stderr, "Invalid number of files\n");
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
            char file_path[PATH_MAX];
            snprintf(file_path, PATH_MAX, "%s/%s", argv[1], dp->d_name);

            // Open file
            int input_fp = open(file_path, O_RDONLY);
            if (input_fp == -1) {
                fprintf(stderr, "Failed to open file\n");
                return 1;
            }

            // Change file path to .out
            char *extension = strrchr(file_path, '.');  // Find the last occurrence of '.'

            if (extension != NULL) {
                // Replace everything after the dot with the new extension
                strcpy(extension + 1, "out");
            }

            // Open file
            int openFlags = O_CREAT | O_WRONLY | O_TRUNC;
            mode_t filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

            int output_fd = open(file_path, openFlags, filePerms); 
            if (output_fd == -1)
                fprintf(stderr, "opening file %s", argv[2]);
            
            while (1) {
                unsigned int event_id, delay;
                size_t num_rows, num_columns, num_coords;
                size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
                enum Command cmd;

                switch (cmd = get_next(input_fp)) {
                case CMD_CREATE:
                    if (parse_create(input_fp, &event_id, &num_rows, &num_columns) != 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                    }

                    if (ems_create(event_id, num_rows, num_columns)) {
                    fprintf(stderr, "Failed to create event\n");
                    }

                    break;

                case CMD_RESERVE:
                    num_coords = parse_reserve(input_fp, MAX_RESERVATION_SIZE, &event_id, xs, ys);

                    if (num_coords == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                    }

                    if (ems_reserve(event_id, num_coords, xs, ys)) {
                    fprintf(stderr, "Failed to reserve seats\n");
                    }

                    break;

                case CMD_SHOW:
                    if (parse_show(input_fp, &event_id) != 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                    }

                    if (ems_show(output_fd, event_id)) {
                    fprintf(stderr, "Failed to show event\n");
                    }

                    break;

                case CMD_LIST_EVENTS:
                    if (ems_list_events(output_fd)) {
                    fprintf(stderr, "Failed to list events\n");
                    }

                    break;

                case CMD_WAIT:
                    if (parse_wait(input_fp, &delay, NULL) == -1) {  // thread_id is not implemented
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                    }

                    if (delay > 0) {
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
                case CMD_EMPTY:
                    break;

                case EOC:
                    break;
                }

                if (cmd == EOC) {
                    break;
                }
            }

            // Close input file
            if (close(input_fp) == -1) {
                fprintf(stderr, "Failed to close file\n");
                return 1;
            }

            // Close output file
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
    

    /*
    while(1)
        Lê o ficheiro
        se o ficheiro for o ultimo
            sai

        se o numero de filhos for menor que o numero de ficheiros
            cria um filho
        se o numero de filhos for igual ao numero de ficheiros
            espera que um filho termine
            diminui o numero de filhos
        
        se for o filho
            processa o ficheiro
            sai

    se for pai
        espera que todos os filhos terminem
    */

    // Close directory
    if (closedir(dir) == -1) {
        fprintf(stderr, "Failed to close directory\n");
        return 1;
    }

    // Terminate EMS
    ems_terminate();
}
