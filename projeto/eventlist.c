#include "eventlist.h"
#include "operations.h"

#include <stdlib.h>
#include <pthread.h>

struct EventList *create_list() {
  struct EventList *list = (struct EventList *)malloc(sizeof(struct EventList));
  if (!list)
    return NULL;
  list->head = NULL;
  list->tail = NULL;
  return list;
}

int append_to_list(struct EventList *list, struct Event *event, pthread_rwlock_t *rwlock_events) {
  if (!list)
    return 1;

  struct ListNode *new_node =
      (struct ListNode *)malloc(sizeof(struct ListNode));
  if (!new_node)
    return 1;

  new_node->event = event;
  new_node->next = NULL;

  // Write lock so that no other thread can modify the list concurrently.
  safe_rwlock_wrlock(rwlock_events);
  if (list->head == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    list->tail = new_node;
  }
  safe_rwlock_unlock(rwlock_events);

  return 0;
}

static void free_event(struct Event *event) {
  if (!event)
    return;

  free(event->data);
  free(event);
}

void free_list(struct EventList *list) {
  if (!list)
    return;

  struct ListNode *current = list->head;
  while (current) {
    struct ListNode *temp = current;
    current = current->next;

    free_event(temp->event);
    free(temp);
  }

  free(list);
}

struct Event *get_event(struct EventList *list, unsigned int event_id) {
  if (!list)
    return NULL;

  // Read lock so that multiple threads can read from the list at the same time.
  struct ListNode *current = list->head;
  while (current) {
    struct Event *event = current->event;
    if (event->id == event_id) {
      return event;
    }
    current = current->next;
  }

  return NULL;
}
