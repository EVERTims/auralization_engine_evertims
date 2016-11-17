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
    FilterBank filterBank;
    
    // tail times
    std::vector<float> tailTimes; // in seconds
    
    // audio buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> tailBuffer;

    // RT60 values
    std::vector<float> slopesRT60;
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    Array<float> absorbanceRT60;
    
//==========================================================================
// METHODS
    
public:
    
ReverbTail() {
    slopesRT60.resize( NUM_OCTAVE_BANDS, 0.0f );
    valuesRT60.resize( NUM_OCTAVE_BANDS, 0.0f );
    absorbanceRT60.resize( NUM_OCTAVE_BANDS );
}

~ReverbTail() {}

// local equivalent of prepareToPlay
void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // prepare buffers
    workingBuffer.setSize(1, samplesPerBlockExpected);
    workingBuffer.clear();
    tailBuffer = workingBuffer;

    // keep local copies
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;

    // init filter bank
    filterBank.prepareToPlay( samplesPerBlockExpected, sampleRate );
    filterBank.setNumFilters( NUM_OCTAVE_BANDS, tailTimes.size() );
}

void updateInternals( std::vector<float> r60Values, float newInitGain, float newInitDelay )
{
    // store new values
    initDelay = newInitDelay;
    initGain = newInitGain;
    valuesRT60 = r60Values;
    
    // update tail decrease slope (based on rt60)
    for (int j = 0; j < NUM_OCTAVE_BANDS; j++)
    {
        slopesRT60[j] = -60 / valuesRT60[j];
    }
    
    // update tail distribution
    updateReverbTailDistribution();
    
    // update filterbank
    filterBank.setNumFilters( filterBank.getNumFilters(), tailTimes.size() );
    
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

    
AudioBuffer<float> getTailBuffer( DelayLine* delayLine )
{
    
    // DBG(String("time , gain (for freq band 0, starting with last source image time / gain):"));
    // DBG(String(initDelay) + String(" , ") + String(initGain) );
    
    // init
    tailBuffer.clear();
    float delayInFractionalSamples = 0.0f;
    float tapGain = 0.f;

    // for each reverb tail tap
    for (int j = 0; j < tailTimes.size(); j++)
    {

        // tape in delay line
        delayInFractionalSamples = (tailTimes[j] * localSampleRate);
        workingBuffer = delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples);

        // get tap gains
        for (int k = 0; k < slopesRT60.size(); k++)
        {
            // negative slope: understand gainCurrent = gainInit - slope*(timeCurrent-timeInit)
            // the "1 - ..." is to get an absorption, not a gain, to be used in filterbank processBuffer method
            tapGain = slopesRT60[k] * ( tailTimes[j] - initDelay ); // dB
            tapGain = initGain * pow(10, tapGain/10.f); // linear
            absorbanceRT60.set(k, 1.0f - tapGain );
            // DBG(String(tailTimes[j]) + String(" , ") + String(1.0 - absorbanceRT60[k]) );
        }
        
        // DBG(String(tailTimes[j]) + String(" , ") + String(1.0 - absorbanceRT60[0]) );
        
        // apply freq-specific gains
        filterBank.processBuffer( workingBuffer, absorbanceRT60, j );
        
        // write reverb tap to ambisonic channel W
        tailBuffer.addFrom(0, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
    }
    
    return tailBuffer;
}

void setFilterBankSize(int numFreqBands)
{
    filterBank.setNumFilters( numFreqBands, tailTimes.size() );
}
    
private:
  
    
float getReverbTailIncrement()
{
    return ( MIN_DELAY_BETWEEN_TAIL_TAP + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(MAX_DELAY_BETWEEN_TAIL_TAP-MIN_DELAY_BETWEEN_TAIL_TAP))) );
}

// compute new tail time distribution
void updateReverbTailDistribution()
{
    float maxDelayTail = getMaxValue(valuesRT60);
    if( maxDelayTail > initDelay ) // discard e.g. full absorbant material scenario
    {
        // set new time distribution size (to max number of values possible)
        tailTimes.resize( (maxDelayTail - initDelay) / (MIN_DELAY_BETWEEN_TAIL_TAP * maxDelayTail), 0.f );
        
        // get reverb tail time distribution over [minTime - maxTime]
        float time = initDelay + getReverbTailIncrement() * maxDelayTail;
        int index = 0;
        while( time < maxDelayTail )
        {
            tailTimes[index] = time;
            index += 1;
            time += getReverbTailIncrement() * maxDelayTail;
        }
        // remove zero at the end (the precaution taken above)
        tailTimes.resize(index);
    }
    else{ tailTimes.resize(0); }
}

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbTail)
    
};

#endif // REVERBTAIL_H_INCLUDED
