#ifndef REVERBTAIL_H_INCLUDED
#define REVERBTAIL_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "FilterBank.h"

#include <numeric>

#define FDN_ORDER 16 // speed of sound in m.s-1

class ReverbTail
{

//==========================================================================
// ATTRIBUTES
    
public:

    std::vector<float> valuesRT60; // in sec
    float initGain = 0.0f; // S.I.
    float initDelay = 0.0f; // in sec
    
private:

    // octave filter bank
    // FilterBank filterBank;
    int numOctaveBands = 3;
    // std::vector<IIRFilter> freqFilters;
    
    // local delay line
    DelayLine delayLine;
    // bool reverbTailReadyToUse;
    
    // setup FDN (static FDN order of 16 is max for now)
    std::array<float, 16> fdnDelays; // in sec
    std::array<float, 16> fdnGains; // S.I.
    std::array< std::array < float, 16>, 16 > fdnFeedbackMatrix; // S.I.
    
    // audio buffers
    AudioBuffer<float> reverbBusBuffers; // working buffer
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> tailBuffer;

    // RT60 values
    // std::vector<float> slopesRT60;
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    // Array<float> absorbanceRT60;
    
//==========================================================================
// METHODS
    
public:
    
ReverbTail() {
    // set to NUM_OCTAVE_BANDS rather than local numOctaveBands since anyway get methods
    // from OSCHandler will resize them (for "clarity's" sake here).
//    slopesRT60.resize( NUM_OCTAVE_BANDS, 0.0f );
    
//    absorbanceRT60.resize( numOctaveBands );
    
    valuesRT60.resize( NUM_OCTAVE_BANDS, 0.0f );
    
    
    defineFdnFeedbackMatrix();
    updateFdnParameters();
    
    // tailNoises.resize( numOctaveBands );
    // reverbTailReadyToUse = false;
}

~ReverbTail() {}

// local equivalent of prepareToPlay
void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // prepare buffers
    reverbBusBuffers.setSize(FDN_ORDER, samplesPerBlockExpected);
    reverbBusBuffers.clear();
    
    workingBuffer.setSize(1, samplesPerBlockExpected);
    workingBuffer.clear();
    tailBuffer = workingBuffer;
    
    // init delay line
    delayLine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    delayLine.setSize(FDN_ORDER, 2*44100); // debug: set delay line max size
    
//    // init local frequency filters
//    freqFilters.resize((1+1+2));
//    float fc = 480;
//    freqFilters[0].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
//    freqFilters[0].reset(); // low
//    freqFilters[2].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
//    freqFilters[2].reset(); // mid
//    
//    fc = 8200;
//    freqFilters[1].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
//    freqFilters[1].reset(); // high
//    freqFilters[3].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
//    freqFilters[3].reset(); // mid
    
    // keep local copies
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
}

void updateInternals( std::vector<float> r60Values, float newInitGain, float newInitDelay )
{
    // store new values
    initDelay = newInitDelay;
    initGain = newInitGain;
    valuesRT60 = r60Values;
    
    // update FDN parameters
    updateFdnParameters();
    
//    // update tail decrease slope (based on rt60)
//    for (int j = 0; j < NUM_OCTAVE_BANDS; j++)
//    {
//        slopesRT60[j] = -60 / valuesRT60[j];
//    }
    
    // update tail distribution
//    updateReverbTailDistribution();
    
    // update filterbank
    // filterBank.setNumFilters( filterBank.getNumFilters(), tailTimes.size() );
    
    
    // DEBUG
//    DBG( String("first reverb tail time / gain : ") + String(initDelay) + String(" / ") + String(initGain) );
//    DBG( String("future reverb tail size: ") + String( tailTimes.size() ) );
//    DBG( String("(max reverb tail size: ") + String( (getMaxValue(valuesRT60) - newInitDelay) / MIN_DELAY_BETWEEN_TAIL_TAP ) + String(")") );
//    DBG( String("(min reverb tail size: ") + String( (getMaxValue(valuesRT60) - newInitDelay) / MAX_DELAY_BETWEEN_TAIL_TAP ) + String(")") );
//    DBG( String("reverb tail distribution (sec):") );
//    for (int j = 0; j < tailTimes.size(); j++) DBG(String(tailTimes[j]));
//    DBG( String("reverb tails slopes (gain/sec across freq bands):") );
//    for (int j = 0; j < slopesRT60.size(); j++) DBG(String(slopesRT60[j]));
}

// add source image to reverberation bus for latter use
void addToBus( int busId, AudioBuffer<float> source )
{
    reverbBusBuffers.addFrom(busId, 0, source, 0, 0, localSamplesPerBlockExpected);
}
    
// process reverb tail from bus tail, return reverb tail buffer
AudioBuffer<float> getTailBuffer()
{
    // loop over FDN bus to write direct input to / read output from delay line
    float delayInFractionalSamples;
    tailBuffer.clear();
    workingBuffer.clear();

     for (int fdnId = 0; fdnId < FDN_ORDER; fdnId++)
    {
        // clear delay line head (copy cleared buffer): necessary, to recursively use the delayLine addFrom afterwards
        delayLine.copyFrom( fdnId, workingBuffer, 0, 0, localSamplesPerBlockExpected );
        
        // write input to delay line
        delayLine.addFrom( fdnId, reverbBusBuffers, fdnId, 0, localSamplesPerBlockExpected );
    
        // read output from delay line (erase current content of reverbBuffers)
        delayInFractionalSamples = fdnDelays[fdnId] * localSampleRate;
        reverbBusBuffers.copyFrom(fdnId, 0,
                                  delayLine.getInterpolatedChunk(fdnId, localSamplesPerBlockExpected, delayInFractionalSamples),
                                  0, 0, localSamplesPerBlockExpected);
        
        // sum FDN to output
        tailBuffer.addFrom(0, 0, reverbBusBuffers, fdnId, 0, localSamplesPerBlockExpected);
        
        // apply FDN gains
        reverbBusBuffers.applyGain(fdnId, 0, localSamplesPerBlockExpected, fdnGains[fdnId]);
        
    }
    
    // write fdn outputs to delay lines (with cross-feedback matrix)
    // had to put this in a different loop to await for reverbBusBuffers fill
     for (int fdnId = 0; fdnId < FDN_ORDER; fdnId++)
    {
         for (int fdnFedId = 0; fdnFedId < FDN_ORDER; fdnFedId++)
        {
            // get fdnFedId output, apply cross-feedback gain
            workingBuffer.copyFrom(0, 0, reverbBusBuffers, fdnFedId, 0, localSamplesPerBlockExpected);
            workingBuffer.applyGain(fdnFeedbackMatrix[fdnId][fdnFedId]);
            
            
            // write to fdnId (delayLine)
            delayLine.addFrom(fdnId, workingBuffer, 0, 0, localSamplesPerBlockExpected );
        }
    }
    
    // increment delay line write position
    delayLine.incrementWritePosition(localSamplesPerBlockExpected);
    
    // clear reverb bus
    reverbBusBuffers.clear();
    
    return tailBuffer;

//    // DBG(String("time , gain (for freq band 0, starting with last source image time / gain):"));
//    // DBG(String(initDelay) + String(" , ") + String(initGain) );
//    
////    // update delay line size if need be (TODO: MOVE THIS .SIZE() OUTSIDE OF AUDIO PROCESSING LOOP
////    if ( reverbTailReadyToUse )
////    {
////        // get maximum required delay line duration
////        float maxDelay = sourceImagesHandler.getMaxDelayFuture();
////        
////        // get associated required delay line buffer length
////        int updatedDelayLineLength = (int)( 1.5 * maxDelay * localSampleRate); // longest delay creates noisy sound if delay line is exactly 1* its duration
////        
////        // update delay line size
////        delayLine.setSize(updatedDelayLineLength);
////        
////        // unflag update required
////        reverbTailReadyToUse = false;
////    }
//    
//    // init
////    float delayInFractionalSamples = 0.0f;
////    float tapGain = 0.f;
//    

//
////    if( !reverbTailReadyToUse ) { return tailBuffer; }
//    
//    // decompose frequency banks (low)
////    workingBuffer = source;
////    freqFilters[0].processSamples(workingBuffer.getWritePointer(0), localSamplesPerBlockExpected);
//    
//    // convolve with reverb tail (low)
////    tailNoises[0].process( workingBuffer.getWritePointer(0) );
//    
//    // add "now" to output
////    tailBuffer.addFrom(0, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
//    // add "future" to delayLine
////    delayLine.addFrom( workingBuffer, 0, localSamplesPerBlockExpected, workingBuffer.getNumSamples() - localSamplesPerBlockExpected );
//    
//    // do the same for medium and high
//    
//    // add "past-future" from delayLine ("localSamplesPerBlockExpected" samples away)
////    tailBuffer.addFrom(0, 0, delayLine.getChunk(localSamplesPerBlockExpected, localSamplesPerBlockExpected), 0, 0, localSamplesPerBlockExpected);
//    
//    // erase used delayLine (copy from null buffer)
////    workingBuffer.clear();
////    delayLine.copyFrom( workingBuffer, 0, 0, localSamplesPerBlockExpected );
//    // increment delay line write position
////    delayLine.incrementWritePosition(localSamplesPerBlockExpected);
//    
////        // tape in delay line
////        delayInFractionalSamples = (tailTimes[j] * localSampleRate);
////        workingBuffer = delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples);
////
////        // get tap gains
////        for (int k = 0; k < slopesRT60.size(); k++)
////        {
////            // negative slope: understand gainCurrent = gainInit - slope*(timeCurrent-timeInit)
////            // the "1 - ..." is to get an absorption, not a gain, to be used in filterbank processBuffer method
////            tapGain = slopesRT60[k] * ( tailTimes[j] - initDelay ); // dB
////            tapGain = initGain * pow(10, tapGain/10.f); // linear
////            absorbanceRT60.set(k, 1.0f - tapGain );
////            // DBG(String(tailTimes[j]) + String(" , ") + String(1.0 - absorbanceRT60[k]) );
////        }
//    
//        // DBG(String(tailTimes[j]) + String(" , ") + String(1.0 - absorbanceRT60[0]) );
//        
//        // apply freq-specific gains
////        filterBank.processBuffer( workingBuffer, absorbanceRT60, j );
//    
//        // write reverb tap to ambisonic channel W
////        tailBuffer.addFrom(0, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
//
//    
    
}

//void setFilterBankSize(int numFreqBands)
//{
//    filterBank.setNumFilters( numFreqBands, tailTimes.size() );
//}
    
private:
  
    void updateFdnParameters(){
        fdnDelays[0] = 0.011995;
        fdnDelays[1] = 0.019070;
        fdnDelays[2] = 0.021791;
        fdnDelays[3] = 0.031043;
        fdnDelays[4] = 0.038118;
        fdnDelays[5] = 0.041927;
        fdnDelays[6] = 0.050091;
        fdnDelays[7] = 0.063696;
        fdnDelays[8] = 0.078934;
        fdnDelays[9] = 0.084376;
        fdnDelays[10] = 0.101791;
        fdnDelays[11] = 0.114308;
        fdnDelays[12] = 0.120839;
        fdnDelays[13] = 0.141519;
        fdnDelays[14] = 0.156213;
        fdnDelays[15] = 0.179615;

        fdnGains[0] = 0.935;
        fdnGains[1] = 0.899;
        fdnGains[2] = 0.885;
        fdnGains[3] = 0.840;
        fdnGains[4] = 0.808;
        fdnGains[5] = 0.790;
        fdnGains[6] = 0.755;
        fdnGains[7] = 0.700;
        fdnGains[8] = 0.642;
        fdnGains[9] = 0.623;
        fdnGains[10] = 0.565;
        fdnGains[11] = 0.527;
        fdnGains[12] = 0.508;
        fdnGains[13] = 0.452;
        fdnGains[14] = 0.416;
        fdnGains[15] = 0.365;
    }

    
//float getReverbTailIncrement()
//{
//    return ( MIN_DELAY_BETWEEN_TAIL_TAP + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(MAX_DELAY_BETWEEN_TAIL_TAP-MIN_DELAY_BETWEEN_TAIL_TAP))) );
//}

//float getRandom() // in [-1 1]
//{
//    return ( 1.f - 2.f * static_cast <float> (rand()) /( static_cast <float> (RAND_MAX) ) );
//}

//// compute new tail time distribution
//void updateReverbTailDistribution()
//{
//    float maxDelayTail = getMaxValue(valuesRT60);
//    std::vector<float> ir;
//    
//    // discard e.g. full absorbant material scenario
//    if( maxDelayTail <= initDelay ){
////        ir.resize(0);
////        ir[0] = 0.f;
////        for( int m = 0; m < 3; m++ ) {
////            tailNoises[m].setImpulseResponse(ir.data());
////            tailNoises[m].init();
////            tailNoises[m].reset();
////        }
//        return;
//    }
//        
//    // NOTE!!!
//    // look at the reset() method from FIR filter: they've got memory..? still need a local delayLine?
//    
//    // init
//    float irMaxTime;
//    float slope;
//    float irMinTimeSample = initDelay * localSampleRate;
//    
//    // BUILD LOW
//    // get RT60
//    irMaxTime = 0.f;
//    for( int m = 0; m < 5; m++ ) { irMaxTime += valuesRT60[m]; }
//    irMaxTime = irMaxTime / 5.f;
//    slope = -60 / irMaxTime;
//    // fill IR
//    int irSize = localSampleRate * irMaxTime;
//    ir.resize( irSize );
//    for( int m = 0; m < irSize; m++ ){
//        if( m > irMinTimeSample ){
//            ir[m] = initGain * getRandom() * pow(10,
//                    slope * (localSampleRate * m - irMinTimeSample ) // current time
//                    / 10.f // with pow, dB to lin conversion
//                    );
//        }
//    }
//    // store IR
//    tailNoises[0].init( localSamplesPerBlockExpected, ir.size() );
//    tailNoises[0].setImpulseResponse(ir.data());
//    DBG( String("ir 0 duration / numSamples: ") + String(irMaxTime) + String(" / ")  + String(irSize) );
//
//    
//    // notify all filters have been correctly initialized
//    reverbTailReadyToUse = true;
//    
////    // set new time distribution size (to max number of values possible)
////    tailTimes.resize( (maxDelayTail - initDelay) / (MIN_DELAY_BETWEEN_TAIL_TAP * maxDelayTail), 0.f );
////    
////    // get reverb tail time distribution over [minTime - maxTime]
////    float time = initDelay + getReverbTailIncrement() * maxDelayTail;
////    int index = 0;
////    while( time < maxDelayTail )
////    {
////        tailTimes[index] = time;
////        index += 1;
////        time += getReverbTailIncrement() * maxDelayTail;
////    }
////    // remove zero at the end (the precaution taken above)
////    tailTimes.resize(index);
//
//}

// direct copy of output of hadamar matrix as defined in reverbTailv3.m file
void defineFdnFeedbackMatrix(){
    fdnFeedbackMatrix[0][0] = 0.250;
    fdnFeedbackMatrix[0][1] = 0.250;
    fdnFeedbackMatrix[0][2] = 0.250;
    fdnFeedbackMatrix[0][3] = 0.250;
    fdnFeedbackMatrix[0][4] = 0.250;
    fdnFeedbackMatrix[0][5] = 0.250;
    fdnFeedbackMatrix[0][6] = 0.250;
    fdnFeedbackMatrix[0][7] = 0.250;
    fdnFeedbackMatrix[0][8] = 0.250;
    fdnFeedbackMatrix[0][9] = 0.250;
    fdnFeedbackMatrix[0][10] = 0.250;
    fdnFeedbackMatrix[0][11] = 0.250;
    fdnFeedbackMatrix[0][12] = 0.250;
    fdnFeedbackMatrix[0][13] = 0.250;
    fdnFeedbackMatrix[0][14] = 0.250;
    fdnFeedbackMatrix[0][15] = 0.250;
    fdnFeedbackMatrix[1][0] = 0.250;
    fdnFeedbackMatrix[1][1] = -0.250;
    fdnFeedbackMatrix[1][2] = 0.250;
    fdnFeedbackMatrix[1][3] = -0.250;
    fdnFeedbackMatrix[1][4] = 0.250;
    fdnFeedbackMatrix[1][5] = -0.250;
    fdnFeedbackMatrix[1][6] = 0.250;
    fdnFeedbackMatrix[1][7] = -0.250;
    fdnFeedbackMatrix[1][8] = 0.250;
    fdnFeedbackMatrix[1][9] = -0.250;
    fdnFeedbackMatrix[1][10] = 0.250;
    fdnFeedbackMatrix[1][11] = -0.250;
    fdnFeedbackMatrix[1][12] = 0.250;
    fdnFeedbackMatrix[1][13] = -0.250;
    fdnFeedbackMatrix[1][14] = 0.250;
    fdnFeedbackMatrix[1][15] = -0.250;
    fdnFeedbackMatrix[2][0] = 0.250;
    fdnFeedbackMatrix[2][1] = 0.250;
    fdnFeedbackMatrix[2][2] = -0.250;
    fdnFeedbackMatrix[2][3] = -0.250;
    fdnFeedbackMatrix[2][4] = 0.250;
    fdnFeedbackMatrix[2][5] = 0.250;
    fdnFeedbackMatrix[2][6] = -0.250;
    fdnFeedbackMatrix[2][7] = -0.250;
    fdnFeedbackMatrix[2][8] = 0.250;
    fdnFeedbackMatrix[2][9] = 0.250;
    fdnFeedbackMatrix[2][10] = -0.250;
    fdnFeedbackMatrix[2][11] = -0.250;
    fdnFeedbackMatrix[2][12] = 0.250;
    fdnFeedbackMatrix[2][13] = 0.250;
    fdnFeedbackMatrix[2][14] = -0.250;
    fdnFeedbackMatrix[2][15] = -0.250;
    fdnFeedbackMatrix[3][0] = 0.250;
    fdnFeedbackMatrix[3][1] = -0.250;
    fdnFeedbackMatrix[3][2] = -0.250;
    fdnFeedbackMatrix[3][3] = 0.250;
    fdnFeedbackMatrix[3][4] = 0.250;
    fdnFeedbackMatrix[3][5] = -0.250;
    fdnFeedbackMatrix[3][6] = -0.250;
    fdnFeedbackMatrix[3][7] = 0.250;
    fdnFeedbackMatrix[3][8] = 0.250;
    fdnFeedbackMatrix[3][9] = -0.250;
    fdnFeedbackMatrix[3][10] = -0.250;
    fdnFeedbackMatrix[3][11] = 0.250;
    fdnFeedbackMatrix[3][12] = 0.250;
    fdnFeedbackMatrix[3][13] = -0.250;
    fdnFeedbackMatrix[3][14] = -0.250;
    fdnFeedbackMatrix[3][15] = 0.250;
    fdnFeedbackMatrix[4][0] = 0.250;
    fdnFeedbackMatrix[4][1] = 0.250;
    fdnFeedbackMatrix[4][2] = 0.250;
    fdnFeedbackMatrix[4][3] = 0.250;
    fdnFeedbackMatrix[4][4] = -0.250;
    fdnFeedbackMatrix[4][5] = -0.250;
    fdnFeedbackMatrix[4][6] = -0.250;
    fdnFeedbackMatrix[4][7] = -0.250;
    fdnFeedbackMatrix[4][8] = 0.250;
    fdnFeedbackMatrix[4][9] = 0.250;
    fdnFeedbackMatrix[4][10] = 0.250;
    fdnFeedbackMatrix[4][11] = 0.250;
    fdnFeedbackMatrix[4][12] = -0.250;
    fdnFeedbackMatrix[4][13] = -0.250;
    fdnFeedbackMatrix[4][14] = -0.250;
    fdnFeedbackMatrix[4][15] = -0.250;
    fdnFeedbackMatrix[5][0] = 0.250;
    fdnFeedbackMatrix[5][1] = -0.250;
    fdnFeedbackMatrix[5][2] = 0.250;
    fdnFeedbackMatrix[5][3] = -0.250;
    fdnFeedbackMatrix[5][4] = -0.250;
    fdnFeedbackMatrix[5][5] = 0.250;
    fdnFeedbackMatrix[5][6] = -0.250;
    fdnFeedbackMatrix[5][7] = 0.250;
    fdnFeedbackMatrix[5][8] = 0.250;
    fdnFeedbackMatrix[5][9] = -0.250;
    fdnFeedbackMatrix[5][10] = 0.250;
    fdnFeedbackMatrix[5][11] = -0.250;
    fdnFeedbackMatrix[5][12] = -0.250;
    fdnFeedbackMatrix[5][13] = 0.250;
    fdnFeedbackMatrix[5][14] = -0.250;
    fdnFeedbackMatrix[5][15] = 0.250;
    fdnFeedbackMatrix[6][0] = 0.250;
    fdnFeedbackMatrix[6][1] = 0.250;
    fdnFeedbackMatrix[6][2] = -0.250;
    fdnFeedbackMatrix[6][3] = -0.250;
    fdnFeedbackMatrix[6][4] = -0.250;
    fdnFeedbackMatrix[6][5] = -0.250;
    fdnFeedbackMatrix[6][6] = 0.250;
    fdnFeedbackMatrix[6][7] = 0.250;
    fdnFeedbackMatrix[6][8] = 0.250;
    fdnFeedbackMatrix[6][9] = 0.250;
    fdnFeedbackMatrix[6][10] = -0.250;
    fdnFeedbackMatrix[6][11] = -0.250;
    fdnFeedbackMatrix[6][12] = -0.250;
    fdnFeedbackMatrix[6][13] = -0.250;
    fdnFeedbackMatrix[6][14] = 0.250;
    fdnFeedbackMatrix[6][15] = 0.250;
    fdnFeedbackMatrix[7][0] = 0.250;
    fdnFeedbackMatrix[7][1] = -0.250;
    fdnFeedbackMatrix[7][2] = -0.250;
    fdnFeedbackMatrix[7][3] = 0.250;
    fdnFeedbackMatrix[7][4] = -0.250;
    fdnFeedbackMatrix[7][5] = 0.250;
    fdnFeedbackMatrix[7][6] = 0.250;
    fdnFeedbackMatrix[7][7] = -0.250;
    fdnFeedbackMatrix[7][8] = 0.250;
    fdnFeedbackMatrix[7][9] = -0.250;
    fdnFeedbackMatrix[7][10] = -0.250;
    fdnFeedbackMatrix[7][11] = 0.250;
    fdnFeedbackMatrix[7][12] = -0.250;
    fdnFeedbackMatrix[7][13] = 0.250;
    fdnFeedbackMatrix[7][14] = 0.250;
    fdnFeedbackMatrix[7][15] = -0.250;
    fdnFeedbackMatrix[8][0] = 0.250;
    fdnFeedbackMatrix[8][1] = 0.250;
    fdnFeedbackMatrix[8][2] = 0.250;
    fdnFeedbackMatrix[8][3] = 0.250;
    fdnFeedbackMatrix[8][4] = 0.250;
    fdnFeedbackMatrix[8][5] = 0.250;
    fdnFeedbackMatrix[8][6] = 0.250;
    fdnFeedbackMatrix[8][7] = 0.250;
    fdnFeedbackMatrix[8][8] = -0.250;
    fdnFeedbackMatrix[8][9] = -0.250;
    fdnFeedbackMatrix[8][10] = -0.250;
    fdnFeedbackMatrix[8][11] = -0.250;
    fdnFeedbackMatrix[8][12] = -0.250;
    fdnFeedbackMatrix[8][13] = -0.250;
    fdnFeedbackMatrix[8][14] = -0.250;
    fdnFeedbackMatrix[8][15] = -0.250;
    fdnFeedbackMatrix[9][0] = 0.250;
    fdnFeedbackMatrix[9][1] = -0.250;
    fdnFeedbackMatrix[9][2] = 0.250;
    fdnFeedbackMatrix[9][3] = -0.250;
    fdnFeedbackMatrix[9][4] = 0.250;
    fdnFeedbackMatrix[9][5] = -0.250;
    fdnFeedbackMatrix[9][6] = 0.250;
    fdnFeedbackMatrix[9][7] = -0.250;
    fdnFeedbackMatrix[9][8] = -0.250;
    fdnFeedbackMatrix[9][9] = 0.250;
    fdnFeedbackMatrix[9][10] = -0.250;
    fdnFeedbackMatrix[9][11] = 0.250;
    fdnFeedbackMatrix[9][12] = -0.250;
    fdnFeedbackMatrix[9][13] = 0.250;
    fdnFeedbackMatrix[9][14] = -0.250;
    fdnFeedbackMatrix[9][15] = 0.250;
    fdnFeedbackMatrix[10][0] = 0.250;
    fdnFeedbackMatrix[10][1] = 0.250;
    fdnFeedbackMatrix[10][2] = -0.250;
    fdnFeedbackMatrix[10][3] = -0.250;
    fdnFeedbackMatrix[10][4] = 0.250;
    fdnFeedbackMatrix[10][5] = 0.250;
    fdnFeedbackMatrix[10][6] = -0.250;
    fdnFeedbackMatrix[10][7] = -0.250;
    fdnFeedbackMatrix[10][8] = -0.250;
    fdnFeedbackMatrix[10][9] = -0.250;
    fdnFeedbackMatrix[10][10] = 0.250;
    fdnFeedbackMatrix[10][11] = 0.250;
    fdnFeedbackMatrix[10][12] = -0.250;
    fdnFeedbackMatrix[10][13] = -0.250;
    fdnFeedbackMatrix[10][14] = 0.250;
    fdnFeedbackMatrix[10][15] = 0.250;
    fdnFeedbackMatrix[11][0] = 0.250;
    fdnFeedbackMatrix[11][1] = -0.250;
    fdnFeedbackMatrix[11][2] = -0.250;
    fdnFeedbackMatrix[11][3] = 0.250;
    fdnFeedbackMatrix[11][4] = 0.250;
    fdnFeedbackMatrix[11][5] = -0.250;
    fdnFeedbackMatrix[11][6] = -0.250;
    fdnFeedbackMatrix[11][7] = 0.250;
    fdnFeedbackMatrix[11][8] = -0.250;
    fdnFeedbackMatrix[11][9] = 0.250;
    fdnFeedbackMatrix[11][10] = 0.250;
    fdnFeedbackMatrix[11][11] = -0.250;
    fdnFeedbackMatrix[11][12] = -0.250;
    fdnFeedbackMatrix[11][13] = 0.250;
    fdnFeedbackMatrix[11][14] = 0.250;
    fdnFeedbackMatrix[11][15] = -0.250;
    fdnFeedbackMatrix[12][0] = 0.250;
    fdnFeedbackMatrix[12][1] = 0.250;
    fdnFeedbackMatrix[12][2] = 0.250;
    fdnFeedbackMatrix[12][3] = 0.250;
    fdnFeedbackMatrix[12][4] = -0.250;
    fdnFeedbackMatrix[12][5] = -0.250;
    fdnFeedbackMatrix[12][6] = -0.250;
    fdnFeedbackMatrix[12][7] = -0.250;
    fdnFeedbackMatrix[12][8] = -0.250;
    fdnFeedbackMatrix[12][9] = -0.250;
    fdnFeedbackMatrix[12][10] = -0.250;
    fdnFeedbackMatrix[12][11] = -0.250;
    fdnFeedbackMatrix[12][12] = 0.250;
    fdnFeedbackMatrix[12][13] = 0.250;
    fdnFeedbackMatrix[12][14] = 0.250;
    fdnFeedbackMatrix[12][15] = 0.250;
    fdnFeedbackMatrix[13][0] = 0.250;
    fdnFeedbackMatrix[13][1] = -0.250;
    fdnFeedbackMatrix[13][2] = 0.250;
    fdnFeedbackMatrix[13][3] = -0.250;
    fdnFeedbackMatrix[13][4] = -0.250;
    fdnFeedbackMatrix[13][5] = 0.250;
    fdnFeedbackMatrix[13][6] = -0.250;
    fdnFeedbackMatrix[13][7] = 0.250;
    fdnFeedbackMatrix[13][8] = -0.250;
    fdnFeedbackMatrix[13][9] = 0.250;
    fdnFeedbackMatrix[13][10] = -0.250;
    fdnFeedbackMatrix[13][11] = 0.250;
    fdnFeedbackMatrix[13][12] = 0.250;
    fdnFeedbackMatrix[13][13] = -0.250;
    fdnFeedbackMatrix[13][14] = 0.250;
    fdnFeedbackMatrix[13][15] = -0.250;
    fdnFeedbackMatrix[14][0] = 0.250;
    fdnFeedbackMatrix[14][1] = 0.250;
    fdnFeedbackMatrix[14][2] = -0.250;
    fdnFeedbackMatrix[14][3] = -0.250;
    fdnFeedbackMatrix[14][4] = -0.250;
    fdnFeedbackMatrix[14][5] = -0.250;
    fdnFeedbackMatrix[14][6] = 0.250;
    fdnFeedbackMatrix[14][7] = 0.250;
    fdnFeedbackMatrix[14][8] = -0.250;
    fdnFeedbackMatrix[14][9] = -0.250;
    fdnFeedbackMatrix[14][10] = 0.250;
    fdnFeedbackMatrix[14][11] = 0.250;
    fdnFeedbackMatrix[14][12] = 0.250;
    fdnFeedbackMatrix[14][13] = 0.250;
    fdnFeedbackMatrix[14][14] = -0.250;
    fdnFeedbackMatrix[14][15] = -0.250;
    fdnFeedbackMatrix[15][0] = 0.250;
    fdnFeedbackMatrix[15][1] = -0.250;
    fdnFeedbackMatrix[15][2] = -0.250;
    fdnFeedbackMatrix[15][3] = 0.250;
    fdnFeedbackMatrix[15][4] = -0.250;
    fdnFeedbackMatrix[15][5] = 0.250;
    fdnFeedbackMatrix[15][6] = 0.250;
    fdnFeedbackMatrix[15][7] = -0.250;
    fdnFeedbackMatrix[15][8] = -0.250;
    fdnFeedbackMatrix[15][9] = 0.250;
    fdnFeedbackMatrix[15][10] = 0.250;
    fdnFeedbackMatrix[15][11] = -0.250;
    fdnFeedbackMatrix[15][12] = 0.250;
    fdnFeedbackMatrix[15][13] = -0.250;
    fdnFeedbackMatrix[15][14] = -0.250;
    fdnFeedbackMatrix[15][15] = 0.250;
}
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbTail)
    
};

#endif // REVERBTAIL_H_INCLUDED
