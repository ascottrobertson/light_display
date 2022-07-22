/*
 * led_task.c
 *
 *  Created on: Oct 31, 2020
 *      Author: aRobe
 */
#include "system_memory_map.h"
#include "led_task.h"

int led_index = 0;
volatile uint8_t *ledg = (uint8_t*)LEDG;

task_status_t led_task(uint32_t task_id)
{
	*ledg = led_index++;

	if (led_index > 255)
	{
		led_index = 0;
	}

	return TASK_STATUS_GOOD;
}
