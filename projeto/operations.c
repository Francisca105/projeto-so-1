#include "operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "eventlist.h"
#include "parser.h"
#include "constants.h"

#define MAX_UINT 4294967295

static struct EventList *event_list = NULL;
static unsigned int state_access_delay_ms = 0;
int barrier = 0;

/* Main thread function */
void *thread_func(void *args) {
  struct thread_args *thread_args = (struct thread_args*) args;
  int id = thread_args->id;
  int jobs_fd = thread_args->jobs_fd;
  int out_fd = thread_args->out_fd;
  int MAX_THREADS = thread_args->MAX_THREADS;
  unsigned int *delays = thread_args->delays;
  pthread_mutex_t *rd_jobs_mutex = thread_args->rd_jobs_mutex;
  pthread_mutex_t *wr_out_mutex = thread_args->wr_out_mutex;
  pthread_mutex_t *reservation = thread_args->reservation;
  pthread_rwlock_t *rwlock_events = thread_args->rwlock_events;

  int exitFlag = 0;
  int *ret_value = (int*) safe_malloc(sizeof(int));

  while (1) {
    unsigned int event_id, delay, thread_id = 0;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    if (delays[id] > 0) {
      ems_wait(delays[id]);
      delays[id] = 0;
    }

    barrier++;

    // Mutex lock so that only one thread can read from the jobs file at a time.
    safe_mutex_lock(rd_jobs_mutex);
    switch (get_next(jobs_fd)) {
      case CMD_CREATE:
        if (parse_create(jobs_fd, &event_id, &num_rows, &num_columns) != 0) {
          safe_mutex_unlock(rd_jobs_mutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        safe_mutex_unlock(rd_jobs_mutex);
        if (ems_create(event_id, num_rows, num_columns, rwlock_events)) {
          fprintf(stderr, "Failed to create event\n");
        }
        break;

      case CMD_RESERVE:
        num_coords =
            parse_reserve(jobs_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
        safe_mutex_unlock(rd_jobs_mutex);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        sortReserve(xs, ys, num_coords);

        if (ems_reserve(event_id, num_coords, xs, ys, reservation)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }
        break;

      case CMD_SHOW:
        if (parse_show(jobs_fd, &event_id) != 0) {
          safe_mutex_unlock(rd_jobs_mutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        safe_mutex_unlock(rd_jobs_mutex);

        if (ems_show(event_id, out_fd, wr_out_mutex)) {
          fprintf(stderr, "Failed to show event\n");
        }
        break;

      case CMD_LIST_EVENTS:
        safe_mutex_unlock(rd_jobs_mutex);

        if (ems_list_events(out_fd, wr_out_mutex)) {
          fprintf(stderr, "Failed to list events\n");
        }
        break;

      case CMD_WAIT:
        int wait = parse_wait(jobs_fd, &delay, &thread_id);
        safe_mutex_unlock(rd_jobs_mutex);

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
        safe_mutex_unlock(rd_jobs_mutex);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        safe_mutex_unlock(rd_jobs_mutex);
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
        safe_mutex_unlock(rd_jobs_mutex);
        *ret_value = 1;
        exitFlag = 1;
        fprintf(stderr, "Reached barrier - Completed %d commands.\n", barrier);
        barrier = 0;

        break;

      case CMD_EMPTY:
        safe_mutex_unlock(rd_jobs_mutex);
        break;

      case EOC:
        safe_mutex_unlock(rd_jobs_mutex);
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

/* Auxiliary functions */
void *safe_malloc(size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    exit(1);
  }
  return ptr;
}

void safe_mutex_init(pthread_mutex_t *mutex) {
  if (pthread_mutex_init(mutex, NULL) != 0) {
    fprintf(stderr, "Failed to init mutex\n");
    exit(EXIT_FAILURE);
  }
}

void safe_mutex_lock(pthread_mutex_t *mutex) {
  if (pthread_mutex_lock(mutex) != 0) {
    fprintf(stderr, "Failed to lock mutex\n");
    exit(EXIT_FAILURE);
  }
}

void safe_mutex_unlock(pthread_mutex_t *mutex) {
  if (pthread_mutex_unlock(mutex) != 0) {
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(EXIT_FAILURE);
  }
}

void safe_mutex_destroy(pthread_mutex_t *mutex) {
  if (pthread_mutex_destroy(mutex) != 0) {
      fprintf(stderr, "Failed to destroy mutex\n");
      exit(EXIT_FAILURE);
  }
}

void safe_rwlock_init(pthread_rwlock_t *rwl) {
  if (pthread_rwlock_init(rwl, NULL) != 0) {
    fprintf(stderr, "Failed to init rwlock\n");
    exit(EXIT_FAILURE);
  }
}

void safe_rwlock_wrlock(pthread_rwlock_t *rwl) {
  if (pthread_rwlock_wrlock(rwl) != 0) {
    fprintf(stderr, "Failed to lock rw_wrlock\n");
    exit(EXIT_FAILURE);
  }
}

void safe_rwlock_rdlock(pthread_rwlock_t *rwl) {
  if (pthread_rwlock_rdlock(rwl) != 0) {
      fprintf(stderr, "Failed to lock rw_rdlock\n");
      exit(EXIT_FAILURE);
  }
}

void safe_rwlock_unlock(pthread_rwlock_t *rwl) {
  if (pthread_rwlock_unlock(rwl) != 0) {
    fprintf(stderr, "Failed to unlock rwlock\n");
    exit(EXIT_FAILURE);
  }
}

void safe_rwlock_destroy(pthread_rwlock_t *rwl) {
  if (pthread_rwlock_destroy(rwl) != 0) {
    fprintf(stderr, "Failed to destroy rwlock\n");
    exit(EXIT_FAILURE);
  }
}

int write_to_out(int out_fd, char *buffer) {
  ssize_t numWritten = 0;
  size_t done = 0, len = strlen(buffer);

  while ((numWritten = write(out_fd, buffer+done, len)) > 0) {
    done += (size_t)numWritten;
    len = len - (size_t)numWritten;
  }

  if (numWritten == -1) {
      fprintf(stderr, "Failed to write to .out file\n");
      return 1;
  }

  return 0;
}

char* realloc_and_copy(char *buffer, size_t size, const char *str) {
  buffer = (char*) realloc(buffer, size);
  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for buffer\n");
    return NULL;
  }

  strcpy(buffer, str);
  return buffer;
}

int compareSeats(const void *a, const void *b) {
  size_t *x = (size_t *)a;
  size_t *y = (size_t *)b;

  if (x[0] < y[0]) {
    return -1;
  } else if (x[0] > y[0]) {
    return 1;
  } else {
    if (x[1] < y[1]) {
      return -1;
    } else if (x[1] > y[1]) {
      return 1;
    } else {
      return 0;
    }
  }
}

void sortReserve(size_t *xs, size_t *ys, size_t num_seats) {
  size_t pairs[num_seats][2];

  for (size_t i = 0; i < num_seats; i++) {
    pairs[i][0] = xs[i];
    pairs[i][1] = ys[i];
  }

  qsort(pairs, num_seats, sizeof(pairs[0]), compareSeats);

  for (size_t i = 0; i < num_seats; i++) {
    xs[i] = pairs[i][0];
    ys[i] = pairs[i][1];
  }
}


/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event *get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int *get_seat_with_delay(struct Event *event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event *event, size_t row, size_t col) {
  return (row - 1) * event->cols + col - 1;
}

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols,
               pthread_rwlock_t *rwlock_events) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    return 1;
  }

  struct Event *event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  event->locks = malloc(num_rows * num_cols * sizeof(pthread_rwlock_t));

  if (event->locks == NULL) {
    fprintf(stderr, "Error allocating memory for event locks\n");
    free(event->data);
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    safe_rwlock_init(&event->locks[i]);
  }

  if (append_to_list(event_list, event, rwlock_events) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    for (size_t i = 0; i < num_rows * num_cols; i++) {
      safe_rwlock_destroy(&event->locks[i]);
    }
    free(event->locks);
    free(event);
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys,
                pthread_mutex_t *reservation) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event *event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  safe_mutex_lock(reservation);
  unsigned int reservation_id = ++event->reservations;
  safe_mutex_unlock(reservation);

  size_t i = 0;
  pthread_rwlock_t *locks = event->locks;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    // Each seat is write-locked during the reservation to ensure that no other thread
    // can reserve the same seat. It also ensures that no other thread can show the event
    // while it is being reserved.
    safe_rwlock_wrlock(&locks[seat_index(event, row, col)]);

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      safe_rwlock_unlock(&locks[seat_index(event, row, col)]);
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    safe_mutex_lock(reservation);
    event->reservations--;
    safe_mutex_unlock(reservation);
    
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
      safe_rwlock_unlock(&locks[seat_index(event, xs[j], ys[j])]);
    }
    return 1;
  }

  for (size_t j = 0; j < num_seats; j++) {
    safe_rwlock_unlock(&locks[seat_index(event, xs[j], ys[j])]);
  }

  return 0;
}

int ems_show(unsigned int event_id, int out_fd, pthread_mutex_t *wr_out_mutex) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event *event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  char *buffer = (char*) malloc((sizeof(MAX_UINT)+1) * event->cols * event->rows);

  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for buffer\n");
    return 1;
  }
  buffer[0] = '\0';

  pthread_rwlock_t *locks = event->locks;

  // The output is copied to a buffer before writing to the output to reduce the time
  // that the output file is locked.
  // Each seat is read-locked during the copying to ensure better concurrency.
  
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      safe_rwlock_rdlock(&locks[seat_index(event, i, j)]);
      unsigned int *seat = get_seat_with_delay(event, seat_index(event, i, j));
      safe_rwlock_unlock(&locks[seat_index(event, i, j)]);

      char seat_as_char[sizeof(MAX_UINT) + 1];  // max size of uint + '\0'
      sprintf(seat_as_char, "%u", *seat);
      strcat(buffer, seat_as_char);

      if (j < event->cols) {
        strcat(buffer, " ");
      }
    }
    strcat(buffer, "\n");
  }

  // Mutex lock so that no other thread can write to the output file while it is being written to.
  safe_mutex_lock(wr_out_mutex);
  if (write_to_out(out_fd, buffer) != 0) {
    safe_mutex_unlock(wr_out_mutex);
    free(buffer);
    return 1;
  };
  safe_mutex_unlock(wr_out_mutex);

  free(buffer);

  return 0;
}

int ems_list_events(int out_fd, pthread_mutex_t *wr_out_mutex) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  // Mutex lock so that no other thread can write to the output file while it is being written to.
  safe_mutex_lock(wr_out_mutex);
  if (event_list->head == NULL) {
    if (write_to_out(out_fd, "No events\n") != 0) {
      safe_mutex_unlock(wr_out_mutex);
      return 1;
    }

    safe_mutex_unlock(wr_out_mutex);
    return 0;
  }
  safe_mutex_unlock(wr_out_mutex);

  char *buffer = (char*) malloc(1);
  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for buffer\n");
    return 1;
  }

  // Write lock so that no other thread can write to the output file while it is being written to.
  // In this case, it is not efficient to copy the output to a buffer before writing to the output 
  // file because the number of events is not known beforehand.
  safe_mutex_lock(wr_out_mutex);
  struct ListNode *current = event_list->head;
  while (current != NULL) {
    buffer = realloc_and_copy(buffer, sizeof("Event: "), "Event: ");
    if (buffer == NULL) {
      safe_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }
    
    if (write_to_out(out_fd, buffer) != 0) {
      safe_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }

    char event_id_as_char[sizeof(MAX_UINT) + 2];  // max size of uint + \n + \0
    sprintf(event_id_as_char, "%u\n", current->event->id);
    buffer = realloc_and_copy(buffer, strlen(event_id_as_char) + 1, event_id_as_char);
    if (buffer == NULL) {
      safe_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }

    if (write_to_out(out_fd, buffer) != 0) {
      safe_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }

    current = current->next;
  }
  safe_mutex_unlock(wr_out_mutex);

  free(buffer);

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
