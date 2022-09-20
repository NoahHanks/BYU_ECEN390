#include "detector.h"
#include "filter.h"
#include "hitLedTimer.h"
#include "lockoutTimer.h"
#include "interrupts.h"
#include "transmitter.h"
#include "isr.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>


#define NUM_PLAYERS 10
#define ADC_MAX_VALUE 4095.0
#define ADC_DOUBLE_SCALAR 2
#define MEDIAN_ELEMENT 4
#define ZERO_TO_NINE_ARRAY {0,1,2,3,4,5,6,7,8,9}
#define DEFAULT_FUDGE_FACTOR 3000

static uint32_t fudgeFactor;
static bool ignoredFreq[NUM_PLAYERS];
static bool interruptsNotEnabled = true;
static bool ignoreSelf = false;
static bool ignoreAll = false;
static bool detectorTestMode = false;
static uint8_t detectorInvocationCount = 0;
static bool forceComputePower = true;
static bool detector_hitDetectedFlag = false;
static uint32_t detector_hitArray[NUM_PLAYERS];
static uint16_t lastHitNumber;
static double testPowerData[10];

// Always have to init things.
// bool array is indexed by frequency number, array location set for true to
// ignore, false otherwise. This way you can ignore multiple frequencies.
void detector_init(bool ignoredFrequencies[]){
    hitLedTimer_enable();
    filter_init();
	for(uint8_t i = 0; i < NUM_PLAYERS; ++i){	//Initialize ignoredFrequencies and detector_hitArray
		ignoredFreq[i] = ignoredFrequencies[i];
		detector_hitArray[i] = 0;
	}
	detectorInvocationCount = 0;
	forceComputePower = true;
	detector_hitDetectedFlag = false;
	fudgeFactor = DEFAULT_FUDGE_FACTOR;
	ignoreSelf = true;
}

// This function uses an insertion sort method to take the array of power values and sort them in descending order. It get input from the aray and the number of elements in that array.
void insertion_sort(double powerValuesArray[], uint8_t indiciesArray[], uint8_t numElements){
    uint8_t i;
	uint8_t j;
	for(i = 1; i < numElements; ++i){	//For each element, move it to the proper location
		j = i - 1;
		while(j >= 0 && powerValuesArray[indiciesArray[j]] < powerValuesArray[i]){	//Move backwards until the right spot is found
			uint8_t temp = indiciesArray[j];
			indiciesArray[j] = indiciesArray[j + 1];
			indiciesArray[j + 1] = temp;
			j--;
		}
	}
}

// Runs the entire detector: decimating fir-filter, iir-filters,
// power-computation, hit-detection. if interruptsNotEnabled = true, interrupts
// are not running. If interruptsNotEnabled = true you can pop values from the
// ADC queue without disabling interrupts. If interruptsNotEnabled = false, do
// the following:
// 1. disable interrupts.
// 2. pop the value from the ADC queue.
// 3. re-enable interrupts if interruptsNotEnabled was true.
// if ignoreSelf == true, ignore hits that are detected on your frequency.
// Your frequency is simply the frequency indicated by the slide switches
void detector(bool interruptsCurrentlyEnabled){
	uint32_t elementCount = isr_adcBufferElementCount();
	for(uint32_t i = 0; i < elementCount; ++i){	//Process each element in the ADC buffer
		if(interruptsCurrentlyEnabled){
			interrupts_disableArmInts();
		}
		uint32_t rawAdcValue = isr_removeDataFromAdcBuffer();
		if(interruptsCurrentlyEnabled){
			interrupts_enableArmInts();
		}
        double scaledAdcValue = ADC_DOUBLE_SCALAR * ((double)rawAdcValue) / (ADC_MAX_VALUE) - 1;
        detectorInvocationCount++;
		filter_addNewInput(scaledAdcValue);
		if(detectorInvocationCount == NUM_PLAYERS){	//If we have added 10 items, run the filters
            detectorInvocationCount = 0;
			filter_firFilter();            
			for(uint8_t j = 0; j < NUM_PLAYERS; ++j){
				filter_iirFilter(j);
			}
			for(uint8_t j = 0; j < NUM_PLAYERS; ++j){
				filter_computePower(j, forceComputePower, false);
			}
            forceComputePower = false;
            if(!lockoutTimer_running() && !hitLedTimer_running() && !detector_hitDetectedFlag) { // Checks if the timers are still running before checking for another hit.
                //do hit-detection algorithm
                double powerValues[NUM_PLAYERS];
				if(!detectorTestMode){
					filter_getCurrentPowerValues(powerValues);
				}
				else{  
                    for(uint8_t k = 0; k < NUM_PLAYERS; ++k)
					    powerValues[k] = testPowerData[k];
				}
				uint8_t indicies[NUM_PLAYERS] = ZERO_TO_NINE_ARRAY;
				insertion_sort(powerValues, indicies, NUM_PLAYERS);
				double threshold = powerValues[indicies[MEDIAN_ELEMENT]] * fudgeFactor;

				uint8_t maxIndex = indicies[0];
				double max = powerValues[maxIndex];			
                if(max > threshold && !ignoredFreq[maxIndex] && !ignoreAll && !(maxIndex == transmitter_getFrequencyNumber() && ignoreSelf)){	//If a hit was detected and not ignored, start timers and set flag   
					lastHitNumber = maxIndex;
					lockoutTimer_start();
					hitLedTimer_start();
                    detector_hitArray[detector_getFrequencyNumberOfLastHit()]++;
                    detector_hitDetectedFlag = true;
				}                
            }
			
		}
	}
}

// Returns true if a hit was detected.
bool detector_hitDetected(){
    return detector_hitDetectedFlag;
}

// Returns the frequency number that caused the hit.
uint16_t detector_getFrequencyNumberOfLastHit(){
    return lastHitNumber;
}

// Clear the detected hit once you have accounted for it.
void detector_clearHit(){
    detector_hitDetectedFlag = false;
}

// Ignore all hits. Used to provide some limited invincibility in some game
// modes. The detector will ignore all hits i("%f ", f the flag is true, otherwise will
// respond to hits normally.
void detector_ignoreAllHits(bool flagValue){
    ignoreAll = flagValue;
}

// Get the current hit counts.
// Copy the current hit counts into the user-provided hitArray
// using a for-loop.
void detector_getHitCounts(detector_hitCount_t hitArray[]){
    for(uint8_t i = 0; i < NUM_PLAYERS; i++)
        hitArray[i] = detector_hitArray[i];
}

// Allows the fudge-factor index to be set externally from the detector.
// The actual values for fudge-factors is stored in an array found in detector.c
void detector_setFudgeFactorIndex(uint32_t factor){
    fudgeFactor = factor;
}

// Encapsulate ADC scaling for easier testing.
double detector_getScaledAdcValue(isr_AdcValue_t adcValue){
    return (ADC_DOUBLE_SCALAR * (adcValue) / (ADC_MAX_VALUE) - 1);
}

/*******************************************************
 ****************** Test Routines **********************
 ******************************************************/

// Runs two tests of the detector code. One should register a hit and the other should not.
void detector_runTest(){
    detectorTestMode = true;

	double testPowerData1[10] = {25, 17, 0, 18, 34, 23, 57, 11, 4600, 40};
	double testPowerData2[10] = {25, 17, 0, 16, 34, 23, 57, 11, 46, 40};

	srand(0);
	for(uint8_t k = 0; k < NUM_PLAYERS; ++k)
		testPowerData[k] = testPowerData1[k];
	
	bool ignored[NUM_PLAYERS] = {false, false, false, false, false,false, false, false, false, false};
	detector_init(ignored);
	isr_init();
	printf("\nFirst test\n----------");
    for(uint32_t i = 0; i < 20000; ++i) {	//Create 20000 random buffer values
		if(i % 15 == 0){
			isr_addDataToAdcBuffer(4095);
		}
		else{
        	isr_addDataToAdcBuffer(rand() % 2000);
		}
	}
	detector_setFudgeFactorIndex(20);
	detector(false);
	if(detector_hitDetected()){ //Supposed to detect a hit from player 8.
		printf("\nHit detected for player %d\n", detector_getFrequencyNumberOfLastHit());
	}
	else{
		printf("\nNo hit detected\n");
	}
	detector_clearHit();
	lockoutTimer_init();
	hitLedTimer_init();

	printf("\nSecond test\n-----------");
	for(uint8_t k = 0; k < NUM_PLAYERS; ++k)
		testPowerData[k] = testPowerData2[k];
	detector_init(ignored);
    for(uint32_t i = 0; i < 20000; ++i)
        isr_addDataToAdcBuffer(rand() % 2000);
	detector_setFudgeFactorIndex(20);
	detector(false);
	if(detector_hitDetected()){ // No hit should be detected.
		printf("\nHit detected for player %d\n", detector_getFrequencyNumberOfLastHit());
	}
	else{
		printf("\nNo hit detected\n");
	}
	detector_clearHit();
	printf("\nCompleted Detector_RunTest\n");
}
