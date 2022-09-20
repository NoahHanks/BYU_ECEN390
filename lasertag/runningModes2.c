/*
This software is provided for student assignment use in the Department of
Electrical and Computer Engineering, Brigham Young University, Utah, USA.
Users agree to not re-host, or redistribute the software, in source or binary
form, to other persons or other institutions. Users may modify and use the
source code for personal or educational use.
For questions, contact Brad Hutchings or Jeff Goeders, https://ece.byu.edu/
*/

#include "runningModes.h"
#include "detector.h"
#include "display.h"
#include "buttons.h"
#include "switches.h"
#include "filter.h"
#include "histogram.h"
#include "hitLedTimer.h"
#include "interrupts.h"
#include "intervalTimer.h"
#include "isr.h"
#include "ledTimer.h"
#include "leds.h"
#include "lockoutTimer.h"
#include "mio.h"
#include "queue.h"
#include "sound.h"
#include "transmitter.h"
#include "trigger.h"
#include "utils.h"
#include "xparameters.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVINCIBILITY_TIMER INTERVAL_TIMER_TIMER_2  
#define HEALTH_REGEN_TIMER INTERVAL_TIMER_TIMER_2
#define END_SOUND_TIMER INTERVAL_TIMER_TIMER_2 
#define RELOAD_TIMER INTERVAL_TIMER_TIMER_1  
#define INVINCIBILITY_DURATION 5
#define RELOAD_DURATION 3
#define END_SOUND_DURATION 1
#define HEALTH_REGEN_DURATION 10

#define TEAM_A_FREQ 9
#define TEAM_B_FREQ 6
#define HEALTH 5
#define LIVES 3
#define AMMO 10

//Two team mode, 3 lives, 5 hits per life
void runningModes_twoTeams() {
  uint16_t hitCount = 0;
  uint16_t lifeCount = LIVES;
  bool reloadTimerRunning = false;
  sound_init();
	
  interrupts_initAll(true); // Inits all interrupts but does not enable them.
  interrupts_enableTimerGlobalInts(); // Allows the timer to generate
                                      // interrupts.
  interrupts_startArmPrivateTimer();  // Start the private ARM timer running.
  interrupts_enableArmInts(); // The ARM will start seeing interrupts after
                              // this.
  runningModes_initAll();
  
  sound_setVolume(SOUND_VOLUME_3);
  sound_playSound(sound_gameStart_e); //Gameboy start sound
	while(sound_isBusy()){
		
	}
  // sound_startSound();
  // More initialization...
  bool ignoredFrequencies[FILTER_FREQUENCY_COUNT];
  for (uint16_t i = 0; i < FILTER_FREQUENCY_COUNT; i++)
    ignoredFrequencies[i] = true;

  if ((switches_read() & SWITCHES_SW0_MASK) == SWITCHES_SW0_MASK) {	//if the switch is up, set it to Team A
    ignoredFrequencies[TEAM_B_FREQ] = false;
	transmitter_setFrequencyNumber(TEAM_A_FREQ);
  }
  else {	//if the switch is down, set it to Team B
    ignoredFrequencies[TEAM_A_FREQ] = false;
	transmitter_setFrequencyNumber(TEAM_B_FREQ);
  }
  
  detector_init(ignoredFrequencies);
  trigger_enable();         // Makes the trigger state machine responsive to the
                            // trigger.
  trigger_setRemainingShotCount(AMMO); // initialize the amount of ammo

  lockoutTimer_start(); // Ignore erroneous hits at startup (when all power
                        // values are essentially 0).
  // Implement game loop...
  bool gunShot = false;
  while(lifeCount > 0) { // Big loop for the game, keeps looping until you lose all 3 lives.
	  	detector(true);
		if (trigger_shotsFired() && trigger_getRemainingShotCount() > 0) { //Shoot sound when shooting.
            if(!gunShot){
				sound_setSound(sound_gunFire_e);
				sound_startSound();
				gunShot = true;
			}
			intervalTimer_start(RELOAD_TIMER);
			reloadTimerRunning = true;
        }
			
        if (!trigger_shotsFired() && trigger_getRemainingShotCount() > 0 && reloadTimerRunning){ //Reset reload timer if force hold is stopped.
			gunShot = false;
            intervalTimer_stop(RELOAD_TIMER);
            intervalTimer_reset(RELOAD_TIMER);
			reloadTimerRunning = false;
        }
        if (trigger_getRemainingShotCount() == 0 && !reloadTimerRunning) { //Just ran out of ammo, start reload timer.
            intervalTimer_stop(RELOAD_TIMER);
            intervalTimer_reset(RELOAD_TIMER);
            intervalTimer_start(RELOAD_TIMER);
			reloadTimerRunning = true;
        }
	  	if(triggerPressed() && trigger_getRemainingShotCount() == 0){ //Clicking sound when empty ammo and trying to shoot.
                sound_setSound(sound_gunClick_e);
                sound_startSound();
        }
		if (intervalTimer_getTotalDurationInSeconds(RELOAD_TIMER) > RELOAD_DURATION && reloadTimerRunning) { //Done reloading, play reload sound.
            trigger_setRemainingShotCount(AMMO);
			sound_setSound(sound_gunReload_e);
            sound_startSound();
			intervalTimer_stop(RELOAD_TIMER);
            intervalTimer_reset(RELOAD_TIMER);
			reloadTimerRunning = false;
        }
	    if(detector_hitDetected()){	//Process a hit
            ++hitCount;
            detector_clearHit();
            sound_setSound(sound_hit_e);
			sound_startSound();
            if(hitCount == HEALTH){ //Lost a life, reset hit count, play lose life sound, disable trigger, start invincibility timer
				--lifeCount;
				detector_ignoreAllHits(true);
				detector_clearHit();
				detector(true);
				sound_setSound(sound_loseLife_e);
				sound_startSound();
      			hitCount = 0;
      			trigger_disable();
                intervalTimer_start(INVINCIBILITY_TIMER);
                while (intervalTimer_getTotalDurationInSeconds(INVINCIBILITY_TIMER) < INVINCIBILITY_DURATION) {
                    //start 5 sec timer for invincibility
					detector(true);
					if(lifeCount == 0)
						break;
        		}
				detector(true);
				detector_clearHit();
				detector_ignoreAllHits(false);
			    trigger_enable();
                intervalTimer_stop(INVINCIBILITY_TIMER);
                intervalTimer_reset(INVINCIBILITY_TIMER);
	        }
		}

  }
  sound_setSound(sound_gameOver_e); //Game over sound
  sound_startSound();
  while(sound_isBusy()){	
  }

  hitLedTimer_turnLedOff();    // Save power :-)
  printf("Two-team mode terminated after detecting %d shots.\n", hitCount);

  while(1){	//Continuously tell the user to return to base until they get annoyed and shut the backpack off
	sound_setSound(sound_returnToBase_e);
	sound_startSound();
	while(sound_isBusy()){	
	}
	  
  	intervalTimer_start(END_SOUND_TIMER);
	while (intervalTimer_getTotalDurationInSeconds(END_SOUND_TIMER) < END_SOUND_DURATION) {
		//start 1 sec timer for silence
	}
	intervalTimer_stop(END_SOUND_TIMER);
	intervalTimer_reset(END_SOUND_TIMER);

	

  }

  interrupts_disableArmInts(); // Done with game loop, disable the interrupts.
}

void runningModes_creativeProject() {
  uint16_t hitCount = 0;
  bool reloadTimerRunning = false;
  sound_init();
	
  interrupts_initAll(true); // Inits all interrupts but does not enable them.
  interrupts_enableTimerGlobalInts(); // Allows the timer to generate
                                      // interrupts.
  interrupts_startArmPrivateTimer();  // Start the private ARM timer running.
  interrupts_enableArmInts(); // The ARM will start seeing interrupts after
                              // this.
  runningModes_initAll();
  
  sound_setVolume(SOUND_VOLUME_3);
  sound_playSound(sound_johnCena_e); //Gameboy start sound
	while(sound_isBusy()){
		
	}
	sound_setSound(sound_oneSecondSilence_e);
	sound_startSound();
	while(sound_isBusy()){	
	}
  // sound_startSound();
  // More initialization...
  bool ignoredFrequencies[FILTER_FREQUENCY_COUNT];
  for (uint16_t i = 0; i < FILTER_FREQUENCY_COUNT; i++)
    ignoredFrequencies[i] = true;

  if ((switches_read() & SWITCHES_SW0_MASK) == SWITCHES_SW0_MASK) {	//if the switch is up, set it to Team A
    ignoredFrequencies[TEAM_B_FREQ] = false;
	transmitter_setFrequencyNumber(TEAM_A_FREQ);
	sound_playSound(sound_teamOne_e);
  }
  else {	//if the switch is down, set it to Team B
    ignoredFrequencies[TEAM_A_FREQ] = false;
	transmitter_setFrequencyNumber(TEAM_B_FREQ);
	sound_playSound(sound_teamTwo_e);
  }
  while(sound_isBusy()){
		
  }
  
  detector_init(ignoredFrequencies);
  trigger_enable();         // Makes the trigger state machine responsive to the
                            // trigger.
  trigger_setRemainingShotCount(AMMO); // initialize the amount of ammo

  lockoutTimer_start(); // Ignore erroneous hits at startup (when all power
                        // values are essentially 0).
  // Implement game loop...
  bool gunShot = false;
  while(true) { // Big loop for the game, keeps looping until you lose all 3 lives.
	  	detector(true);
		if (trigger_shotsFired() && trigger_getRemainingShotCount() > 0) { //Shoot sound when shooting.
            if(!gunShot){
				sound_setSound(sound_newShot_e);
				sound_startSound();
				gunShot = true;
			}
			intervalTimer_start(RELOAD_TIMER);
			reloadTimerRunning = true;
        }
			
        if (!trigger_shotsFired() && trigger_getRemainingShotCount() > 0 && reloadTimerRunning){ //Reset reload timer if force hold is stopped.
			gunShot = false;
            intervalTimer_stop(RELOAD_TIMER);
            intervalTimer_reset(RELOAD_TIMER);
			reloadTimerRunning = false;
        }
        if (trigger_getRemainingShotCount() == 0 && !reloadTimerRunning) { //Just ran out of ammo, start reload timer.
            intervalTimer_stop(RELOAD_TIMER);
            intervalTimer_reset(RELOAD_TIMER);
            intervalTimer_start(RELOAD_TIMER);
			reloadTimerRunning = true;
        }
	  	if(triggerPressed() && trigger_getRemainingShotCount() == 0){ //Clicking sound when empty ammo and trying to shoot.
                sound_setSound(sound_gunClick_e);
                sound_startSound();
        }
		if (intervalTimer_getTotalDurationInSeconds(RELOAD_TIMER) > RELOAD_DURATION && reloadTimerRunning) { //Done reloading, play reload sound.
            trigger_setRemainingShotCount(AMMO);
			sound_setSound(sound_gunReload_e);
            sound_startSound();
			intervalTimer_stop(RELOAD_TIMER);
            intervalTimer_reset(RELOAD_TIMER);
			reloadTimerRunning = false;
        }
	  	if (intervalTimer_getTotalDurationInSeconds(HEALTH_REGEN_TIMER) > HEALTH_REGEN_DURATION) {
			if (hitCount > 0) {
				hitCount--;
				sound_playSound(sound_gameStart_e); //Gameboy start sound
				while(sound_isBusy()){
		
				}
			}
			intervalTimer_stop(HEALTH_REGEN_TIMER);
            intervalTimer_reset(HEALTH_REGEN_TIMER);
			intervalTimer_start(HEALTH_REGEN_TIMER);
		}
	    if(detector_hitDetected()){	//Process a hit
			intervalTimer_stop(HEALTH_REGEN_TIMER);
            intervalTimer_reset(HEALTH_REGEN_TIMER);
            ++hitCount;
            detector_clearHit();
            sound_setSound(sound_robloxOof_e);
			sound_startSound();
            if(hitCount == HEALTH){ //Lost a life, reset hit count, play lose life sound, disable trigger, start invincibility timer
				transmitter_setFrequencyNumber(detector_getFrequencyNumberOfLastHit());
				for (uint16_t i = 0; i < FILTER_FREQUENCY_COUNT; i++)
				    ignoredFrequencies[i] = true;
				if(transmitter_getFrequencyNumber() == TEAM_A_FREQ){
					ignoredFrequencies[TEAM_B_FREQ] = false;
					transmitter_setFrequencyNumber(TEAM_A_FREQ); 
				}
				else if (transmitter_getFrequencyNumber() == TEAM_B_FREQ){
					ignoredFrequencies[TEAM_A_FREQ] = false;
					transmitter_setFrequencyNumber(TEAM_B_FREQ);
				}
				detector_init(ignoredFrequencies);
				detector_ignoreAllHits(true);
				detector_clearHit();
				detector(true);
      			hitCount = 0;
      			trigger_disable();
				sound_setSound(sound_loseLife_e);
				sound_startSound();
                intervalTimer_start(INVINCIBILITY_TIMER);
                while (intervalTimer_getTotalDurationInSeconds(INVINCIBILITY_TIMER) < INVINCIBILITY_DURATION) {
                    //start 5 sec timer for invincibility
					detector(true);
        		}
				if(transmitter_getFrequencyNumber() == TEAM_A_FREQ){
					sound_playSound(sound_teamOne_e);
				}
				else if (transmitter_getFrequencyNumber() == TEAM_B_FREQ){
					sound_playSound(sound_teamTwo_e);
				}
				while(sound_isBusy()){}
				detector(true);
				detector_clearHit();
				detector_ignoreAllHits(false);
			    trigger_enable();
                intervalTimer_stop(INVINCIBILITY_TIMER);
                intervalTimer_reset(INVINCIBILITY_TIMER);

	        }
			intervalTimer_start(HEALTH_REGEN_TIMER);
		}

  }

  interrupts_disableArmInts(); // Done with game loop, disable the interrupts.
}
