#ifndef FILTER_H_
#define FILTER_H_

#include "queue.h"
#include "filterCoefficients.h"
#include <stdint.h>

#define FILTER_SAMPLE_FREQUENCY_IN_KHZ 100
#define FILTER_FREQUENCY_COUNT 10
#define FILTER_FIR_DECIMATION_FACTOR                                           \
  10 // FIR-filter needs this many new inputs to compute a new output.
#define FILTER_INPUT_PULSE_WIDTH                                               \
  2000 // This is the width of the pulse you are looking for, in terms of
       // decimated sample count.
// These are the tick counts that are used to generate the user frequencies.
// Not used in filter.h but are used to TEST the filter code.
// Placed here for general access as they are essentially constant throughout
// the code. The transmitter will also use these.
static const uint16_t filter_frequencyTickTable[FILTER_FREQUENCY_COUNT] = {
    68, 58, 50, 44, 38, 34, 30, 28, 26, 24};

#define IIR_A_COEFFICIENT_COUNT 10
#define QUEUE_INIT_VALUE 0.0
#define X_QUEUE_SIZE 81
#define Y_QUEUE_SIZE IIR_B_COEFFICIENT_COUNT
#define Z_QUEUE_SIZE IIR_A_COEFFICIENT_COUNT
#define OUTPUT_QUEUE_SIZE 2000
#define FILTER_IIR_FILTER_COUNT 10

//Queue declarations
static queue_t xQueue;
static queue_t yQueue;
static queue_t zQueue[FILTER_IIR_FILTER_COUNT];
static queue_t outputQueue[FILTER_IIR_FILTER_COUNT];

// Filtering routines for the laser-tag project.
// Filtering is performed by a two-stage filter, as described below.

// 1. First filter is a decimating FIR filter with a configurable number of taps
// and decimation factor.
// 2. The output from the decimating FIR filter is passed through a bank of 10
// IIR filters. The characteristics of the IIR filter are fixed.

/*********************************************************************************************************
****************************************** Main Filter Functions
******************************************
**********************************************************************************************************/

//Initializes the xQueue and fills it with zeros
void initXQueue(){
    queue_init(&xQueue, X_QUEUE_SIZE, "xQueue");
    for (uint32_t j=0; j<X_QUEUE_SIZE; j++)
        queue_overwritePush(&(xQueue), QUEUE_INIT_VALUE);
}

// Initializes and fills the yQueue with all zeros.
void initYQueue(){
    queue_init(&yQueue, Y_QUEUE_SIZE, "yQueue");
    for (uint32_t j=0; j<Y_QUEUE_SIZE; j++)
        queue_overwritePush(&(yQueue), QUEUE_INIT_VALUE);
}

// Call queue_init() on all of the zQueues and fill each z queue with zeros.
void initZQueues(){
    //Loop through each queue and initialize
    for (uint32_t i=0; i<FILTER_IIR_FILTER_COUNT; i++) {
        queue_init(&(zQueue[i]), Z_QUEUE_SIZE, "zQueue");
        for (uint32_t j=0; j<Z_QUEUE_SIZE; j++)
            queue_overwritePush(&(zQueue[i]), QUEUE_INIT_VALUE);
    }
}

// Call queue_init() on all of the outputQueues and fill each output queue with zeros.
void initOutputQueues(){
  //Loop through each queue and initialize
    for (uint32_t i=0; i<FILTER_IIR_FILTER_COUNT; i++) {
        queue_init(&(outputQueue[i]), OUTPUT_QUEUE_SIZE, "outputQueue");
        for (uint32_t j=0; j<OUTPUT_QUEUE_SIZE; j++)
            queue_overwritePush(&(outputQueue[i]), QUEUE_INIT_VALUE);
    }
}

// Must call this prior to using any filter functions.
void filter_init(){
    // Init queues and fill them with 0s.
    initXQueue();  // Call queue_init() on xQueue and fill it with zeros.
    initYQueue();  // Call queue_init() on yQueue and fill it with zeros.
    initZQueues(); // Call queue_init() on all of the zQueues and fill each z queue with zeros.
    initOutputQueues();  // Call queue_init() all of the outputQueues and fill each outputQueue with zeros.
}

// Use this to copy an input into the input queue of the FIR-filter (xQueue).
void filter_addNewInput(double x){
    queue_overwritePush(&(xQueue), x);
}

// Fills a queue with the given fillValue. For example,
// if the queue is of size 10, and the fillValue = 1.0,
// after executing this function, the queue will contain 10 values
// all of them 1.0.
void filter_fillQueue(queue_t *q, double fillValue){
    for (uint32_t i=0; i<queue_size(q); i++) 
        queue_overwritePush((q), fillValue);
}

// Invokes the FIR-filter. Input is contents of xQueue.
// Output is returned and is also pushed on to yQueue.
double filter_firFilter(){
    double y = 0.0;
    for (uint32_t i=0; i<FIR_FILTER_TAP_COUNT; i++)
        y += queue_readElementAt(&xQueue, FIR_FILTER_TAP_COUNT-1-i) * firCoefficients[i];
    queue_overwritePush(&yQueue, y);
    return y;
}

// Use this to invoke a single iir filter. Input comes from yQueue.
// Output is returned and is also pushed onto zQueue[filterNumber].
double filter_iirFilter(uint16_t filterNumber){
    double z = 0.0;
    double y = 0.0;

    for (uint32_t i=0; i<IIR_B_COEFFICIENT_COUNT; i++)
      y += queue_readElementAt(&yQueue, (IIR_B_COEFFICIENT_COUNT-1)-i) * iirBCoefficientConstants[filterNumber][i];
    for (uint32_t i=0; i<IIR_A_COEFFICIENT_COUNT; i++)
      z += queue_readElementAt(&zQueue[filterNumber], (IIR_A_COEFFICIENT_COUNT-1)-i) * iirACoefficientConstants[filterNumber][i];
    queue_overwritePush(&zQueue[filterNumber], y - z);
    queue_overwritePush(&outputQueue[filterNumber], y - z);
	return y-z;
}

static double currentPowerValue[FILTER_FREQUENCY_COUNT];

// Use this to compute the power for values contained in an outputQueue.
// If force == true, then recompute power by using all values in the
// outputQueue. This option is necessary so that you can correctly compute power
// values the first time. After that, you can incrementally compute power values
// by:
// 1. Keeping track of the power computed in a previous run, call this
// prev-power.
// 2. Keeping track of the oldest outputQueue value used in a previous run, call
// this oldest-value.
// 3. Get the newest value from the power queue, call this newest-value.
// 4. Compute new power as: prev-power - (oldest-value * oldest-value) +
// (newest-value * newest-value). Note that this function will probably need an
// array to keep track of these values for each of the 10 output queues.
double filter_computePower(uint16_t filterNumber, bool forceComputeFromScratch, bool debugPrint){
	static double oldestValue[FILTER_FREQUENCY_COUNT];
	double sum = 0.0;

	//Recompute all power values from scratch if forceComputeFromScratch == true
    if(forceComputeFromScratch){
		//Loop through all queue values and sum up the power
        for(uint32_t i = 0; i < OUTPUT_QUEUE_SIZE; ++i){
            double elementVal = queue_readElementAt(&outputQueue[filterNumber], i);
            sum += elementVal * elementVal;
        }
        currentPowerValue[filterNumber] = sum;
    }
    else{	//If forceComputefromScratch == false, remove the oldest value from the previous sum and add the newest value
      	sum = currentPowerValue[filterNumber] - (oldestValue[filterNumber] * oldestValue[filterNumber]) + (queue_readElementAt(&outputQueue[filterNumber],
        	OUTPUT_QUEUE_SIZE - 1) * queue_readElementAt(&outputQueue[filterNumber], OUTPUT_QUEUE_SIZE - 1));
    	currentPowerValue[filterNumber] = sum;
	}

    oldestValue[filterNumber] = queue_readElementAt(&outputQueue[filterNumber], 0);
    return currentPowerValue[filterNumber];
}

// Returns the last-computed output power value for the IIR filter
// [filterNumber].
double filter_getCurrentPowerValue(uint16_t filterNumber){
	return currentPowerValue[filterNumber];
}

// Get a copy of the current power values.
// This function copies the already computed values into a previously-declared
// array so that they can be accessed from outside the filter software by the
// detector. Remember that when you pass an array into a C function, changes to
// the array within that function are reflected in the returned array.
void filter_getCurrentPowerValues(double powerValues[]){
    for(uint32_t i = 0; i < FILTER_FREQUENCY_COUNT; i++)
        powerValues[i] = currentPowerValue[i];
}

// Using the previously-computed power values that are current stored in
// currentPowerValue[] array, Copy these values into the normalizedArray[]
// argument and then normalize them by dividing all of the values in
// normalizedArray by the maximum power value contained in currentPowerValue[].
void filter_getNormalizedPowerValues(double normalizedArray[],
                                     uint16_t *indexOfMaxValue){		
    double max = currentPowerValue[0];
	//Loop through all filters to find which filter had the make power
    for(uint32_t i = 0; i < FILTER_FREQUENCY_COUNT; i++) {
		//If the current value is the largest, update max and indexOfMaxValue
        if(currentPowerValue[i] > max){
            max = currentPowerValue[i];
			*indexOfMaxValue = i;
		}
	}
    
	for(uint32_t i = 0; i < FILTER_FREQUENCY_COUNT; i++)
        normalizedArray[i] = currentPowerValue[i] / max;
}

/*********************************************************************************************************
********************************** Verification-assisting functions.
**************************************
********* Test functions access the internal data structures of the filter.c via
*these functions. ********
*********************** These functions are not used by the main filter
*functions. ***********************
**********************************************************************************************************/

// Returns the array of FIR coefficients.
const double *filter_getFirCoefficientArray(){
  return firCoefficients;
}

// Returns the number of FIR coefficients.
uint32_t filter_getFirCoefficientCount(){
  return FIR_FILTER_TAP_COUNT;
}

// Returns the array of coefficients for a particular filter number.
const double *filter_getIirACoefficientArray(uint16_t filterNumber){
  return iirACoefficientConstants[filterNumber];
}

// Returns the number of A coefficients.
uint32_t filter_getIirACoefficientCount(){
  return IIR_A_COEFFICIENT_COUNT;
}

// Returns the array of coefficients for a particular filter number.
const double *filter_getIirBCoefficientArray(uint16_t filterNumber){
  return iirBCoefficientConstants[filterNumber];
}

// Returns the number of B coefficients.
uint32_t filter_getIirBCoefficientCount(){
  return IIR_B_COEFFICIENT_COUNT;
}

// Returns the size of the yQueue.
uint32_t filter_getYQueueSize(){
  return queue_size(&yQueue);
}

// Returns the decimation value.
uint16_t filter_getDecimationValue(){
  return FILTER_FIR_DECIMATION_FACTOR;
}

// Returns the address of xQueue.
queue_t *filter_getXQueue(){
  return &xQueue;
}

// Returns the address of yQueue.
queue_t *filter_getYQueue(){
  return &yQueue;
}

// Returns the address of zQueue for a specific filter number.
queue_t *filter_getZQueue(uint16_t filterNumber){
  return &zQueue[filterNumber];
}

// Returns the address of the IIR output-queue for a specific filter-number.
queue_t *filter_getIirOutputQueue(uint16_t filterNumber){
  return &outputQueue[filterNumber];
}

// void filter_runTest();

#endif /* FILTER_H_ */