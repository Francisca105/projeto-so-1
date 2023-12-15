#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

struct thread_args {
  int id;
  int MAX_THREADS;
  int jobs_fd;
  int out_fd;
  unsigned int *delays;
  pthread_mutex_t *rd_jobs_mutex;
  pthread_mutex_t *wr_out_mutex;
  pthread_rwlock_t *rwlock_events;
  pthread_rwlock_t *rwlock_seats;
};

static void *safe_malloc(size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    exit(1);
  }
  return ptr;
}

static void cleanup(int fd) {
  char ch;
  while (read(fd, &ch, 1) == 1 && ch != '\n')
    ;
}

void *thread_func(void *args) {
  struct thread_args *thread_args = (struct thread_args*) args;
  int id = thread_args->id;
  int jobs_fd = thread_args->jobs_fd;
  int out_fd = thread_args->out_fd;
  int MAX_THREADS = thread_args->MAX_THREADS;
  unsigned int *delays = thread_args->delays;
  pthread_mutex_t *rd_jobs_mutex = thread_args->rd_jobs_mutex;
  pthread_mutex_t *wr_out_mutex = thread_args->wr_out_mutex;
  pthread_rwlock_t *rwlock_events = thread_args->rwlock_events;
  pthread_rwlock_t *rwlock_seats = thread_args->rwlock_seats;

  int exitFlag = 0;
  int *ret_value = (int*) safe_malloc(sizeof(int));

  while (1) {
    unsigned int event_id, delay, thread_id = 0;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    if (delays[id] > 0) {
      printf("%d:  Waiting...\n",id); //TODO
      ems_wait(delays[id]);
      delays[id] = 0;
    }

    // Mutex lock so that only one thread can read from the jobs file at a time.
    pthread_mutex_lock(rd_jobs_mutex);
    switch (get_next(jobs_fd)) {
      case CMD_CREATE:
        printf("%d:  Creating...\n", id); //TODO
        if (parse_create(jobs_fd, &event_id, &num_rows, &num_columns) != 0) {
          pthread_mutex_unlock(rd_jobs_mutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(rd_jobs_mutex);
        if (ems_create(event_id, num_rows, num_columns, rwlock_events)) {
          fprintf(stderr, "Failed to create event\n");
        }
        break;

      case CMD_RESERVE:
        printf("%d:  Reserving...\n",id); //TODO
        num_coords =
            parse_reserve(jobs_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
        pthread_mutex_unlock(rd_jobs_mutex);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        sortReserve(xs, ys, num_coords);

        if (ems_reserve(event_id, num_coords, xs, ys, rwlock_events, rwlock_seats)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }

        break;

      case CMD_SHOW:
        printf("%d:  Showing...\n",id); //TODO
        if (parse_show(jobs_fd, &event_id) != 0) {
          pthread_mutex_unlock(rd_jobs_mutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(rd_jobs_mutex);

        if (ems_show(event_id, out_fd, wr_out_mutex, rwlock_events, rwlock_seats)) {
          fprintf(stderr, "Failed to show event\n");
        }

        break;

      case CMD_LIST_EVENTS:
        printf("%d:  Listing...\n",id); //TODO
        pthread_mutex_unlock(rd_jobs_mutex);

        if (ems_list_events(out_fd, wr_out_mutex)) {
          fprintf(stderr, "Failed to list events\n");
        }

        break;

      case CMD_WAIT:
        printf("%d:  Parsing wait...\n",id); //TODO
        int wait = parse_wait(jobs_fd, &delay, &thread_id);
        pthread_mutex_unlock(rd_jobs_mutex);

        if (wait == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        } else if (wait == 0) {
          for (int i = 0; i < MAX_THREADS; i++) {
            if (i != id) {
              delays[i] += delay;
            }
          }
          ems_wait(delay);
        } else {
          delays[thread_id-1] += delay;
        }

        break;

      case CMD_INVALID:
        pthread_mutex_unlock(rd_jobs_mutex);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        pthread_mutex_unlock(rd_jobs_mutex);
        printf(
          "Available commands:\n"
          "  CREATE <event_id> <num_rows> <num_columns>\n"
          "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
          "  SHOW <event_id>\n"
          "  LIST\n"
          "  WAIT <delay_ms> [thread_id]\n"
          "  BARRIER\n"
          "  HELP\n");

        break;

      case CMD_BARRIER:
        printf("%d:  Barrier...\n",id); //TODO
        pthread_mutex_unlock(rd_jobs_mutex);
        *ret_value = 1;
        exitFlag = 1;

        break;

      case CMD_EMPTY:
        pthread_mutex_unlock(rd_jobs_mutex);
        break;

      case EOC:
        printf("%d:  Reached EOC...\n",id); //TODO
        pthread_mutex_unlock(rd_jobs_mutex);
        *ret_value = 0;
        exitFlag = 1;

        break;
      }

      if (exitFlag == 1) {
        free(thread_args);
        return ret_value;
    }
  }
}

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
  int num_active_proc = 0;

  int MAX_THREADS = atoi(argv[3]);

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
        size_t len_path = strlen(argv[1]) + 1 + strlen(dp->d_name) + 1; // +1 for '/' and +1 for '\0'

        char *jobs_file_path = (char*) safe_malloc(len_path);
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

        char *out_file_path = (char*) safe_malloc(len_path);
        memset(out_file_path, 0, len_path);
        strncpy(out_file_path, jobs_file_path, strlen(jobs_file_path) - 5);
        strcat(out_file_path, ".out");

        int out_fd = open(out_file_path, openFlags, filePerms);

        if (out_fd == -1) {
          fprintf(stderr, "Failed to open .out file\n");
          return 1;
        }

        int threads_id[MAX_THREADS];
        for (int i=0; i<MAX_THREADS; i++) {
          threads_id[i] = i;
        }

        pthread_t threads[MAX_THREADS];

        unsigned int *delays = (unsigned int*) safe_malloc((size_t)MAX_THREADS * sizeof(unsigned int));
        for (int i=0; i<MAX_THREADS; i++) {
          delays[i]=0;
        }

        pthread_mutex_t rd_jobs_mutex;
        pthread_mutex_init(&rd_jobs_mutex, NULL);
        pthread_mutex_t wr_out_mutex;
        pthread_mutex_init(&wr_out_mutex, NULL);
        pthread_rwlock_t rwlock_events;
        pthread_rwlock_init(&rwlock_events, NULL);
        pthread_rwlock_t rwlock_seats;
        pthread_rwlock_init(&rwlock_seats, NULL);

        void *ret_value;
        int exitFlag;

        while (1) {
          exitFlag = 1;

          for (int i = 0; i < MAX_THREADS; i++) {
            struct thread_args *args = (struct thread_args*) safe_malloc(sizeof(struct thread_args));
            args->id = threads_id[i];
            args->jobs_fd = jobs_fd;
            args->out_fd = out_fd;
            args->MAX_THREADS = MAX_THREADS;
            args->delays = delays;
            args->rd_jobs_mutex = &rd_jobs_mutex;
            args->wr_out_mutex= &wr_out_mutex;
            args->rwlock_events = &rwlock_events;
            args->rwlock_seats = &rwlock_seats;

            if (pthread_create(&threads[i], NULL, thread_func, args) != 0) {
              fprintf(stderr, "Failed to create thread\n");
              return 1;
            }
          }

          for (int i = 0; i < MAX_THREADS; i++) {
            if (pthread_join(threads[i], &ret_value) != 0) {
              fprintf(stderr, "Failed to join thread\n");
              free(ret_value);
              return 1;
            } else if (*(int*) ret_value == 1) {
                exitFlag = 0;
            }
            free(ret_value);
          }

          if (exitFlag == 1) {
            break;
          } else {
            cleanup(jobs_fd);
          }
        }

        free(delays);

        if (pthread_mutex_destroy(&rd_jobs_mutex) != 0) {
          fprintf(stderr, "Failed to destroy rd_jobs_mutex\n");
          return 1;
        }

        if (pthread_mutex_destroy(&wr_out_mutex) != 0) {
          fprintf(stderr, "Failed to destroy wr_out_mutex\n");
          return 1;
        }

        if (pthread_rwlock_destroy(&rwlock_events) != 0) {
          fprintf(stderr, "Failed to destroy rwlock_events\n");
          return 1;
        }

        if (pthread_rwlock_destroy(&rwlock_seats) != 0) {
          fprintf(stderr, "Failed to destroy rwlock_seats\n");
          return 1;
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
