
#include <stdint.h>
#include <stdio.h>
#include "buttons.h"
#include "mio.h"
#include "utils.h"
#include "trigger.h"
#include "transmitter.h"

// The trigger state machine debounces both the press and release of gun
// trigger. Ultimately, it will activate the transmitter when a debounced press
// is detected.
#define TRIGGER_GUN_TRIGGER_MIO_PIN 10     // JF-2
#define GUN_TRIGGER_PRESSED 1
#define DEBOUNCE_COUNTER_MAX_VALUE 5000
#define STARTING_SHOTS 10

volatile static bool enabled = false;
volatile static bool shotFired = false;
volatile static bool ignoreGunInput = false;
static trigger_shotsRemaining_t shotsRemaining;
static uint32_t counter;
static bool debugPrint = true;

enum trigger_st {init_st, not_pressed_st, debounce_press_st, pressed_st, debounce_release_st} currentState_trig = init_st;

// Trigger can be activated by either btn0 or the external gun that is attached to TRIGGER_GUN_TRIGGER_MIO_PIN
// Gun input is ignored if the gun-input is high when the init() function is invoked.
bool triggerPressed() {
	return ((!ignoreGunInput & (mio_readPin(TRIGGER_GUN_TRIGGER_MIO_PIN) == GUN_TRIGGER_PRESSED)) || 
                (buttons_read() & BUTTONS_BTN0_MASK));
	//return buttons_read() & BUTTONS_BTN0_MASK;
}

// Init trigger data-structures.
// Determines whether the trigger switch of the gun is connected (see discussion
// in lab web pages). Initializes the mio subsystem.
void trigger_init(){
	debugPrint = false;
	mio_init(false);
	shotsRemaining = STARTING_SHOTS;
	mio_setPinAsInput(TRIGGER_GUN_TRIGGER_MIO_PIN);
	// If the trigger is pressed when trigger_init() is called, assume that the gun is not connected and ignore it.
	if (triggerPressed()) {
		ignoreGunInput = true;
	}
}

// Enable the trigger state machine. The trigger state-machine is inactive until
// this function is called. This allows you to ignore the trigger when helpful
// (mostly useful for testing).
void trigger_enable(){
	enabled = true;
	// printf("Trigger enabled\n");
}

// Disable the trigger state machine so that trigger presses are ignored.
void trigger_disable(){
	enabled = false;
	// printf("Trigger disabled\n");
}

// Returns the number of remaining shots.
trigger_shotsRemaining_t trigger_getRemainingShotCount(){
	return shotsRemaining;
}

// Sets the number of remaining shots.
void trigger_setRemainingShotCount(trigger_shotsRemaining_t count){
	shotsRemaining = count;
}

// Standard tick function.
void trigger_tick(){
	//State updates
	if(!enabled){
		currentState_trig = init_st;
	}
    switch (currentState_trig){
        case init_st:
      		shotFired = false;
			if(enabled){
				currentState_trig = not_pressed_st;
			}
            break;
        case not_pressed_st:
			// printf("not_pressed_st\n");
			//If the trigger is pressed, reset the debounce counter and transistion to debounce state
			if(triggerPressed() && shotsRemaining > 0){
				counter = 0;
				currentState_trig = debounce_press_st;
			}
			break;
        case debounce_press_st:
			// printf("debounce_press_st\n");
            if(!triggerPressed()){
				currentState_trig = not_pressed_st;
			}
			else if(counter > DEBOUNCE_COUNTER_MAX_VALUE){
				currentState_trig = pressed_st;
        		shotFired = true;
				transmitter_run();
				if(debugPrint){
					printf(" D \n");
				}
			}
            break;
        case pressed_st:
            // printf("pressed_st\n");
			//If the trigger is released, reset the debounce counter and transistion to debounce state
			if(!triggerPressed()){
				currentState_trig = debounce_release_st;
				counter = 0;
			}
			break;
        case debounce_release_st:
            // printf("debounce_release_st\n");
			if(triggerPressed()){
				currentState_trig = pressed_st;
			}
			else if(counter > DEBOUNCE_COUNTER_MAX_VALUE){	//Transistion to not_pressed_st and decrement shotsRemaining
				currentState_trig = not_pressed_st;
        		shotFired = false;
				--shotsRemaining;
				if(debugPrint){
					printf(" U \n");
				}
			}
			break;           
    }

	//State actions
    switch (currentState_trig){
        case init_st:
            counter = 0;
			break;
        case not_pressed_st:
			break;
        case debounce_press_st:
            ++counter;
            break;
        case pressed_st:
			break;
        case debounce_release_st:
            ++counter;
			break;
        }
	
}

//Returns true if the trigger state machine is in the pressed state
bool trigger_shotsFired() {
  return shotFired;
}
// Runs the test continuously until BTN1 is pressed.
// The test just prints out a 'D' when the trigger or BTN0
// is pressed, and a 'U' when the trigger or BTN0 is released.
#define TEST_DELAY 15000
#define BOUNCE_DELAY 5
void trigger_runTest(){
	printf("Starting trigger_runTest()\n");
	buttons_init();
    trigger_init();
    trigger_enable();
	trigger_setRemainingShotCount(STARTING_SHOTS);
	debugPrint = true;
	while(!(buttons_read() & BUTTONS_BTN1_MASK)){}
	trigger_disable();
	debugPrint = false;
	do {utils_msDelay(BOUNCE_DELAY);} while (buttons_read());
	printf("Completed trigger_runTest()\n");
}
