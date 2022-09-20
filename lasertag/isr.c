
#include <stdint.h>
#include "lockoutTimer.h"
#include "hitLedTimer.h"
#include "trigger.h"
#include "transmitter.h"
#include "interrupts.h"
#include "detector.h"
#include "sound.h"

typedef uint32_t
    isr_AdcValue_t; // Used to represent ADC values in the ADC buffer.

#define ADC_BUFFER_SIZE 20001
#define NUM_PLAYERS 10

static isr_AdcValue_t adcBuffer[ADC_BUFFER_SIZE];
static uint32_t frontIndex = 0;
static uint32_t backIndex = 0;
static uint32_t elementCount = 0;


// isr provides the isr_function() where you will place functions that require
// accurate timing. A buffer for storing values from the Analog to Digital
// Converter (ADC) is implemented in isr.c Values are added to this buffer by
// the code in isr.c. Values are removed from this queue by code in detector.c

//this initializes the ADC buffer much like queue_init() does. It would make sense to have isr_init() invoke this function.
void adcBufferInit(){
    for(uint32_t i = 0; i < ADC_BUFFER_SIZE; i++)
        adcBuffer[i] = 0;
	frontIndex = 0;
	backIndex = 0;
	elementCount = 0;
}

// Performs inits for anything in isr.c
void isr_init(){
	lockoutTimer_init();
    hitLedTimer_init();
	trigger_init();
	transmitter_init();
    adcBufferInit();
    
}

// This adds data to the ADC queue. Data are removed from this queue and used by
// the detector.
//this is similar to queue_overwritePush() with an uint32_t argument instead of a double. If the circular buffer is full, overwrite the oldest value.
void isr_addDataToAdcBuffer(uint32_t adcData){
	adcBuffer[backIndex] = adcData;
	backIndex = (backIndex + 1) % ADC_BUFFER_SIZE;
	if(elementCount == ADC_BUFFER_SIZE){
		frontIndex = (frontIndex + 1) % ADC_BUFFER_SIZE;
	}
	else{
		++elementCount;
	}
}

// This function is invoked by the timer interrupt at 100 kHz.
void isr_function(){
    lockoutTimer_tick();
	hitLedTimer_tick();
	trigger_tick();
    transmitter_tick();
	sound_tick();
	isr_addDataToAdcBuffer(interrupts_getAdcData());
}

// This removes a value from the ADC buffer.
uint32_t isr_removeDataFromAdcBuffer(){
	if(elementCount == 0){
		return 0;
	}
	--elementCount;
	uint16_t indexToRemove = frontIndex;
	frontIndex = (frontIndex + 1) % ADC_BUFFER_SIZE;
    return adcBuffer[indexToRemove];
}

// This returns the number of values in the ADC buffer.
uint32_t isr_adcBufferElementCount(){
	return elementCount;
}

