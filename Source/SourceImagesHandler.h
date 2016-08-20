#ifndef SOURCEIMAGESHANDLER_H_INCLUDED
#define SOURCEIMAGESHANDLER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "AmbixEncode/AmbixEncoder.h"

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
    
private:
    
    // audio buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> workingBufferTemp; // 2nd working buffer, e.g. for crossfade mecanism
    AudioBuffer<float> clipboardBuffer; // to be used as local copy of working buffer when working buffer modified in loops
    
    AudioBuffer<float> irRecWorkingBuffer; // used for IR recording
    AudioBuffer<float> irRecAmbisonicBuffer; // used for IR recording
    AudioBuffer<float> irRecClipboardBuffer;
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    
    // crossfade mecanism
    float crossfadeGain = 0.0;
    bool crossfadeOver = true;
    
    // octave filter bank
    IIRFilter octaveFilterBank[NUM_OCTAVE_BANDS];
    std::vector<float> octaveFilterData[NUM_OCTAVE_BANDS];
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
    double f0 = 31.5;
    double Q = sqrt(2) / (2 - 1);
    float gainFactor = 1.0;
    for( int i = 0; i < NUM_OCTAVE_BANDS; i++ )
    {
        f0 *= 2;
        octaveFilterBank[i].setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, f0, Q, gainFactor));
    }
    
}
    
// get max source image delay in seconds
float getMaxDelayFuture()
{
    float maxDelay = *std::max_element(std::begin(delaysFuture), std::end(delaysFuture));
    return maxDelay;
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
        
        // keep local copy since working buffer will be used as cumulative, iteratively receiving octave bands buffers
        clipboardBuffer = workingBuffer;
        workingBuffer.clear();
        
        for( int k = 0; k < NUM_OCTAVE_BANDS; k++ )
        {
            // local working copy
            workingBufferTemp.copyFrom(0, 0, clipboardBuffer, 0, 0, localSamplesPerBlockExpected);
            
            // filter bank decomposition
            octaveFilterBank[k].processSamples(workingBufferTemp.getWritePointer(0), localSamplesPerBlockExpected);
            
            // apply frequency specific absorption gains
            float octaveFreqGain = fmin(abs(1.0 - absorptionCoefs[j][k]), 1.0);
            workingBufferTemp.applyGain(octaveFreqGain);
            
            // sum to output (recompose)
            workingBuffer.addFrom(0, 0, workingBufferTemp, 0, 0, localSamplesPerBlockExpected);
        }
        
        
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
    
    // save (compute) new Ambisonic gains
    auto sourceImageDOAs = oscHandler.getSourceImageDOAs();
    ambisonicGainsCurrent.resize(IDs.size());
    ambisonicGainsFuture.resize(IDs.size());
    for (int i = 0; i < IDs.size(); i++)
    {
        ambisonicGainsFuture[i] = ambisonicEncoder.calcParams(sourceImageDOAs[i](0), sourceImageDOAs[i](1));
    }
    
    // update number of valid source images
    numSourceImages = IDs.size();
    
    // trigger crossfade mecanism
    crossfadeGain = 0.0;
    crossfadeOver = false;
}

AudioBuffer<float> getCurrentIR ()
    {
        // init buffers
        float maxDelay = getMaxDelayCurrent();
        // TODO: acount for impact of absorbtion on maxDelay required (if it does make sense..)
        int irNumSamples = (int) (maxDelay*1.1 * localSampleRate);
        irRecWorkingBuffer.setSize(1, irNumSamples);
        irRecAmbisonicBuffer.setSize(N_AMBI_CH, irNumSamples);
        irRecAmbisonicBuffer.clear();
        
        // loop over sources images
        for( int j = 0; j < IDs.size(); j++ )
        {
            // reset working buffer
            irRecWorkingBuffer.clear();
            
            // write IR taps to buffer
            int tapSample = (int) (delaysCurrent[j] * localSampleRate);
            float tapGain = fmin( 1.0, fmax( 0.0, 1.0/pathLengths[j] ));
            irRecWorkingBuffer.setSample(0, tapSample, tapGain);
            
            // apply material absorbtion
        
            // Ambisonic encoding
            irRecClipboardBuffer = irRecWorkingBuffer;
            for( int k = 0; k < N_AMBI_CH; k++ )
            {
                // get clean (no ambisonic gain applied) input buffer
                irRecWorkingBuffer = irRecClipboardBuffer;
                // apply ambisonic gains
                irRecWorkingBuffer.applyGain(ambisonicGainsCurrent[j][k]);
                // iteratively fill in general ambisonic buffer with source image buffers (cumulative)
                irRecAmbisonicBuffer.addFrom(k, 0, irRecWorkingBuffer, 0, 0, irRecWorkingBuffer.getNumSamples());
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
        ambisonicGainsCurrent = ambisonicGainsFuture;
        
        crossfadeGain = 1.0; // just to make sure for the last loop using crossfade gain
        crossfadeOver = true;
    }
}

// get max source image delay in seconds
float getMaxDelayCurrent()
{
    float maxDelay = *std::max_element(std::begin(delaysCurrent), std::end(delaysCurrent));
    return maxDelay;
}
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceImagesHandler)
    
};

#endif // SOURCEIMAGESHANDLER_H_INCLUDED
