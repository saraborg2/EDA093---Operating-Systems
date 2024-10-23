/*
 * Exercise on thread synchronization.
 *
 * Assume a half-duplex communication bus with limited capacity, measured in
 * tasks, and 2 priority levels:
 *
 * - tasks: A task signifies a unit of data communication over the bus
 *
 * - half-duplex: All tasks using the bus should have the same direction
 *
 * - limited capacity: There can be only 3 tasks using the bus at the same time.
 *                     In other words, the bus has only 3 slots.
 *
 *  - 2 priority levels: Priority tasks take precedence over non-priority tasks
 *
 *  Fill-in your code after the TODO comments
 */

#include <stdio.h>
#include <string.h>

#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "timer.h"

/* This is where the API for the condition variables is defined */
#include "threads/synch.h"

/* This is the API for random number generation.
 * Random numbers are used to simulate a task's transfer duration
 */
#include "lib/random.h"

#define MAX_NUM_OF_TASKS 200

#define BUS_CAPACITY 3

typedef enum {
  SEND,
  RECEIVE,

  NUM_OF_DIRECTIONS
} direction_t;

typedef enum {
  NORMAL,
  PRIORITY,

  NUM_OF_PRIORITIES
} priority_t;

typedef struct {
  direction_t direction;
  priority_t priority;
  unsigned long transfer_duration;
} task_t;


/* Globals*/
static direction_t current_bus_dir; // Bus direction
static int tasks_on_bus; // Number of tasks on bus
static struct lock bus_lock; // Lock for the bus
static struct condition wait_dir_cond[NUM_OF_DIRECTIONS][NUM_OF_PRIORITIES] = {0}; // Conditions for directions and priority pairs
static int waiting_tasks[NUM_OF_DIRECTIONS][NUM_OF_PRIORITIES] = {0}; // Count array for conditions 

void init_bus (void);
void batch_scheduler (unsigned int num_priority_send,
                      unsigned int num_priority_receive,
                      unsigned int num_tasks_send,
                      unsigned int num_tasks_receive);

/* Thread function for running a task: Gets a slot, transfers data and finally
 * releases slot */
static void run_task (void *task_);

/* WARNING: This function may suspend the calling thread, depending on slot
 * availability */
static void get_slot (const task_t *task);

/* Simulates transfering of data */
static void transfer_data (const task_t *task);

/* Releases the slot */
static void release_slot (const task_t *task);

void init_bus (void) {

  random_init ((unsigned int)123456789);

  tasks_on_bus = 0; // No tasks on bus
  current_bus_dir = SEND; // Initally send
  lock_init(&bus_lock); // Init the lock

  //Init all conditions in the wait_dir_cond array:
  for (int dir = 0; dir < NUM_OF_DIRECTIONS; dir++){
    for (int prio = 0; prio < NUM_OF_PRIORITIES; prio++){
      cond_init(&wait_dir_cond[dir][prio]);
    }
  }  
}

void batch_scheduler (unsigned int num_priority_send,
                      unsigned int num_priority_receive,
                      unsigned int num_tasks_send,
                      unsigned int num_tasks_receive) {
  ASSERT (num_tasks_send + num_tasks_receive + num_priority_send +
             num_priority_receive <= MAX_NUM_OF_TASKS);

  static task_t tasks[MAX_NUM_OF_TASKS] = {0};

  char thread_name[32] = {0};

  unsigned long total_transfer_dur = 0;

  int j = 0;

  /* create priority sender threads */
  for (unsigned i = 0; i < num_priority_send; i++) {
    tasks[j].direction = SEND;
    tasks[j].priority = PRIORITY;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "sender-prio");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create priority receiver threads */
  for (unsigned i = 0; i < num_priority_receive; i++) {
    tasks[j].direction = RECEIVE;
    tasks[j].priority = PRIORITY;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "receiver-prio");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create normal sender threads */
  for (unsigned i = 0; i < num_tasks_send; i++) {
    tasks[j].direction = SEND;
    tasks[j].priority = NORMAL;
    tasks[j].transfer_duration = random_ulong () % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "sender");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create normal receiver threads */
  for (unsigned i = 0; i < num_tasks_receive; i++) {
    tasks[j].direction = RECEIVE;
    tasks[j].priority = NORMAL;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "receiver");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* Sleep until all tasks are complete */
  timer_sleep (2 * total_transfer_dur);
}

/* Thread function for the communication tasks */
void run_task(void *task_) {
  task_t *task = (task_t *)task_;

  get_slot (task);

  msg ("%s acquired slot", thread_name());
  transfer_data (task);

  release_slot (task);
}

static direction_t other_direction(direction_t this_direction) {
  return this_direction == SEND ? RECEIVE : SEND;
}

void get_slot (const task_t *task) {
  direction_t current_dir = task->direction;
  priority_t current_prio = task->priority;

  lock_acquire(&bus_lock); // Acquire the lock
  while((tasks_on_bus == BUS_CAPACITY) || (tasks_on_bus > 0 && current_bus_dir == other_direction(current_dir))
  | ((current_prio == NORMAL) && (waiting_tasks[other_direction(current_dir)][PRIORITY]>0))){
    // Block acces if:
    // - Buss full
    // - Tasks in other direction
    // - Task in other direction has Priority
    waiting_tasks[current_dir][current_prio]++; // Inc. waiting tasks in dir, prio
    cond_wait(&wait_dir_cond[current_dir][current_prio], &bus_lock); // Wait on condition
    waiting_tasks[current_dir][current_prio]--; // Dec. waiting tasks in dir, prio
  }
  tasks_on_bus++; // Add task on bus
  current_bus_dir = current_dir; // Change the bus direction to that of the task
  lock_release(&bus_lock); // Relelase the lock
}

void transfer_data (const task_t *task) {
  /* Simulate bus send/receive */
  timer_sleep (task->transfer_duration);
}

void release_slot (const task_t *task) {

  direction_t current_dir = task->direction;
  direction_t other_dir = other_direction(current_dir);

  lock_acquire(&bus_lock); // Acquire the lock
  tasks_on_bus--; // Remove task from the bus
  
  if(waiting_tasks[current_dir][PRIORITY] > 0 ){
    // If there is a task waiting in direction with priority
    cond_signal(&wait_dir_cond[current_dir][PRIORITY],&bus_lock);
  } 
  else if((waiting_tasks[other_dir][PRIORITY] > 0 ) && (tasks_on_bus == 0)){
    // If there is a task waiting in the other direction with priority
    cond_signal(&wait_dir_cond[other_dir][PRIORITY],&bus_lock);
  }
  else if(waiting_tasks[current_dir][NORMAL] > 0 ){
    // If there is a task in current direction without priority
    cond_signal(&wait_dir_cond[current_dir][NORMAL], &bus_lock);
  } 
  else if((waiting_tasks[other_dir][NORMAL] > 0 ) && (tasks_on_bus == 0)){
    // If there is a task waiting in the other direction without priority
    cond_signal(&wait_dir_cond[other_dir][NORMAL], &bus_lock);
  }
  lock_release(&bus_lock); // Release lock
}