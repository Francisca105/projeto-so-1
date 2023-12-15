#include "operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "eventlist.h"

#define MAX_UINT 4294967295

static struct EventList *event_list = NULL;
static unsigned int state_access_delay_ms = 0;

/* Auxiliary functions */

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
/// @param rwlock_events TODO
/// @return Pointer to the event if found, NULL otherwise.
static struct Event *get_event_with_delay(unsigned int event_id, pthread_rwlock_t *rwlock_events) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return get_event(event_list, event_id, rwlock_events);
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

  if (get_event_with_delay(event_id, rwlock_events) != NULL) {
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

  if (append_to_list(event_list, event, rwlock_events) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs,
                size_t *ys, pthread_rwlock_t *rwlock_events, pthread_rwlock_t *rwlock_seats) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event *event = get_event_with_delay(event_id, rwlock_events);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  // Read lock so that multiple threads can try to reserve seats at the same time.
  pthread_rwlock_rdlock(rwlock_seats);
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    pthread_rwlock_unlock(rwlock_seats);
    return 1;
  }
  pthread_rwlock_unlock(rwlock_seats);

  return 0;
}

int ems_show(unsigned int event_id, int out_fd, pthread_mutex_t *wr_out_mutex,
             pthread_rwlock_t *rwlock_events, pthread_rwlock_t *rwlock_seats) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event *event = get_event_with_delay(event_id, rwlock_events);

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

  // Write lock so that no other thread can modify the event while it is being read.
  // Before writing to the output file, the output is copied to a buffer so that the
  // output file does not have to be locked for the entire duration of the function.
  pthread_rwlock_wrlock(rwlock_seats);
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int *seat = get_seat_with_delay(event, seat_index(event, i, j));

      char seat_as_char[sizeof(MAX_UINT) + 1];  // max size of uint + '\0'
      sprintf(seat_as_char, "%u", *seat);
      strcat(buffer, seat_as_char);

      if (j < event->cols) {
        strcat(buffer, " ");
      }
    }
    strcat(buffer, "\n");
  }
  pthread_rwlock_unlock(rwlock_seats);

  // Mutex lock so that no other thread can write to the output file while it is being written to.
  pthread_mutex_lock(wr_out_mutex);
  if (write_to_out(out_fd, buffer) != 0) {
    pthread_mutex_unlock(wr_out_mutex);
    free(buffer);
    return 1;
  };
  pthread_mutex_unlock(wr_out_mutex);

  free(buffer);

  return 0;
}

int ems_list_events(int out_fd, pthread_mutex_t *wr_out_mutex) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  // Mutex lock so that no other thread can write to the output file while it is being written to.
  pthread_mutex_lock(wr_out_mutex);
  if (event_list->head == NULL) {
    if (write_to_out(out_fd, "No events\n") != 0) {
      pthread_mutex_unlock(wr_out_mutex);
      return 1;
    }

    pthread_mutex_unlock(wr_out_mutex);
    return 0;
  }
  pthread_mutex_unlock(wr_out_mutex);

  char *buffer = (char*) malloc(1);
  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for buffer\n");
    return 1;
  }

  // Write lock so that no other thread can write to the output file while it is being written to.
  // In this case, it is not efficient to copy the output to a buffer before writing to the output 
  // file because the number of events is not known beforehand.
  pthread_mutex_lock(wr_out_mutex);
  struct ListNode *current = event_list->head;
  while (current != NULL) {
    buffer = realloc_and_copy(buffer, sizeof("Event: "), "Event: ");
    if (buffer == NULL) {
      pthread_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }
    
    if (write_to_out(out_fd, buffer) != 0) {
      pthread_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }

    char event_id_as_char[sizeof(MAX_UINT) + 2];  // max size of uint + \n + \0
    sprintf(event_id_as_char, "%u\n", current->event->id);
    buffer = realloc_and_copy(buffer, strlen(event_id_as_char) + 1, event_id_as_char);
    if (buffer == NULL) {
      pthread_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }

    if (write_to_out(out_fd, buffer) != 0) {
      pthread_mutex_unlock(wr_out_mutex);
      free(buffer);
      return 1;
    }

    current = current->next;
  }
  pthread_mutex_unlock(wr_out_mutex);

  free(buffer);

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
