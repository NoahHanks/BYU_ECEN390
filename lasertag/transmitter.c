#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mio.h"
#include "utils.h"
#include "buttons.h"
#include "switches.h"
#include "transmitter.h"
#include "filter.h"
#include "interrupts.h"

#define TRANSMITTER_ON_OFF_DURATION 20000
#define TRANSMITTER_OUTPUT_PIN 13
#define TRANSMITTER_HIGH_VALUE 1
#define TRANSMITTER_LOW_VALUE 0

volatile static bool continuousMode = false;
volatile static bool startRunning = false;
volatile static bool testMode = false;
static uint16_t transmitterFrequencyNum;
static uint32_t counter;
static uint32_t timeCounter;

// The transmitter state machine generates a square wave output at the chosen
// frequency as set by transmitter_setFrequencyNumber(). The step counts for the
// frequencies are provided in filter.h

enum transmitter_st {init_st, off_st, high_st, low_st} currentState_trans = init_st;

// Standard init function.
void transmitter_init(){  
    currentState_trans = init_st;
	counter = 0;
    timeCounter = 0;
	startRunning = false;
	continuousMode = false;
    mio_init(false);  // false disables any debug printing if there is a system failure during init.
    mio_setPinAsOutput(TRANSMITTER_OUTPUT_PIN);  // Configure the signal direction of the pin to be an output.
}

// Write a one to the JF1 pin.
void transmitter_set_jf1_to_one() {
	mio_writePin(TRANSMITTER_OUTPUT_PIN, TRANSMITTER_HIGH_VALUE); // Write a '1' to JF-1.
}

// Write a zero to the JF1 pin.
void transmitter_set_jf1_to_zero() {
	mio_writePin(TRANSMITTER_OUTPUT_PIN, TRANSMITTER_LOW_VALUE); // Write a '0' to JF-1.
}

// Starts the transmitter.
void transmitter_run(){ 
	startRunning = true;
}

// Returns true if the transmitter is still running.
bool transmitter_running(){  
    return (currentState_trans == high_st || currentState_trans == low_st || startRunning);
}

// Sets the frequency number. If this function is called while the
// transmitter is running, the frequency will not be updated until the
// transmitter stops and transmitter_run() is called again.
void transmitter_setFrequencyNumber(uint16_t frequencyNumber){ 
	if(continuousMode || currentState_trans == init_st || currentState_trans == off_st){
    	transmitterFrequencyNum = frequencyNumber;
	}
}

// Returns the current frequency setting.
uint16_t transmitter_getFrequencyNumber(){ 
    return transmitterFrequencyNum;
}

// Standard tick function.
void transmitter_tick(){ 
    //State updates
    switch (currentState_trans){
        case init_st:
			currentState_trans = off_st;
			transmitter_set_jf1_to_zero();
			if(testMode){
				printf("Waiting to start");
			}
            break;
        case off_st:
			if(startRunning){	//Initialize everything and transition to high_st
				currentState_trans = high_st;
				startRunning = false;
				counter = 0;
                timeCounter = 0;
				if(testMode){
					printf("\nhigh_st\n");
				}
				transmitter_set_jf1_to_one();
			}
			break;
        case high_st:
			if(timeCounter > TRANSMITTER_ON_OFF_DURATION && !continuousMode){	//If we are in continuous mode and 200 ms has elapsed, turn state machine off
				currentState_trans = off_st;
				transmitter_set_jf1_to_zero();
				if(testMode){
					printf("Waiting to start");
				}
			}
            if(counter >= filter_frequencyTickTable[transmitterFrequencyNum] / 2){	//If half the cycle has elapsed, transistion to low
                counter = 0;
				currentState_trans = low_st;
				if(testMode){
					printf("\nlow_st\n");
				}
				transmitter_set_jf1_to_zero();
            }
            break;
		case low_st:
			if(timeCounter > TRANSMITTER_ON_OFF_DURATION && !continuousMode){	//If we are in continuous mode and 200 ms has elapsed, turn state machine off
				currentState_trans = off_st;
				if(testMode){
					printf("Waiting to start");
				}
			}
            if(counter >= filter_frequencyTickTable[transmitterFrequencyNum] / 2){	//If half the cycle has elapsed, transistion to high
                counter = 0;
				currentState_trans = high_st;
				if(testMode){
					printf("\nhigh_st\n");
				}
				transmitter_set_jf1_to_one();
            }
			break;         
    }

	//State actions
	switch (currentState_trans){
        case init_st:
            break;
        case off_st:
			break;
        case high_st:
            counter++;
            timeCounter++;
			if(testMode){
				printf("1");
			}
            break;
		case low_st:
            counter++;
            timeCounter++;
			if(testMode){
				printf("0");
			}
			break;         
    }
}

// Prints out the clock waveform to stdio. Terminates when BTN1 is pressed.
// Prints out one line of 1s and 0s that represent one period of the clock signal, in terms of ticks.
#define TRANSMITTER_TEST_TICK_PERIOD_IN_MS 10
#define DELAY_PERIOD_FOR_SCOPE 400
#define BOUNCE_DELAY 5
void transmitter_runTest() {
	interrupts_disableTimerGlobalInts();
  	printf("starting transmitter_runTest()\n");
  	buttons_init();                                         // Using buttons
  	switches_init();                                        // and switches.
  	transmitter_init();                                     // init the transmitter.
  	while (!(buttons_read() & BUTTONS_BTN1_MASK)) {         // Run continuously until BTN1 is pressed.
    	uint16_t switchValue = switches_read() % FILTER_FREQUENCY_COUNT;  // Compute a safe number from the switches.
    	transmitter_setFrequencyNumber(switchValue);          // set the frequency number based upon switch value.
    	transmitter_run();                                    // Start the transmitter.
    	while (transmitter_running()) {                       // Keep ticking until it is done.
      		transmitter_tick();                                 // tick.
      		utils_msDelay(TRANSMITTER_TEST_TICK_PERIOD_IN_MS);  // short delay between ticks.
    	}
    	printf("completed one test period.\n");
        utils_msDelay(DELAY_PERIOD_FOR_SCOPE);
  	}
  	do {utils_msDelay(BOUNCE_DELAY);} while (buttons_read());
  	printf("exiting transmitter_runTest()\n");
	interrupts_enableTimerGlobalInts();
}

// Runs the transmitter continuously.
// if continuousModeFlag == true, transmitter runs continuously, otherwise,
// transmits one pulse-width and stops. To set continuous mode, you must invoke
// this function prior to calling transmitter_run(). If the transmitter is in
// currently in continuous mode, it will stop running if this function is
// invoked with continuousModeFlag == false. It can stop immediately or wait
// until the last 200 ms pulse is complete. NOTE: while running continuously,
// the transmitter will change frequencies at the end of each 200 ms pulse.
void transmitter_setContinuousMode(bool continuousModeFlag){ 
    continuousMode = continuousModeFlag;
}

// Tests the transmitter in non-continuous mode.
// The test runs until BTN1 is pressed.
// To perform the test, connect the oscilloscope probe
// to the transmitter and ground probes on the development board
// prior to running this test. You should see about a 300 ms dead
// spot between 200 ms pulses.
// Should change frequency in response to the slide switches.
void transmitter_runNoncontinuousTest(){ 
    printf("starting transmitter_runNoncontinuousTest()\n");
  	buttons_init();                                         // Using buttonswhile (transmitter_running())
  	switches_init();                                        // and switches.
  	transmitter_init();                                     // init the transmitter.
	transmitter_setContinuousMode(false);
  	while (!(buttons_read() & BUTTONS_BTN1_MASK)) {         // Run continuously until BTN1 is pressed.
		while (!(buttons_read() & BUTTONS_BTN0_MASK)){
			if(buttons_read() & BUTTONS_BTN1_MASK)
                break;
		}
        if(buttons_read() & BUTTONS_BTN1_MASK)
                break;
    	uint16_t switchValue = switches_read() % FILTER_FREQUENCY_COUNT;  // Compute a safe number from the switches.
    	transmitter_setFrequencyNumber(switchValue);          // set the frequency number based upon switch value.
    	transmitter_run();                                    // Start the transmitter.
    	while (transmitter_running()) {                       // Keep ticking until it is done.
    	}
		do {utils_msDelay(BOUNCE_DELAY);} while (buttons_read());
  	}
  	do {utils_msDelay(BOUNCE_DELAY);} while (buttons_read());
  	printf("Completed transmitter_runNoncontinuousTest()\n");
}

// Tests the transmitter in continuous mode.
// To perform the test, connect the oscilloscope probe
// to the transmitter and ground probes on the development board
// prior to running this test.
// Transmitter should continuously generate the proper waveform
// at the transmitter-probe pin and change frequencies
// in response to changes to the changes in the slide switches.
// Test runs until BTN1 is pressed.
void transmitter_runContinuousTest(){ 
	printf("Starting runContinuousTest()\n");
    transmitter_init();
	transmitter_run();
	transmitter_setContinuousMode(true);
	while(!(buttons_read() & BUTTONS_BTN1_MASK)){	//Continously run transmitter and update frequency
		transmitter_run();
		transmitter_setFrequencyNumber(switches_read() % FILTER_FREQUENCY_COUNT);
	}
	do {utils_msDelay(BOUNCE_DELAY);} while (buttons_read());
	printf("Completed runContinuousTest()\n");
}