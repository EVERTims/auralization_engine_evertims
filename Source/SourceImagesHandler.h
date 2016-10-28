#ifndef SOURCEIMAGESHANDLER_H_INCLUDED
#define SOURCEIMAGESHANDLER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "AmbixEncode/AmbixEncoder.h"
#include "FilterBank.h"

#include <numeric>

#define MIN_DELAY_BETWEEN_TAIL_TAP 0.001f // minimum delay between reverb tail tap, in sec
#define MAX_DELAY_BETWEEN_TAIL_TAP 0.005f  // maximum delay between reverb tail tap, in sec

class SourceImagesHandler
{

//==========================================================================
// ATTRIBUTES
    
public:
    
    // sources images
    std::vector<float> IDs;
    std::vector<float> delaysCurrent; // in seconds
    std::vector<float> delaysFuture; // in seconds
    std::vector<float> pathLengths; // in meters
    int numSourceImages;
    
    // octave filter bank
    FilterBank filterBank;
    FilterBank filterBankTail;
    FilterBank filterBankRec;
    
    // reverb tail
    std::vector<float> valuesRT60Future; // in seconds
    std::vector<float> valuesRT60Current; // in seconds
    std::vector<float> slopesRT60;
    Array<float> gainsRT60;
    std::vector<float> tailTimesCurrent; // in seconds
    std::vector<float> tailTimesFuture; // in seconds
    bool enableReverbTail;
    
private:
    
    // audio buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> workingBufferTemp; // 2nd working buffer, e.g. for crossfade mecanism
    AudioBuffer<float> clipboardBuffer; // to be used as local copy of working buffer when working buffer modified in loops
    
    AudioBuffer<float> irRecWorkingBuffer; // used for IR recording
    AudioBuffer<float> irRecWorkingBufferTemp; // used for IR recording
    AudioBuffer<float> irRecAmbisonicBuffer; // used for IR recording
    AudioBuffer<float> irRecClipboardBuffer;
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    
    // crossfade mecanism
    float crossfadeGain = 0.0;
    bool crossfadeOver = true;
    
    // octave filter bank
    std::vector< Array<float> > absorptionCoefs; // buffer for input data
    
    // ambisonic encoding
    AmbixEncoder ambisonicEncoder;
    std::vector< Array<float> > ambisonicGainsCurrent; // buffer for input data
    std::vector< Array<float> > ambisonicGainsFuture; // to avoid zipper effect
    AudioBuffer<float> ambisonicBuffer; // output buffer, N (Ambisonic) channels
    
    
//==========================================================================
// METHODS
    
public:
    
SourceImagesHandler()
{
    // these values are here rather than in prepare to play since they're not (directly) concerned
    // with the number of absorption bands
    valuesRT60Current.resize(10, 0.0f);
    valuesRT60Future.resize(10, 0.0f);
    slopesRT60.resize(10, 0.0f);
    gainsRT60.resize(10);
}

~SourceImagesHandler() {}

// local equivalent of prepareToPlay
void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // set number of valid source image
    numSourceImages = 0;
    
    // prepare buffers
    workingBuffer.setSize(1, samplesPerBlockExpected);
    workingBuffer.clear();
    workingBufferTemp = workingBuffer;
    clipboardBuffer = workingBuffer;
    
    ambisonicBuffer.setSize(N_AMBI_CH, samplesPerBlockExpected);
    
    // keep local copies
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
    
    // init filter bank
    filterBank.prepareToPlay( samplesPerBlockExpected, sampleRate );
    filterBank.setNumFilters( 3, IDs.size() );
    
    // init reverb tail filter bank
    filterBankTail.prepareToPlay( samplesPerBlockExpected, sampleRate );
    filterBankTail.setNumFilters( 3, tailTimesCurrent.size() );
    
    
    // init IR recording filter bank (need a separate, see comment on continuous data stream in FilterBank class)
    filterBankRec.prepareToPlay( samplesPerBlockExpected, sampleRate );
    filterBankRec.setNumFilters( 10, IDs.size() ); // may as well get the best quality for recording IR
}
    
// get max source image delay in seconds
float getMaxDelayFuture()
{
    float maxDelayTap = getMaxValue(delaysFuture);
    if( enableReverbTail ){
        float maxDelayTail = getMaxValue(valuesRT60Future);
        return fmax(maxDelayTap, maxDelayTail);
    }
    else{ return maxDelayTap; }
}
    
// main: loop over sources images, apply delay + room coloration + spatialization
AudioBuffer<float> getNextAudioBlock (DelayLine* delayLine)
{
    
    // update crossfade mecanism
    updateCrossfade();
    
    // clear output buffer (since used as cumulative buffer, iteratively summing sources images buffers)
    ambisonicBuffer.clear();
    
    // loop over sources images
    for( int j = 0; j < numSourceImages; j++ )
    {
        
        //==========================================================================
        // GET DELAYED BUFFER
        float delayInFractionalSamples = 0.0;
        if( !crossfadeOver ) // Add old and new tapped delayed buffers with gain crossfade
        {
            // get old delay, tap from delay line, apply gain=f(delay)
            delayInFractionalSamples = delaysCurrent[j] * localSampleRate;
            workingBuffer.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
            workingBuffer.applyGain(1.0 - crossfadeGain);
            
            // get new delay, tap from delay line, apply gain=f(delay)
            delayInFractionalSamples = delaysFuture[j] * localSampleRate;
            workingBufferTemp.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
            workingBufferTemp.applyGain(crossfadeGain);
            
            // add both buffers
            workingBuffer.addFrom(0, 0, workingBufferTemp, 0, 0, localSamplesPerBlockExpected);
        }
        else // simple update
        {
            // get delay, tap from delay line
            delayInFractionalSamples = (delaysCurrent[j] * localSampleRate);
            workingBuffer.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
        }
        
        //==========================================================================
        // APPLY GAIN BASED ON SOURCE IMAGE PATH LENGTH
        float gainDelayLine = fmin( 1.0, fmax( 0.0, 1.0/pathLengths[j] ));
        workingBuffer.applyGain(gainDelayLine);
        
        //==========================================================================
        // APPLY ABSORPTION COEFFICIENTS (octave filer bank decomposition)
        filterBank.processBuffer( workingBuffer, absorptionCoefs[j], j );
        
        //==========================================================================
        // ADD SOURCE IMAGE REVERB TAIL
        
        // get tap time distribution
        // create taps as 1st order ambisonic
        // do not forget zipper effect in all that
        
        //==========================================================================
        // AMBISONIC ENCODING
        
        // keep local copy since working buffer will be used to store ambisonic buffers
        clipboardBuffer = workingBuffer;
        
        for( int k = 0; k < N_AMBI_CH; k++ )
        {
            // create working copy
            workingBuffer = clipboardBuffer;
            
            if( !crossfadeOver )
            {
                // create 2nd working copy
                workingBufferTemp = clipboardBuffer;
                
                // apply ambisonic gain past
                workingBuffer.applyGain((1.0 - crossfadeGain)*ambisonicGainsCurrent[j][k]);
                
                // apply ambisonic gain future
                workingBufferTemp.applyGain(crossfadeGain*ambisonicGainsFuture[j][k]);
                
                // add past / future buffers
                workingBuffer.addFrom(0, 0, workingBufferTemp, 0, 0, localSamplesPerBlockExpected);
            }
            else
            {
                // apply ambisonic gain
                workingBuffer.applyGain(ambisonicGainsCurrent[j][k]);
            }
            
            // iteratively fill in general ambisonic buffer with source image buffers (cumulative)
            ambisonicBuffer.addFrom(k, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
        }
        
    }
    
    //==========================================================================
    // ADD GENERAL REVERB TAIL
    
    // TODO:
    //      • add crossfade
    //      • double check RT60 values and usage if correct
    //      • add reverb tail to save IR method
    
    if( enableReverbTail )
    {
        // get init tail gain / time
        float tailInitGain = fmin( 1.0, fmax( 0.0, 1.0/getMaxValue(pathLengths) ));
        float tailInitDelay = getMaxValue(delaysCurrent);
        
        // get tail decrease slope (based on rt60)
        for (int j = 0; j < slopesRT60.size(); j++)
        {
            slopesRT60[j] = tailInitGain * ((9.53674316e-7)-1) / valuesRT60Current[j]; // 1/(2^20) -> -60dB
        }
        
        // DBG(String("time , gain:"));
        // DBG(String(tailInitDelay) + String(" , ") + String(tailInitGain) );
        
        // for each reverb tail tap
        float delayInFractionalSamples = 0.0f;
        for (int j = 0; j < tailTimesCurrent.size(); j++)
        {
            // tape in delay line
            delayInFractionalSamples = (tailTimesCurrent[j] * localSampleRate);
            workingBuffer.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
            
            // get tap gains
            for (int k = 0; k < slopesRT60.size(); k++)
            {
                // negative slope: understand gainCurrent = gainInit - slope*(timeCurrent-timeInit)
                // the "1 - ..." is to get an absorption, not a gain, to be used in filterbank processBuffer method 
                gainsRT60.set(k, 1.0f - (tailInitGain + slopesRT60[k] * ( tailTimesCurrent[j] - tailInitDelay )) );
                // DBG(String(tailTimesCurrent[j]) + String(" , ") + String(gainsRT60[k]) );
            }
            
            // DBG(String(tailTimesCurrent[j]) + String(" , ") + String(gainsRT60[0]) );
            
            // apply freq-specific gains
            filterBankTail.processBuffer( workingBuffer, gainsRT60, j );
            // workingBuffer.applyGain(gainsRT60[0]);
            
            // write reverb tap to ambisonic channel W
            ambisonicBuffer.addFrom(0, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
        }
        
        
        
    }
    
    return ambisonicBuffer;
}
    
// update local attributes based on latest received OSC info
void updateFromOscHandler(OSCHandler& oscHandler)
{
    // lame mecanism to avoid changing future gains if crossfade not over yet
    while(!crossfadeOver) sleep(0.001);
    
    // make sure not to use non-valid source image ID in audio thread during update
    auto IDsTemp = oscHandler.getSourceImageIDs();
    numSourceImages = min(IDs.size(), IDsTemp.size());
    
    // save new source image data, ready to be used in next audio loop
    IDs = oscHandler.getSourceImageIDs();
    delaysFuture = oscHandler.getSourceImageDelays();
    delaysCurrent.resize(delaysFuture.size(), 0.0f);
    pathLengths = oscHandler.getSourceImagePathsLength();
    // TODO: add crossfade mecanism to absorption coefficients if zipper effect perceived at material change
    absorptionCoefs.resize(IDs.size());
    for (int j = 0; j < IDs.size(); j++)
    {
        absorptionCoefs[j] = oscHandler.getSourceImageAbsorbtion(IDs[j]);
    }
    
    // save new RT60 times
    valuesRT60Future = oscHandler.getRT60Values();
    updateReverbTailDistribution();
    DBG( String("future tail size: ") + String( tailTimesCurrent.size() ));
    DBG( String("max tail size:    ") + String( (getMaxValue(valuesRT60Future) - getMaxValue(delaysFuture)) / MIN_DELAY_BETWEEN_TAIL_TAP ));
    DBG( String("min tail size:    ") + String( (getMaxValue(valuesRT60Future) - getMaxValue(delaysFuture)) / MAX_DELAY_BETWEEN_TAIL_TAP ));
    
    // save (compute) new Ambisonic gains
    auto sourceImageDOAs = oscHandler.getSourceImageDOAs();
    ambisonicGainsCurrent.resize(IDs.size());
    ambisonicGainsFuture.resize(IDs.size());
    for (int i = 0; i < IDs.size(); i++)
    {
        ambisonicGainsFuture[i] = ambisonicEncoder.calcParams(sourceImageDOAs[i](0), sourceImageDOAs[i](1));
    }
    
    // update filter bank size
    filterBank.setNumFilters( filterBank.getNumFilters(), IDs.size() );
    filterBankTail.setNumFilters( filterBankTail.getNumFilters(), tailTimesFuture.size() );
    
    // update number of valid source images
    numSourceImages = IDs.size();
    
    // trigger crossfade mecanism
    if( numSourceImages > 0 )
    {
        crossfadeGain = 0.0;
        crossfadeOver = false;
    }
}

AudioBuffer<float> getCurrentIR ()
    {
        // init buffers
        float maxDelay = getMaxValue(delaysCurrent);
        // TODO: acount for impact of absorbtion on maxDelay required (if it does make sense..)
        int irNumSamples = (int) (maxDelay*1.1 * localSampleRate);
        irRecWorkingBuffer.setSize(1, irNumSamples);
        irRecWorkingBufferTemp = irRecWorkingBuffer;
        irRecAmbisonicBuffer.setSize(N_AMBI_CH, irNumSamples);
        irRecAmbisonicBuffer.clear();
        
        // define remapping order for ambisonic IR exported to follow ACN convention
        // (TODO: clean + procedural way to gerenate remapping, eventually remap original lib)
        int ambiChannelExportRemapping [N_AMBI_CH] = { 0, 3, 2, 1, 8, 7, 6, 5, 4 };
        
        // loop over sources images
        for( int j = 0; j < IDs.size(); j++ )
        {
            // reset working buffer
            irRecWorkingBuffer.clear();
            
            // write IR taps to buffer
            int tapSample = (int) (delaysCurrent[j] * localSampleRate);
            float tapGain = fmin( 1.0, fmax( 0.0, 1.0/pathLengths[j] ));
            irRecWorkingBuffer.setSample(0, tapSample, tapGain);
            
            // apply material absorbtion (update filter bank size first, to match number of source image)
            filterBankRec.setNumFilters( filterBankRec.getNumFilters(), IDs.size() );
            filterBankRec.processBuffer( irRecWorkingBuffer, absorptionCoefs[j], j );
            
            // Ambisonic encoding
            irRecClipboardBuffer = irRecWorkingBuffer;
            for( int k = 0; k < N_AMBI_CH; k++ )
            {
                // get clean (no ambisonic gain applied) input buffer
                irRecWorkingBuffer = irRecClipboardBuffer;
                // apply ambisonic gains
                irRecWorkingBuffer.applyGain(ambisonicGainsCurrent[j][k]);
                // iteratively fill in general ambisonic buffer with source image buffers (cumulative)
                irRecAmbisonicBuffer.addFrom(ambiChannelExportRemapping[k], 0, irRecWorkingBuffer, 0, 0, irRecWorkingBuffer.getNumSamples());
            }
        }
        
        return irRecAmbisonicBuffer;
    }
    
private:
    
// update crossfade mecanism (to avoid zipper noise with smooth gains transitions)
void updateCrossfade()
{
    // handle crossfade gain mecanism
    if( crossfadeGain < 1.0 )
    {
        // either increment delay line crossfade
        crossfadeGain += 0.1;
    }
    else if (!crossfadeOver)
    {
        // or stop crossfade mecanism
        delaysCurrent = delaysFuture;
        valuesRT60Current = valuesRT60Future;
        tailTimesCurrent = tailTimesFuture;
        ambisonicGainsCurrent = ambisonicGainsFuture;
        
        crossfadeGain = 1.0; // just to make sure for the last loop using crossfade gain
        crossfadeOver = true;
    }
}

// return max value of vector
float getMaxValue(std::vector<float> vectIn)
{
    float maxValue = *std::max_element(std::begin(vectIn), std::end(vectIn));
    return maxValue;
}
    
float getReverbTailIncrement()
{
    return ( MIN_DELAY_BETWEEN_TAIL_TAP + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(MAX_DELAY_BETWEEN_TAIL_TAP-MIN_DELAY_BETWEEN_TAIL_TAP))) );
}

// compute new tail time distribution
void updateReverbTailDistribution()
{
    float maxDelayTail = getMaxValue(valuesRT60Future);
    float maxDelayTap = getMaxValue(delaysFuture);
    if( maxDelayTail > maxDelayTap ) // discard e.g. full absorbant material scenario
    {
        // set new time distribution size (to max number of values possible)
        tailTimesFuture.resize( (maxDelayTail - maxDelayTap) / MIN_DELAY_BETWEEN_TAIL_TAP, 0.f );
        
        // get reverb tail time distribution over [minTime - maxTime]
        float time = maxDelayTap + getReverbTailIncrement();
        int index = 0;
        while( time < maxDelayTail )
        {
            tailTimesFuture[index] = time;
            index += 1;
            time += getReverbTailIncrement();
        }
        // remove zero at the end (the precaution taken above)
        tailTimesFuture.resize(index);
    }
    else{ tailTimesFuture.resize(0); }
}
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceImagesHandler)
    
};

#endif // SOURCEIMAGESHANDLER_H_INCLUDED
