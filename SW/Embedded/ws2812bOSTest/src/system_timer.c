#include "system_timer.h"

#define CLOCKS_PER_MILLI 50000

volatile uint32_t *sys_timer = (uint32_t*)SYS_TIMER;

// Gets the current time since startup, in milliseconds
uint32_t system_get_time_ms()
{
	// Write to register to take a snapshot
	*sys_timer = 0;

	// Return snapshot value
	return (*sys_timer / CLOCKS_PER_MILLI);
}

// Not yet implemented
uint32_t realtime_get_time_seconds()
{
	return 0;
}

void system_delay_ms(uint32_t ms)
{
	uint32_t start_time = system_get_time_ms();
	while (system_get_time_ms() - start_time < ms);
}
