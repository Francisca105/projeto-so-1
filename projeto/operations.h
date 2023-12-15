#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include <stddef.h>
#include <pthread.h>

/// Writes the buffer to the .out file.
/// @param out_fd File descriptor of the .out file.
/// @param buffer Buffer to be copied to the file.
/// @return 0 if the buffer was written successfully, 1 otherwise.
int write_to_out(int out_fd, char *buffer);

/// Reallocates the buffer to the given size and copies the string.
/// @param buffer Buffer to be reallocated.
/// @param size Size of the new buffer.
/// @param str String to be copied to the new buffer.
/// @return the new buffer
char* realloc_and_copy(char *buffer, size_t size, const char *str);

/// Compares two seats.
/// @param a First seat.
/// @param b Second seat.
/// @return an integer representing the relative order between the seats.
int compareSeats(const void *a, const void *b);

/// Sorts the given seats in a defined order.
/// @param xs Array of rows of the seats to sort.
/// @param ys Array of columns of the seats to sort.
/// @param num_seats Number of seats to sort.
void sortReserve(size_t *xs, size_t *ys, size_t num_seats);

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
/// @param rwlock_events RWLock to be used to access the events list.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols,
               pthread_rwlock_t *rwlock_events);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @param rwlock_events RWLock to be used to access the events list.
/// @param rwlock_seats RWLock to be used to access an event's seats.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys,
                pthread_rwlock_t *rwlock_events, pthread_rwlock_t *rwlock_seats);

/// Prints the given event.
/// @param event_id Id of the event to print.
/// @param wr_out_mutex Mutex to be used to write to the output file.
/// @param rwlock_events RWLock to be used to access the events list.
/// @param rwlock_seats RWLock to be used to access an event's seats.
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, int out_fd, pthread_mutex_t *wr_out_mutex,
             pthread_rwlock_t *rwlock_events, pthread_rwlock_t *rwlock_seats);

/// Prints all the events.
/// @param wr_out_mutex Mutex to be used to write to the output file.
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events(int out_fd, pthread_mutex_t *wr_out_mutex);

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void ems_wait(unsigned int delay_ms);

#endif // EMS_OPERATIONS_H
