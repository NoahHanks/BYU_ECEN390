/*
This software is provided for student assignment use in the Department of
Electrical and Computer Engineering, Brigham Young University, Utah, USA.
Users agree to not re-host, or redistribute the software, in source or binary
form, to other persons or other institutions. Users may modify and use the
source code for personal or educational use.
For questions, contact Brad Hutchings or Jeff Goeders, https://ece.byu.edu/
*/

#ifndef RUNNINGMODES_H_
#define RUNNINGMODES_H_

#define RUNNING_MODES_TWO_TEAM_TEAM0_FREQUENCY                                 \
  6 // Team-0 players shoot at this frequency.
#define RUNNING_MODES_TWO_TEAM_TEAM1_FREQUENCY                                 \
  9 // Team-1 players shoot at this frequency.

// Used by runningModes_twoTeams()
#define LIVES 3
#define HITS_PER_LIFE 5


#include <stdint.h>

// Prints out various run-time statistics on the TFT display.
// Assumes the following:
// detected interrupts is retrieved with interrupts_isrInvocationCount(),
// interval_timer(0) is the cumulative run-time of the ISR,
// interval_timer(1) is the total run-time,
// interval_timer(2) is the time spent in main running the filters, updating the
// display, and so forth. No comments in the code, the print statements are
// self-explanatory.
void runningModes_printRunTimeStatistics();

// Group all of the inits together to reduce visual clutter.
void runningModes_initAll();

// This mode runs continuously until btn3 is pressed.
// When btn3 is pressed, it exits and prints performance information to the TFT.
// During operation, it continuously displays that received power on each
// channel, on the TFT. Frequency is selected via the slide-switches.
void runningModes_continuous();

// This mode runs continuously until btn3 is pressed.
// When btn3 is pressed, it exits and prints performance information to the TFT.
// Game-playing mode. Each shot is registered on the histogram on the TFT.
// Press BTN0 or the gun-trigger to shoot.
// Frequency is selected via the slide-switches.
void runningModes_shooter();

// This mode supports two teams, Team-A and Team-B.
// Each team operates on its own configurable frequency.
// Each player has a fixed set of lives and once they
// have expended all lives, operation ceases and they are told
// to return to base to await the ultimate end of the game.
// The gun is clip-based and each clip contains a fixed number of shots
// that takes a short time to reload a new clip.
// The clips are automatically loaded.
void runningModes_twoTeams();

void runningModes_zombie();

// Continuously cycles through all channels, shooting one pulse per channel.
void runningModes_testShootAllChannels();

// Returns the current switch-setting
uint16_t runningModes_getFrequencySetting();

// A simple test mode that continuously prints out raw ADC values.
void runningModes_dumpRawAdcValues();

#endif /* RUNNINGMODES_H_ */
