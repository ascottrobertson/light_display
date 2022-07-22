#include "task_scheduler_if.h"
#include "system_timer.h"

#include <string.h>
#include <stdlib.h>

#define MAX_TASKS 64

task_entry_t current_tasks[MAX_TASKS];

task_entry_t* task_alloc(); // Allocates a new task.  Returns null if task could not be allocated
void task_free(task_entry_t* task); // Frees the task.

uint32_t task_index;

void scheduler_init()
{
	// Reset task index
	task_index = 1;

	// Clear task list
	memset(current_tasks, 0, sizeof(current_tasks));
}

uint32_t schedule_interval(task_status_t (*task)(uint32_t), uint32_t milliseconds, bool execute_now)
{
	// Verify task pointer is not null
	if (task == NULL)
	{
		return 0;
	}
	
	// Verify task interval time is non-zero
	if (milliseconds == 0)
	{
		return 0;
	}
	
	// Allocate new task entry
	task_entry_t *new_task = task_alloc();
	
	if (new_task == NULL)
	{
		return 0;
	}
	
	// Initialize task entry
	new_task->task_id = task_index++;
	new_task->type = TASK_TYPE_INTERVAL;
	new_task->time = milliseconds;
	new_task->start_time = system_get_time_ms();
	new_task->last_exec_time = new_task->start_time;
	new_task->current_status = TASK_STATUS_GOOD;
	new_task->task = task;
	
	// If task should be executed immediately, execute.
	if (execute_now)
	{
		new_task->current_status = new_task->task(new_task->task_id);
	}

	return new_task->task_id;
}

uint32_t schedule_timeout(task_status_t (*task)(uint32_t), uint32_t milliseconds)
{
	// Verify task pointer is not null
	if (task == NULL)
	{
		return 0;
	}
	
	// Verify task timeout time is non-zero
	if (milliseconds == 0)
	{
		return 0;
	}
	
	// Allocate new task entry
	task_entry_t *new_task = task_alloc();
	
	if (new_task == NULL)
	{
		return 0;
	}
	
	// Initialize task entry
	new_task->task_id = task_index++;
	new_task->type = TASK_TYPE_TIMEOUT;
	new_task->time = milliseconds;
	new_task->last_exec_time = 0;
	new_task->start_time = system_get_time_ms();
	new_task->current_status = TASK_STATUS_GOOD;
	new_task->task = task;

	return new_task->task_id;
}

uint32_t schedule_realtime(task_status_t (*task)(uint32_t), uint32_t timestamp)
{
	// Verify task pointer is not null
	if (task == NULL)
	{
		return 0;
	}
	
	// Allocate new task entry
	task_entry_t *new_task = task_alloc();
	
	if (new_task == NULL)
	{
		return 0;
	}
	
	// Initialize task entry
	new_task->task_id = task_index++;
	new_task->type = TASK_TYPE_REALTIME;
	new_task->time = timestamp;
	new_task->last_exec_time = 0;
	new_task->start_time = system_get_time_ms();
	new_task->current_status = TASK_STATUS_GOOD;
	new_task->task = task;

	return new_task->task_id;
}

void remove_task(uint32_t task_id)
{
	int t = 0;
	task_entry_t *task;
	for (t = 0; t < MAX_TASKS; t++)
	{
		task = &current_tasks[t];
		if (task->task_id == task_id)
		{
			task_free(task);
			return;
		}
	}
}

// Process tasks in progress
void process_tasks()
{
	uint32_t current_system_time = system_get_time_ms();
	uint32_t current_realtime = realtime_get_time_seconds();
	
	int t = 0;
	task_entry_t *task;
	for (t = 0; t < MAX_TASKS; t++)
	{
		task = &current_tasks[t];
		
		// Skip if no task in this entry
		if (task->task_id == 0)
		{
			continue;
		}
		
		switch (task->type)
		{
			case TASK_TYPE_INTERVAL:
			{
				// Check if time since last execution is >= interval time
				if (current_system_time - task->last_exec_time >= task->time)
				{
					task->last_exec_time = current_system_time;
					task->current_status = task->task(task->task_id);
				}
				break;
			}
			case TASK_TYPE_TIMEOUT:
			{
				// Check if time since creation is >= timeout time
				if (current_system_time - task->start_time >= task->time)
				{
					task->current_status = task->task(task->task_id);
					remove_task(task->task_id);
				}
				break;
			}
			case TASK_TYPE_REALTIME:
			{
				// Check if current realtime is >= task execution time
				if (current_realtime >= task->time)
				{
					task->current_status = task->task(task->task_id);
					remove_task(task->task_id);
				}
				break;
			}
		}
	}
}

// Finds the first task entry with a task id of 0 and returns a pointer
// Returns null if task list is full
task_entry_t* task_alloc()
{
	int t = 0;
	task_entry_t *task;
	for (t = 0; t < MAX_TASKS; t++)
	{
		task = &current_tasks[t];
		if (task->task_id == 0)
		{
			return task;
		}
	}
	
	return NULL;
}

// Mark task entry as free
void task_free(task_entry_t* task)
{
	task->task_id = 0;
}
