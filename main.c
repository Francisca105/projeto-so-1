#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

#define BUFFER_SIZE 1024
#define DT_REG 8

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <dir_path> <MAX_PROC> <MAX_THREADS> [delay]\n", argv[0]);
    return 1;
  }

  if (argc > 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  char *dir_path = argv[1];
  DIR *dir = opendir(dir_path);

  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory\n");
    return 1;
  }

  struct dirent *dp;
  errno = 0;

  int MAX_PROC = atoi(argv[2]);
  int MAX_THREADS = atoi(argv[3]);
  
  int num_active_proc = 0;

  while((dp = readdir(dir)) != NULL) {

    if (dp->d_type == DT_REG && strlen(dp->d_name) > 4 && !strcmp(dp->d_name + strlen(dp->d_name) - 5, ".jobs")) {
      int status;

      if (num_active_proc == MAX_PROC) {
        wait(&status);
        num_active_proc--;

        if (WIFEXITED(status)) {
          printf("Child process exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
          printf("Child process exited due to signal %d\n", WTERMSIG(status));
        }
      }

      int pid = fork();

      if (pid == -1) {
        fprintf(stderr, "Failed to fork\n");
        return 1;
      }

      num_active_proc++;

      if (pid == 0) {
        size_t len_path = strlen(argv[1]) + strlen(dp->d_name) + 2; // +2 for '/' and '\0'

        char *jobs_file_path = (char*) malloc(len_path);
        if (jobs_file_path == NULL) {
          fprintf(stderr, "Failed to allocate memory\n");
          return 1;
        }
        strcpy(jobs_file_path, argv[1]);
        strcat(jobs_file_path, "/");
        strcat(jobs_file_path, dp->d_name);

        int jobs_fd = open(jobs_file_path, O_RDONLY);

        if (jobs_fd == -1) {
          fprintf(stderr, "Failed to open .jobs file\n");
          return 1;
        }

        int openFlags = O_WRONLY | O_CREAT | O_TRUNC;
        mode_t filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        char *out_file_path = (char*) malloc(len_path);
        if (out_file_path == NULL) {
          fprintf(stderr, "Failed to allocate memory\n");
          return 1;
        }
        memset(out_file_path, 0, len_path);
        strncpy(out_file_path, jobs_file_path, strlen(jobs_file_path) - 5);
        strcat(out_file_path, ".out");

        int out_fd = open(out_file_path, openFlags, filePerms);

        if (out_fd == -1) {
          fprintf(stderr, "Failed to open .out file\n");
          return 1;
        }

        while (1) {
          unsigned int event_id, delay;
          size_t num_rows, num_columns, num_coords;
          size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
          int toExit = 0;

          switch (get_next(jobs_fd)) {
          case CMD_CREATE:
            if (parse_create(jobs_fd, &event_id, &num_rows, &num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;

          case CMD_RESERVE:
            num_coords =
                parse_reserve(jobs_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;

          case CMD_SHOW:
            if (parse_show(jobs_fd, &event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_show(event_id, out_fd)) {
              fprintf(stderr, "Failed to show event\n");
            }

            break;

          case CMD_LIST_EVENTS:
            if (ems_list_events(out_fd)) {
              fprintf(stderr, "Failed to list events\n");
            }

            break;

          case CMD_WAIT:
            if (parse_wait(jobs_fd, &delay, NULL) ==
                -1) { // thread_id is not implemented
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (delay > 0) {
              printf("Waiting...\n");
              ems_wait(delay);
            }
            //if (delay > 0) {  TODO: perguntar ao professor o que fazer no wait
            //  char *buffer = malloc(sizeof("Waiting...\n"));
            //  strcpy(buffer, "Waiting...\n");
            //  write_to_out(out_fd, buffer);
            //  free(buffer);
            //  ems_wait(delay);
            //}

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
            //char *buffer = malloc(BUFFER_SIZE);  TODO: perguntar ao professor o que fazer no help
            //strcpy(buffer, "Available commands:\n"
            //      "  CREATE <event_id> <num_rows> <num_columns>\n"
            //      "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
            //      "  SHOW <event_id>\n"
            //      "  LIST\n"
            //      "  WAIT <delay_ms> [thread_id]\n" // thread_id is not implemented
            //      "  BARRIER\n"                     // Not implemented
            //      "  HELP\n");
            //
            //write_to_out(out_fd, buffer);
            //free(buffer);

            break;

          case CMD_BARRIER: // Not implemented
          case CMD_EMPTY:
            break;

          case EOC:
            toExit = 1;

            break;
          }

          if (toExit) {
            break;
          }
        }

      if (close(jobs_fd) == -1) {
        fprintf(stderr, "Failed to close .jobs file\n");
        return 1;
      }

      if (close(out_fd) == -1) {
        fprintf(stderr, "Failed to close .out file\n");
        return 1;
      }

      free(jobs_file_path);
      free(out_file_path);

      exit(0);
      }
    }
  }

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory\n");
    return 1;
  }

  while (num_active_proc > 0) {
    int status;
    wait(&status);
    num_active_proc--;

    if (WIFEXITED(status)) {
      printf("Child process exited with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("Child process exited due to signal %d\n", WTERMSIG(status));
    }
  }

  return ems_terminate();
}
