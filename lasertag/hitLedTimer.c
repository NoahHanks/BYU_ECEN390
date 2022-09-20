#include <stdbool.h>
#include <stdio.h>
#include "utils.h"
#include "leds.h"
#include "mio.h"
#include "hitLedTimer.h"
#include "buttons.h"

// The lockouttimer_HLT is active for 1/2 second once it is started.
// It is used to lock-out the detector once a hit has been detected.
// This ensure that only one hit is detected per 1/2-second interval.

#define HIT_LED_TIMER_EXPIRE_VALUE 50000 // Defined in terms of 100 kHz ticks.
#define HIT_LED_TIMER_OUTPUT_PIN 11      // JF-3
#define LED_HIGH_VALUE 1
#define LED_LOW_VALUE 0
#define TEST_DELAY 500

static uint32_t timer_HLT;
volatile static bool enabled = false;
enum hitLed_st {init_st, timer_running_st, disabled_st} currentState_HLT = init_st;

// Calling this starts the timer.
void hitLedTimer_start(){
	if(enabled){
		timer_HLT = 0;
		currentState_HLT = timer_running_st;
	}
}

// Returns true if the timer_HLT is currently running.
bool hitLedTimer_running(){
	return currentState_HLT == timer_running_st;
}

// Need to init things.
void hitLedTimer_init(){
	leds_init(false);
    mio_init(false);
	mio_setPinAsOutput(HIT_LED_TIMER_OUTPUT_PIN);
	timer_HLT = 0;
}

// Turns the gun's hit-LED on.
void hitLedTimer_turnLedOn(){
	mio_writePin(HIT_LED_TIMER_OUTPUT_PIN, LED_HIGH_VALUE);
}

// Turns the gun's hit-LED off.
void hitLedTimer_turnLedOff(){
    mio_writePin(HIT_LED_TIMER_OUTPUT_PIN, LED_LOW_VALUE);
}

// Disables the hitLedTimer.
void hitLedTimer_disable(){
    enabled = false;
}

// Enables the hitLedTimer.
void hitLedTimer_enable(){
    enabled = true;
}

// Standard tick function.
void hitLedTimer_tick(){
	//State updates
    switch (currentState_HLT){
        case init_st:
            break;
        case timer_running_st:
		
            if(timer_HLT >= HIT_LED_TIMER_EXPIRE_VALUE){
				currentState_HLT = init_st;
			}
			break;
        case disabled_st:
            if(enabled)
                currentState_HLT = init_st;
    }

	//State actions
    switch (currentState_HLT){
        case init_st:
            hitLedTimer_turnLedOff();
            leds_write(0);
			break;
        case timer_running_st:
            hitLedTimer_turnLedOn();
            leds_write(1);
            ++timer_HLT;
			break;
        case disabled_st:
            hitLedTimer_turnLedOff();
            leds_write(0);           
    }
}

// Runs a visual test of the hit LED.
// The test continuously blinks the hit-led on and off.
void hitLedTimer_runTest(){
    printf("Starting hitLedTimer_runTest()\n");
	//Continuously start and stop the state machine
	buttons_init();
	//Repeat until BTN1 is pressed
	while(!(buttons_read() & BUTTONS_BTN1_MASK)){
		hitLedTimer_enable();
		hitLedTimer_start();
		while(hitLedTimer_running()){
			utils_msDelay(1);
		}
		hitLedTimer_disable();
		utils_msDelay(TEST_DELAY);
	}
	printf("Completed hitLedTimer_runTest()\n");
}