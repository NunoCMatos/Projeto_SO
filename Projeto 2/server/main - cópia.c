#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>


#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

typedef struct {
  int session_id;
} ThreadArgs;

typedef struct {
  char *req_pipe_path, *resp_pipe_path;
} ClientArgs;

pthread_mutex_t clientMutex;

sem_t semClientFull;
sem_t semClientEmpty;

ClientArgs clients[MAX_SESSION_COUNT];
int clientCount = 0;

void *consumer(void *args) {

  ThreadArgs *threadArgs = (ThreadArgs *)args;
  int client_available, session_id = threadArgs->session_id;

  while (1) {
    sem_wait(&semClientFull);
    client_available = 1;
    pthread_mutex_lock(&clientMutex);
    char *req_pipe_path = clients[clientCount - 1].req_pipe_path;
    char *resp_pipe_path = clients[clientCount - 1].resp_pipe_path;
    clientCount--;
    pthread_mutex_unlock(&clientMutex);
    sem_post(&semClientEmpty);

    int req_pipe_fd = open(req_pipe_path, O_RDONLY);
    if (req_pipe_fd == -1) {
      fprintf(stderr, "Failed to open req_pipe_fd\n");
    }

    int resp_pipe_fd = open(resp_pipe_path, O_WRONLY);
    if (resp_pipe_fd == -1) {
      fprintf(stderr, "Failed to open req_pipe_fd\n");
    }

    if (write(resp_pipe_fd, &session_id, sizeof(int)) == -1) {
      fprintf(stderr, "Failed to write session_id\n");
    }

    while (1) {
      char op_code;

      if (!client_available) {
        break;
      }

      if (read(req_pipe_fd, &op_code, sizeof(char)) == -1) {
        fprintf(stderr, "Failed to read op_code\n");
      }

      fprintf(stderr, "%c: %d\n", op_code, client_available);

      if (read(req_pipe_fd, &session_id, sizeof(int)) == -1) {
        fprintf(stderr, "Failed to read session_id\n");
      }

      unsigned int event_id, response;
      size_t num_rows, num_columns, num_coords;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

      switch (op_code) {
        case '2':
          // ems_quit();

          fprintf(stderr, "Quit\n");
          close(req_pipe_fd);
          close(resp_pipe_fd);

          client_available = 0;

          break;

        case '3':
          // ems_create();

          if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read event_id\n");
          }

          if (read(req_pipe_fd, &num_rows, sizeof(size_t)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read num_rows\n");
            break;
          }

          if (read(req_pipe_fd, &num_columns, sizeof(size_t)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read num_columns\n");
            break;
          }

          if (ems_create(event_id, num_rows, num_columns)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to create event\n");
            break;
          }

          response = 0;
          if (write(resp_pipe_fd, &response, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to write response\n");
            break;
          }
          break;

        case '4':
          // ems_reserve();

          if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read event_id\n");
            break;
          }

          if (read(req_pipe_fd, &num_coords, sizeof(size_t)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read num_coords\n");
            break;
          }

          if (read(req_pipe_fd, xs, sizeof(size_t) * num_coords) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read xs\n");
            break;
          }

          if (read(req_pipe_fd, ys, sizeof(size_t) * num_coords) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read ys\n");
            break;
          }

          if (ems_reserve(event_id, num_coords, xs, ys)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to reserve seats\n");
            break;
          }

          response = 0;
          if (write(resp_pipe_fd, &response, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to write response\n");
            break;
          }

          break;

        case '5':
          // ems_show();

          if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read event_id\n");
            break;
          }

          if (ems_show(resp_pipe_fd, event_id)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to show event\n");
            break;
          }

          break;

        case '6':
          // ems_list_events();

          if (ems_list_events(resp_pipe_fd)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to list events\n");
            break;
          }

          break;

        default:
          fprintf(stderr, "Invalid op_code\n");
          break;
      }
    }
  }

  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  // TODO: Intialize server, create worker threads
  pthread_mutex_init(&clientMutex, NULL);
  sem_init(&semClientFull, 0, 0);
  sem_init(&semClientEmpty, 0, MAX_SESSION_COUNT);

  const char* pipe_path = argv[1];

  unlink(pipe_path);

  // Create the FIFO
  if (mkfifo(pipe_path, 0777) == -1) {
    perror("mkfifo");
    return 1;
  }

  while (1) {
    int client_available, session_id = 1;
    char op_code = '0';
    char req_pipe_path[MAX_PIPE_PATH_SIZE], resp_pipe_path[MAX_PIPE_PATH_SIZE];

    // Open the FIFO
    int server_fd = open(pipe_path, O_RDONLY);
    if (server_fd == -1) {
      perror("open");
      return 1;
    }

    if (read(server_fd, &op_code, sizeof(char)) == -1) {
      fprintf(stderr, "Failed to read op_code\n");
    }

    if (op_code == '1')
      client_available = 1;
    else
      continue;

    if (read(server_fd, req_pipe_path, sizeof(char) * MAX_PIPE_PATH_SIZE) == -1) {
      fprintf(stderr, "Failed to read req_pipe_path\n");
    }

    if (read(server_fd, resp_pipe_path, sizeof(char) * MAX_PIPE_PATH_SIZE) == -1) {
      fprintf(stderr, "Failed to read resp_pipe_path\n");
    }

    int req_pipe_fd = open(req_pipe_path, O_RDONLY);
    if (req_pipe_fd == -1) {
      fprintf(stderr, "Failed to open req_pipe_fd\n");
    }

    int resp_pipe_fd = open(resp_pipe_path, O_WRONLY);
    if (resp_pipe_fd == -1) {
      fprintf(stderr, "Failed to open resp_pipe_fd\n");
    }

    if (write(resp_pipe_fd, &session_id, sizeof(int)) == -1) {
      fprintf(stderr, "Failed to write session_id\n");
    }

    while (1) {
      if (!client_available) {
        break;
      }

      if (read(req_pipe_fd, &op_code, sizeof(char)) == -1) {
        fprintf(stderr, "Failed to read op_code\n");
      }

      fprintf(stderr, "%c: %d\n", op_code, client_available);

      if (read(req_pipe_fd, &session_id, sizeof(int)) == -1) {
        fprintf(stderr, "Failed to read session_id\n");
      }

      unsigned int event_id, response;
      size_t num_rows, num_columns, num_coords;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

      switch (op_code) {
        case '2':
          // ems_quit();

          fprintf(stderr, "Quit\n");
          close(req_pipe_fd);
          close(resp_pipe_fd);
          close(server_fd);

          client_available = 0;

          break;

        case '3':
          // ems_create();

          if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read event_id\n");
          }

          if (read(req_pipe_fd, &num_rows, sizeof(size_t)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read num_rows\n");
            break;
          }

          if (read(req_pipe_fd, &num_columns, sizeof(size_t)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read num_columns\n");
            break;
          }

          if (ems_create(event_id, num_rows, num_columns)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to create event\n");
            break;
          }

          response = 0;
          if (write(resp_pipe_fd, &response, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to write response\n");
            break;
          }
          break;

        case '4':
          // ems_reserve();

          if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read event_id\n");
            break;
          }

          if (read(req_pipe_fd, &num_coords, sizeof(size_t)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read num_coords\n");
            break;
          }

          if (read(req_pipe_fd, xs, sizeof(size_t) * num_coords) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read xs\n");
            break;
          }

          if (read(req_pipe_fd, ys, sizeof(size_t) * num_coords) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read ys\n");
            break;
          }

          if (ems_reserve(event_id, num_coords, xs, ys)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to reserve seats\n");
            break;
          }

          response = 0;
          if (write(resp_pipe_fd, &response, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to write response\n");
            break;
          }

          break;

        case '5':
          // ems_show();

          if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to read event_id\n");
            break;
          }

          if (ems_show(resp_pipe_fd, event_id)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to show event\n");
            break;
          }

          break;

        case '6':
          // ems_list_events();

          if (ems_list_events(resp_pipe_fd)) {
            error_msg(resp_pipe_fd);
            fprintf(stderr, "Failed to list events\n");
            break;
          }

          break;

        default:
          fprintf(stderr, "Invalid op_code\n");
          break;
      }
    }
  }

  //TODO: Close Server
  unlink(pipe_path);

  ems_terminate();
}