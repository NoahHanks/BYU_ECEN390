/*
This software is provided for student assignment use in the Department of
Electrical and Computer Engineering, Brigham Young University, Utah, USA.

Users agree to not re-host, or redistribute the software, in source or binary
form, to other persons or other institutions. Users may modify and use the
source code for personal or educational use.

For questions, contact Brad Hutchings or Jeff Goeders, https://ece.byu.edu/
*/

#include <stdint.h>
#include <stdio.h>
#include <xparameters.h>

#include "bhTester.h"
#include "buttonHandler.h"
#include "config.h"
#include "display.h"
#include "flashSequence.h"
#include "fsTester.h"
#include "interrupts.h"
#include "intervalTimer.h"
#include "leds.h"
#include "simonControl.h"
#include "simonDisplay.h"
#include "utils.h"
#include "verifySequence.h"
#include "vsTester.h"

#define MILESTONE_1 1
#define MILESTONE_2 2
#define MILESTONE_3 3
#define MILESTONE_4 4

////////////////////////////////////////////////////////////////////////////////
// Uncomment one of the following lines to run Milestone 1, 2, 3, or 4    //////
////////////////////////////////////////////////////////////////////////////////
// #define RUN_PROGRAM MILESTONE_1
// #define RUN_PROGRAM MILESTONE_2
// #define RUN_PROGRAM MILESTONE_3
// #define RUN_PROGRAM MILESTONE_4

// If nothing is uncommented above, run milestone 4
#ifndef RUN_PROGRAM
#define RUN_PROGRAM MILESTONE_4
#endif

// Compute the timer clock freq.
#define TIMER_CLOCK_FREQUENCY (XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 2)
// Compute timer load value.
#define TIMER_LOAD_VALUE ((CONFIG_TIMER_PERIOD * TIMER_CLOCK_FREQUENCY) - 1.0)

#define INTERRUPTS_PER_SECOND (1.0 / CONFIG_TIMER_PERIOD)
#define TOTAL_SECONDS 45
#define MAX_INTERRUPT_COUNT (INTERRUPTS_PER_SECOND * TOTAL_SECONDS)

/****************************** RUN_BUTTON_HANDLER_TEST ****************/
#if RUN_PROGRAM == MILESTONE_1
static void test_init() {
  leds_init(true);
  bhTester_init();
  printf("Running the buttonHandler test.\n");
}

void tickAll() {
  buttonHandler_tick();
  bhTester_tick();
}
#endif

/****************************** RUN_SIMON_FLASH_SEQUENCE_TEST ****************/
#if RUN_PROGRAM == MILESTONE_2
static void test_init() {
  leds_init(true);
  fsTester_init();
  printf("Running the flashSequence test.\n");
}

void tickAll() {
  fsTester_tick();
  flashSequence_tick();
}

/*********************** RUN VERIFY SEQUENCE TEST ***************************/
#elif RUN_PROGRAM == MILESTONE_3
static void test_init() {
  vsTester_init();
  printf("Running the verifySequence test.\n");
}

void tickAll() {
  buttonHandler_tick();
  flashSequence_tick();
  verifySequence_tick();
  vsTester_tick();
}

/****************************** RUN_SIMON_GAME ****************/
#elif RUN_PROGRAM == MILESTONE_4
static void test_init() {
  display_init();
  leds_init(true);
  simonControl_init();
  buttonHandler_init();
  flashSequence_init();
  verifySequence_init();
  display_fillScreen(DISPLAY_BLACK);
  simonControl_enable();
  printf("Running the simon game.\n");
}

void tickAll() {
  simonControl_tick();
  buttonHandler_tick();
  flashSequence_tick();
  verifySequence_tick();
}
#endif

// All programs share the same main.
// Differences are limited to test_init() and isr_function().
int main() {
  test_init(); // Program specific.
  // Init all interrupts (but does not enable the interrupts at the devices).
  // Prints an error message if an internal failure occurs because the argument
  // = true.
  interrupts_initAll(true);
  interrupts_setPrivateTimerLoadValue(TIMER_LOAD_VALUE);
  interrupts_enableTimerGlobalInts();
  // Keep track of your personal interrupt count. Want to make sure that you
  // don't miss any interrupts.
  int32_t personalInterruptCount = 0;
  // Start the private ARM timer running.
  interrupts_startArmPrivateTimer();
  // Enable interrupts at the ARM.
  interrupts_enableArmInts();
  while (1) {
    if (interrupts_isrFlagGlobal) {
      // Count ticks.
      personalInterruptCount++;
      tickAll();
      interrupts_isrFlagGlobal = 0;
      if (personalInterruptCount >= MAX_INTERRUPT_COUNT)
        break;
      utils_sleep();
    }
  }
  interrupts_disableArmInts();
  printf("isr invocation count: %d\n", interrupts_isrInvocationCount());
  printf("internal interrupt count: %d\n", personalInterruptCount);
  return 0;
}

// Interrupt routine
void isr_function() {
  // Empty for flag method (flag set elsewhere)
}