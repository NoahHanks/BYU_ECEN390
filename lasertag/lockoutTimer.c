#include <stdbool.h>
#include <stdio.h>
#include "intervalTimer.h"
#include "utils.h"
#include "lockoutTimer.h"

volatile static uint32_t timer;

enum lockOut_st {init_st, timer_running_st} currentState = init_st;

// Calling this starts the timer.
void lockoutTimer_start(){
    timer = 0;
    currentState = timer_running_st;
}

// Perform any necessary inits for the lockout timer.
void lockoutTimer_init(){
	timer = 0;
	currentState = init_st;
}

// Returns true if the timer is running.
bool lockoutTimer_running(){
	return currentState == timer_running_st;
}

// Standard tick function.
void lockoutTimer_tick(){
	//State updates
    switch (currentState){
        case init_st:
			break;
        case timer_running_st:
            if(timer >= LOCKOUT_TIMER_EXPIRE_VALUE){
				currentState = init_st;
			}
    }

	//State actions
    switch (currentState){
        case init_st:
            //do stuff
        case timer_running_st:
            ++timer;
    }
}

// Test function assumes interrupts have been completely enabled and
// lockoutTimer_tick() function is invoked by isr_function().
// Prints out pass/fail status and other info to console.
// Returns true if passes, false otherwise.
// This test uses the interval timer to determine correct delay for
// the interval timer.
bool lockoutTimer_runTest(){
	printf("Starting lockoutTimer_runTest()\n");
	intervalTimer_initAll();
    intervalTimer_resetAll();
	intervalTimer_start(INTERVAL_TIMER_TIMER_2);
    lockoutTimer_start();
    while(lockoutTimer_running()){
			utils_msDelay(1);
    }
	intervalTimer_stop(INTERVAL_TIMER_TIMER_2);
	printf("Time: %f\n", intervalTimer_getTotalDurationInSeconds(INTERVAL_TIMER_TIMER_2));
	printf("Completed lockoutTimer_runTest()\n");
}
