#ifndef SOURCEIMAGESHANDLER_H_INCLUDED
#define SOURCEIMAGESHANDLER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "AmbixEncode/AmbixEncoder.h"
#include "BinauralEncoder.h"
#include "FilterBank.h"
#include "ReverbTail.h"

class SourceImagesHandler
{

//==========================================================================
// ATTRIBUTES
    
public:
    
    // sources images
    std::vector<int> IDs;
    std::vector<float> delaysCurrent; // in seconds
    std::vector<float> delaysFuture; // in seconds
    std::vector<float> pathLengths; // in meters
    int numSourceImages;
    float earlyGain = 1.f;
    
    // octave filter bank
    FilterBank filterBank;
    
    // reverb tail
    ReverbTail reverbTail;
    bool enableReverbTail;
    float reverbTailGain = 1.0f;
    
    // direct path to binaural
    int directPathId;
    float directPathGain = 1.0f;
    bool enableDirectToBinaural = true;
    
    // crossfade mecanism
    float crossfadeStep = 0.1f;
    bool crossfadeOver = true;
    
    // direct binaural encoding (for direct path only)
    BinauralEncoder binauralEncoder;
    
private:
    
    // audio buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> workingBufferTemp; // 2nd working buffer, e.g. for crossfade mecanism
    AudioBuffer<float> clipboardBuffer; // to be used as local copy of working buffer when working buffer modified in loops
    AudioBuffer<float> bandBuffer; // N band buffer returned by the filterbank for f(freq) absorption
    AudioBuffer<float> tailBuffer; // FDN_ORDER band buffer returned by the FDN reverb tail
    AudioBuffer<float> binauralBuffer; // stereo buffer to handle binaural encoder output
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    
    // crossfade mecanism
    float crossfadeGain = 0.0;
    
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
    
SourceImagesHandler() {}

~SourceImagesHandler() {}

// local equivalent of prepareToPlay
void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // set number of valid source image
    numSourceImages = 0;
    directPathId = -1;
    
    // prepare buffers
    workingBuffer.setSize(1, samplesPerBlockExpected);
    workingBuffer.clear();
    workingBufferTemp = workingBuffer;
    clipboardBuffer = workingBuffer;
    bandBuffer.setSize(NUM_OCTAVE_BANDS, samplesPerBlockExpected);
    binauralBuffer.setSize(2, samplesPerBlockExpected);
    
    // ambisonic buffer holds 2 stereo channels (first) + ambisonic channels
    ambisonicBuffer.setSize(2 + N_AMBI_CH, samplesPerBlockExpected);
    
    // keep local copies
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
    
    // init filter bank
    filterBank.prepareToPlay( samplesPerBlockExpected, sampleRate );
    filterBank.setNumFilters( NUM_OCTAVE_BANDS, IDs.size() );
    
    // init reverb tail
    reverbTail.prepareToPlay( samplesPerBlockExpected, sampleRate );
    tailBuffer.setSize(reverbTail.fdnOrder, samplesPerBlockExpected);
    
    // init binaural encoder
    binauralEncoder.prepareToPlay( samplesPerBlockExpected, sampleRate );
}
    
// get max source image delay in seconds
float getMaxDelayFuture()
{
    float maxDelayTap = getMaxValue(delaysFuture);
    return maxDelayTap;
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
            workingBuffer.copyFrom(0, 0, delayLine->getInterpolatedChunk(0, localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
            workingBuffer.applyGain(1.0 - crossfadeGain);
            
            // get new delay, tap from delay line, apply gain=f(delay)
            delayInFractionalSamples = delaysFuture[j] * localSampleRate;
            workingBufferTemp.copyFrom(0, 0, delayLine->getInterpolatedChunk(0, localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
            workingBufferTemp.applyGain(crossfadeGain);
            
            // add both buffers
            workingBuffer.addFrom(0, 0, workingBufferTemp, 0, 0, localSamplesPerBlockExpected);
        }
        else // simple update
        {
            // get delay, tap from delay line
            delayInFractionalSamples = (delaysCurrent[j] * localSampleRate);
            workingBuffer.copyFrom(0, 0, delayLine->getInterpolatedChunk(0, localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
        }
        
        //==========================================================================
        // APPLY GAIN BASED ON SOURCE IMAGE PATH LENGTH
        float gainDelayLine = fmin( 1.0, fmax( 0.0, 1.0/pathLengths[j] ));
        workingBuffer.applyGain(gainDelayLine);
        
        //==========================================================================
        // APPLY ABSORPTION (FREQUENCY-BAND WISE)
        
        // decompose in frequency bands
        bandBuffer = filterBank.getBandBuffer( workingBuffer, j);
        
        // apply abs gains and recompose
        workingBuffer.clear();
        for( int k = 0; k < bandBuffer.getNumChannels(); k++ )
        {
            // apply absorption gains (TODO: sometimes crashes here at startup because absorptionCoefs data is null pointer)
            bandBuffer.applyGain(k, 0, localSamplesPerBlockExpected, fmin(abs(1.0 - absorptionCoefs[j][k]), 1.f));
            
            // recompose (add-up frequency bands)
            workingBuffer.addFrom(0, 0, bandBuffer, k, 0, localSamplesPerBlockExpected);
        }

        //==========================================================================
        // FEED REVERB TAIL FDN
        if( enableReverbTail )
        {
            int busId = j % reverbTail.fdnOrder;
            reverbTail.addToBus(busId, bandBuffer);
        }
        
        //==========================================================================
        // APPLY DIRECT PATH / EARLY GAINS
        if( directPathId == IDs[j] )
        {
            workingBuffer.applyGain(directPathGain);
        }
        else
        {
            workingBuffer.applyGain(earlyGain);
        }
        
        //==========================================================================
        // BINAURAL ENCODING (DIRECT PATH ONLY)
        if( enableDirectToBinaural && directPathId == IDs[j] )
        {
            // apply filter
            binauralBuffer = binauralEncoder.processBuffer(workingBuffer);
            
            // add to output
            ambisonicBuffer.copyFrom(0, 0, binauralBuffer, 0, 0, localSamplesPerBlockExpected);
            ambisonicBuffer.copyFrom(1, 0, binauralBuffer, 1, 0, localSamplesPerBlockExpected);
            
            // skip remaining (ambisonic encoding)
            continue;
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
            ambisonicBuffer.addFrom(2+k, 0, workingBuffer, 0, 0, localSamplesPerBlockExpected);
        }
    }
    
    //==========================================================================
    // ADD REVERB TAIL
    
    if( enableReverbTail )
    {
        
        // get tail buffer
        tailBuffer = reverbTail.getTailBuffer();
        
        // apply gain
        tailBuffer.applyGain( reverbTailGain );
        
        // add to ambisonic channels
        int ambiId; int fdnId;
        for( int k = 0; k < fmin(N_AMBI_CH, reverbTail.fdnOrder); k++ )
        {
            ambiId = k % 4; // only add reverb tail to WXYZ
            fdnId = k % reverbTail.fdnOrder;
            ambisonicBuffer.addFrom(2+ambiId, 0, tailBuffer, fdnId, 0, localSamplesPerBlockExpected);
        }
        
    // TODO:
    //      • check why evert sometimes sends "nan" RT60 (in one or two bands when changin room / material)
    //      • add reverb tail to save IR method ?
    }
    
    return ambisonicBuffer;
}
    
// update local attributes based on latest received OSC info
void updateFromOscHandler(OSCHandler& oscHandler)
{
    // make sure not to use non-valid source image ID in audio thread during update
    auto IDsTemp = oscHandler.getSourceImageIDs();
    numSourceImages = min(IDs.size(), IDsTemp.size());
    
    // save new source image data, ready to be used in next audio loop
    IDs = oscHandler.getSourceImageIDs();
    directPathId = oscHandler.getDirectPathId();
    delaysFuture = oscHandler.getSourceImageDelays();
    delaysCurrent.resize(delaysFuture.size(), 0.0f);
    pathLengths = oscHandler.getSourceImagePathsLength();
    // TODO: add crossfade mecanism to absorption coefficients if zipper effect perceived at material change
    absorptionCoefs.resize(IDs.size());
    for (int j = 0; j < IDs.size(); j++)
    {
        absorptionCoefs[j] = oscHandler.getSourceImageAbsorption(IDs[j]);
        if( filterBank.getNumFilters() == 3 )
        {
            absorptionCoefs[j] = from10to3bands(absorptionCoefs[j]);
        }
    }
    
    // update reverb tail (even if not enabled, not cpu demanding and that way it's ready to use)
    reverbTail.updateInternals( oscHandler.getRT60Values() );
    
    // save (compute) new Ambisonic gains
    auto sourceImageDOAs = oscHandler.getSourceImageDOAs();
    ambisonicGainsCurrent.resize(IDs.size());
    ambisonicGainsFuture.resize(IDs.size());
    for (int i = 0; i < IDs.size(); i++)
    {
        ambisonicGainsFuture[i] = ambisonicEncoder.calcParams(sourceImageDOAs[i](0), sourceImageDOAs[i](1));
    }
    
    // update binaural encoder (even if not enabled, not cpu demanding and that way it's ready to use)
    if( IDs.size() > 0 && directPathId > -1 )
    {
        binauralEncoder.setPosition(sourceImageDOAs[directPathId](0), sourceImageDOAs[directPathId](1));
    }
    
    // update filter bank size
    filterBank.setNumFilters( filterBank.getNumFilters(), IDs.size() );
    
    // update number of valid source images
    numSourceImages = IDs.size();
    
    // trigger crossfade mecanism
    if( numSourceImages > 0 )
    {
        crossfadeGain = 0.0;
        crossfadeOver = false;
    }
}
    
void setFilterBankSize(int numFreqBands)
{
    filterBank.setNumFilters( numFreqBands, IDs.size() );
    bandBuffer.setSize( numFreqBands, localSamplesPerBlockExpected );
}
    
private:
    
// update crossfade mecanism (to avoid zipper noise with smooth gains transitions)
void updateCrossfade()
{
    // either update crossfade
    if( crossfadeGain < 1.0 )
    {
        crossfadeGain = fmin( crossfadeGain + crossfadeStep, 1.0 );
    }
    // or stop crossfade mecanism if not already stopped
    else if (!crossfadeOver)
    {
        // set past = future
        delaysCurrent = delaysFuture;
        ambisonicGainsCurrent = ambisonicGainsFuture;
        
        // reset crossfade internals
        crossfadeGain = 1.0; // just to make sure for the last loop using crossfade gain
        crossfadeOver = true;
    }
    
}
    
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceImagesHandler)
    
};

#endif // SOURCEIMAGESHANDLER_H_INCLUDED
