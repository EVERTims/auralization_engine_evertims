#ifndef REVERBTAIL_H_INCLUDED
#define REVERBTAIL_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "FilterBank.h"

#include <numeric>

#define MIN_DELAY_BETWEEN_TAIL_TAP 0.001f // minimum delay between reverb tail tap, in sec
#define MAX_DELAY_BETWEEN_TAIL_TAP 0.007f  // maximum delay between reverb tail tap, in sec
#define G60 1e-6 // 9.53674316e-7 = 1/(2^20) -> -60dB

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
    std::vector<IIRFilter> freqFilters;
    
    // local delay line
    DelayLine delayLine;
    bool reverbTailReadyToUse;
    
    // tail times
    // std::vector<float> tailTimes; // in seconds
    
    // tail
    std::vector< FIRFilter > tailNoises; // in seconds
    
    // audio buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> tailBuffer;

    // RT60 values
//    std::vector<float> slopesRT60;
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
//    Array<float> absorbanceRT60;
    
//==========================================================================
// METHODS
    
public:
    
ReverbTail() {
    // set to NUM_OCTAVE_BANDS rather than local numOctaveBands since anyway get methods
    // from OSCHandler will resize them (for "clarity's" sake here).
//    slopesRT60.resize( NUM_OCTAVE_BANDS, 0.0f );
    valuesRT60.resize( NUM_OCTAVE_BANDS, 0.0f );
//    absorbanceRT60.resize( numOctaveBands );
    
    tailNoises.resize( numOctaveBands );
    reverbTailReadyToUse = false;
}

~ReverbTail() {}

// local equivalent of prepareToPlay
void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // prepare buffers
    workingBuffer.setSize(1, samplesPerBlockExpected);
    workingBuffer.clear();
    tailBuffer = workingBuffer;

    // init delay line
    delayLine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    
    // init local frequency filters
    freqFilters.resize((1+1+2));
    float fc = 480;
    freqFilters[0].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
    freqFilters[0].reset(); // low
    freqFilters[2].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
    freqFilters[2].reset(); // mid
    
    fc = 8200;
    freqFilters[1].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
    freqFilters[1].reset(); // high
    freqFilters[3].setCoefficients( IIRCoefficients::makeLowPass( sampleRate, fc ) );
    freqFilters[3].reset(); // mid
    
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
    
//    // update tail decrease slope (based on rt60)
//    for (int j = 0; j < NUM_OCTAVE_BANDS; j++)
//    {
//        slopesRT60[j] = -60 / valuesRT60[j];
//    }
    
    // update tail distribution
    updateReverbTailDistribution();
    
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

    
AudioBuffer<float> getTailBuffer( AudioBuffer<float> source )
{
    
    // DBG(String("time , gain (for freq band 0, starting with last source image time / gain):"));
    // DBG(String(initDelay) + String(" , ") + String(initGain) );
    
//    // update delay line size if need be (TODO: MOVE THIS .SIZE() OUTSIDE OF AUDIO PROCESSING LOOP
//    if ( reverbTailReadyToUse )
//    {
//        // get maximum required delay line duration
//        float maxDelay = sourceImagesHandler.getMaxDelayFuture();
//        
//        // get associated required delay line buffer length
//        int updatedDelayLineLength = (int)( 1.5 * maxDelay * localSampleRate); // longest delay creates noisy sound if delay line is exactly 1* its duration
//        
//        // update delay line size
//        delayLine.setSize(updatedDelayLineLength);
//        
//        // unflag update required
//        reverbTailReadyToUse = false;
//    }
    
    // init
//    float delayInFractionalSamples = 0.0f;
//    float tapGain = 0.f;
    
    tailBuffer.clear();
    
    if( !reverbTailReadyToUse ) { return tailBuffer; }
    
    // decompose frequency banks (low)
    workingBuffer = source;
    freqFilters[0].processSamples(workingBuffer.getWritePointer(0), localSamplesPerBlockExpected);
    
    // convolve with reverb tail (low)
    tailNoises[0].process( workingBuffer.getWritePointer(0) );
    
    // add "now" to output
    tailBuffer.addFrom(0, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
    // add "future" to delayLine
//    delayLine.addFrom( workingBuffer, 0, localSamplesPerBlockExpected, workingBuffer.getNumSamples() - localSamplesPerBlockExpected );
    
    // do the same for medium and high
    
    // add "past-future" from delayLine ("localSamplesPerBlockExpected" samples away)
//    tailBuffer.addFrom(0, 0, delayLine.getChunk(localSamplesPerBlockExpected, localSamplesPerBlockExpected), 0, 0, localSamplesPerBlockExpected);
    
    // erase used delayLine (copy from null buffer)
//    workingBuffer.clear();
//    delayLine.copyFrom( workingBuffer, 0, 0, localSamplesPerBlockExpected );
    // increment delay line write position
//    delayLine.incrementWritePosition(localSamplesPerBlockExpected);
    
//        // tape in delay line
//        delayInFractionalSamples = (tailTimes[j] * localSampleRate);
//        workingBuffer = delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples);
//
//        // get tap gains
//        for (int k = 0; k < slopesRT60.size(); k++)
//        {
//            // negative slope: understand gainCurrent = gainInit - slope*(timeCurrent-timeInit)
//            // the "1 - ..." is to get an absorption, not a gain, to be used in filterbank processBuffer method
//            tapGain = slopesRT60[k] * ( tailTimes[j] - initDelay ); // dB
//            tapGain = initGain * pow(10, tapGain/10.f); // linear
//            absorbanceRT60.set(k, 1.0f - tapGain );
//            // DBG(String(tailTimes[j]) + String(" , ") + String(1.0 - absorbanceRT60[k]) );
//        }
    
        // DBG(String(tailTimes[j]) + String(" , ") + String(1.0 - absorbanceRT60[0]) );
        
        // apply freq-specific gains
//        filterBank.processBuffer( workingBuffer, absorbanceRT60, j );
    
        // write reverb tap to ambisonic channel W
//        tailBuffer.addFrom(0, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);

    
    return tailBuffer;
}

//void setFilterBankSize(int numFreqBands)
//{
//    filterBank.setNumFilters( numFreqBands, tailTimes.size() );
//}
    
private:
  
    
float getReverbTailIncrement()
{
    return ( MIN_DELAY_BETWEEN_TAIL_TAP + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(MAX_DELAY_BETWEEN_TAIL_TAP-MIN_DELAY_BETWEEN_TAIL_TAP))) );
}

float getRandom() // in [-1 1]
{
    return ( 1.f - 2.f * static_cast <float> (rand()) /( static_cast <float> (RAND_MAX) ) );
}

// compute new tail time distribution
void updateReverbTailDistribution()
{
    float maxDelayTail = getMaxValue(valuesRT60);
    std::vector<float> ir;
    
    // discard e.g. full absorbant material scenario
    if( maxDelayTail <= initDelay ){
//        ir.resize(0);
//        ir[0] = 0.f;
//        for( int m = 0; m < 3; m++ ) {
//            tailNoises[m].setImpulseResponse(ir.data());
//            tailNoises[m].init();
//            tailNoises[m].reset();
//        }
        return;
    }
        
    // NOTE!!!
    // look at the reset() method from FIR filter: they've got memory..? still need a local delayLine?
    
    // init
    float irMaxTime;
    float slope;
    float irMinTimeSample = initDelay * localSampleRate;
    
    // BUILD LOW
    // get RT60
    irMaxTime = 0.f;
    for( int m = 0; m < 5; m++ ) { irMaxTime += valuesRT60[m]; }
    irMaxTime = irMaxTime / 5.f;
    slope = -60 / irMaxTime;
    // fill IR
    int irSize = localSampleRate * irMaxTime;
    ir.resize( irSize );
    for( int m = 0; m < irSize; m++ ){
        if( m > irMinTimeSample ){
            ir[m] = initGain * getRandom() * pow(10,
                    slope * (localSampleRate * m - irMinTimeSample ) // current time
                    / 10.f // with pow, dB to lin conversion
                    );
        }
    }
    // store IR
    tailNoises[0].init( localSamplesPerBlockExpected, ir.size() );
    tailNoises[0].setImpulseResponse(ir.data());
    DBG( String("ir 0 duration / numSamples: ") + String(irMaxTime) + String(" / ")  + String(irSize) );

    
    // notify all filters have been correctly initialized
    reverbTailReadyToUse = true;
    
//    // set new time distribution size (to max number of values possible)
//    tailTimes.resize( (maxDelayTail - initDelay) / (MIN_DELAY_BETWEEN_TAIL_TAP * maxDelayTail), 0.f );
//    
//    // get reverb tail time distribution over [minTime - maxTime]
//    float time = initDelay + getReverbTailIncrement() * maxDelayTail;
//    int index = 0;
//    while( time < maxDelayTail )
//    {
//        tailTimes[index] = time;
//        index += 1;
//        time += getReverbTailIncrement() * maxDelayTail;
//    }
//    // remove zero at the end (the precaution taken above)
//    tailTimes.resize(index);

}

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbTail)
    
};

#endif // REVERBTAIL_H_INCLUDED
