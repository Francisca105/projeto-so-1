#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include <stddef.h>

typedef struct thread_struct {
  pthread_t thread;
  unsigned int thread_id;
  struct thread_struct *next;
} thread_struct;

typedef struct thread_wait {
  unsigned int thread_id;
  unsigned int delay;
  struct thread_wait *next;
} thread_wait;

/// Writes the given buffer to the .out file.
/// @param out_fd File descriptor of the .out file.
/// @param buffer Buffer to be coppied to the file.
void write_to_out(int out_fd, char *buffer);

/// Reallocates the given buffer to the given size and copies the given string.
/// @param buffer Buffer to be reallocated.
/// @param size Size of the new buffer.
/// @param str String to be copied to the new buffer.
/// @return the new buffer
char* realloc_and_copy(char *buffer, size_t size, const char *str);

/// Adds a new thread to the given list.
/// @param head Head of the list.
/// @param thread Thread to be added.
/// @param thread_id Id of the thread to be added.
void add_thread(thread_struct *head, pthread_t thread, unsigned int thread_id);

/// Frees the given list of threads.
/// @param head Head of the list.
void free_threads(thread_struct *head);

/// Adds a new thread to the given wait list.
/// @param head Head of the list.
/// @param thread_id Id of the thread to be added.
/// @param delay Delay of the thread to be added.
void add_wait_thread(thread_wait *head, unsigned int thread_id,
                     unsigned int delay);

/// Finds the thread with the given id in the given list.
/// @param head 
/// @param thread_id 
/// @return 
thread_wait *find_wait_thread(thread_wait *head, unsigned int thread_id);


/// Frees the given thread from the given wait list.
/// @param head Head of the list.
/// @param thread_id Id of the thread to be freed.
void free_wait_thread(thread_wait *head, unsigned int thread_id);

/// Frees the given list of wait threads.
/// @param head Head of the list.
void free_wait_threads(thread_wait *head);

/// Initializes the EMS state.
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(unsigned int delay_ms);

/// Destroys the EMS state.
int ems_terminate();

/// Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs,
                size_t *ys);

/// Prints the given event.
/// @param event_id Id of the event to print.
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, int out_fd);

/// Prints all the events.
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events(int out_fd);

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void ems_wait(unsigned int delay_ms);

#endif // EMS_OPERATIONS_H
