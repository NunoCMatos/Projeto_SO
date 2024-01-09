#include "api.h"

#include <stdio.h>

int op_code(char op_code) {
  if (write(req_pipe_fd, &op_code, sizeof(char)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  if (write(req_pipe_fd, &session_id, sizeof(int)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  return 0;
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param num_cols Number of columns.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(size_t num_cols, size_t row, size_t col) { return (row - 1) * num_cols + col - 1; }

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  // Create pipes and connect to the server

  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  req_pipe = req_pipe_path;
  resp_pipe = resp_pipe_path;

  if (mkfifo(req_pipe_path, 0777) == -1) {
    fprintf(stderr, "Error creating request pipe\n");
    return 1;
  }

  if (mkfifo(resp_pipe_path, 0777) == -1) {
    fprintf(stderr, "Error creating response pipe\n");
    return 1;
  }

  // open server pipe
  server_pipe_fd = open(server_pipe_path, O_WRONLY);
  if (server_pipe_fd == -1) {
    fprintf(stderr, "Error opening server pipe\n");
    return 1;
  }

  char buffer[82];
  memset(buffer, 0, sizeof(char) * 82);
  char op_code = '1';
  memcpy(buffer, &op_code, sizeof(char));
  memcpy(buffer + sizeof(char), req_pipe_path, sizeof(char) * MAX_PIPE_PATH_SIZE);
  memcpy(buffer + sizeof(char) * (MAX_PIPE_PATH_SIZE + 1), resp_pipe_path, sizeof(char) * MAX_PIPE_PATH_SIZE);

  if (write(server_pipe_fd, buffer, sizeof(char) * (MAX_PIPE_PATH_SIZE * 2 + 1)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  req_pipe_fd = open(req_pipe_path, O_WRONLY);
  if (req_pipe_fd == -1) {
    fprintf(stderr, "Error opening request pipe\n");
    ems_quit();
    return 1;
  }

  resp_pipe_fd = open(resp_pipe_path, O_RDONLY);
  if (resp_pipe_fd == -1) {
    fprintf(stderr, "Error opening response pipe\n");
    ems_quit();
    return 1;
  }

  if (read(resp_pipe_fd, &session_id, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  return 0;
}

int ems_quit(void) { 
  // Close pipes and disconnect from the server

  op_code('2');

  close(req_pipe_fd);
  close(resp_pipe_fd);
  close(server_pipe_fd);

  unlink(req_pipe);
  unlink(resp_pipe);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  if (op_code('3') == 1) {
    return 1;
  }

  if (write(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  if (write(req_pipe_fd, &num_rows, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  if (write(req_pipe_fd, &num_cols, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  int response;
  if (read(resp_pipe_fd, &response, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  // (char) OP_CODE=4 | (unsigned int) event_id | (size_t) num_seats | (size_t[num_seats]) conteúdo de xs | (size_t[num_seats]) conteúdo de ys

  if (op_code('4') == 1) {
    return 1;
  }

  if (write(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  if (write(req_pipe_fd, &num_seats, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  if (write(req_pipe_fd, xs, sizeof(size_t) * num_seats) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  if (write(req_pipe_fd, ys, sizeof(size_t) * num_seats) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  int response;
  if (read(resp_pipe_fd, &response, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  if (op_code('5') == 1) {
    return 1;
  }

  if (write(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
    fprintf(stderr, "Error writing to pipe\n");
    ems_quit();
    return 1;
  }

  int response;
  if (read(resp_pipe_fd, &response, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  if (response == 1) {
    return 1;
  }

  size_t num_rows, num_cols;

  if (read(resp_pipe_fd, &num_rows, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  if (read(resp_pipe_fd, &num_cols, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  unsigned int seats[num_rows * num_cols];

  if (read(resp_pipe_fd, seats, sizeof(unsigned int) * num_rows * num_cols) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  for (size_t i = 1; i <= num_rows; i++) {
    for (size_t j = 1; j <= num_cols; j++) {
      char seat_str[2];

      sprintf(seat_str, "%u", seats[seat_index(num_cols, i, j)]);
      if (write(out_fd, seat_str, 1) == -1) {
        fprintf(stderr, "Error writing to file\n");
        ems_quit();
        return 1;
      }

      if (j < num_cols) {
        if (write(out_fd, " ", 1) == -1) {
          fprintf(stderr, "Error writing to file\n");
          ems_quit();
          return 1;
        }
      }
    }

    if (write(out_fd, "\n", 1) == -1) {
      fprintf(stderr, "Error writing to file\n");
      ems_quit();
      return 1;
    }
  }

  return 0;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  if (op_code('6') == 1) {
    return 1;
  }

  int response;
  if (read(resp_pipe_fd, &response, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  if (response == 1) {
    return 1;
  }

  size_t num_events;
  if (read(resp_pipe_fd, &num_events, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  unsigned int events[num_events];
  if (read(resp_pipe_fd, events, sizeof(unsigned int) * num_events) == -1) {
    fprintf(stderr, "Error reading from pipe\n");
    ems_quit();
    return 1;
  }

  for (size_t i = 0; i < num_events; i++) {
    char event_str[11];
    sprintf(event_str, "%u", events[i]);

    if (write(out_fd, "Event: ", 7) == -1) {
      fprintf(stderr, "Error writing to file\n");
      ems_quit();
      return 1;
    }

    if (write(out_fd, event_str, 1) == -1) {
      fprintf(stderr, "Error writing to file\n");
      ems_quit();
      return 1;
    }

    if (write(out_fd, "\n", 1) == -1) {
      fprintf(stderr, "Error writing to file\n");
      ems_quit();
      return 1;
    }
  }

  return 0;
}
