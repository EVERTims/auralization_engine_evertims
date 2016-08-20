#ifndef SOURCEIMAGESHANDLER_H_INCLUDED
#define SOURCEIMAGESHANDLER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "AmbixEncode/AmbixEncoder.h"

class SourceImagesHandler
{

//==========================================================================
// ATTRIBUTES
    
public:
    AudioBuffer<float> sourceImageBufferTemp;
    AudioBuffer<float> crossFadeBuffer;
    
    double localSampleRate;
    int localSamplesPerBlockExpected;
    
    // sources images
    std::vector<float> IDs;
    std::vector<float> delaysCurrent; // in seconds
    std::vector<float> delaysFuture; // in seconds
    std::vector<float> pathLengths; // in meters
    
    // ambisonic encoding
    AmbixEncoder ambisonicEncoder;
    
    std::vector< Array<float> > ambisonicGainsCurrent; // buffer for input data
    std::vector< Array<float> > ambisonicGainsFuture; // to avoid zipper effect

    AudioBuffer<float> ambisonicBufferTemp;
    AudioBuffer<float> ambisonicCrossfadeBufferTemp;
    AudioBuffer<float> ambisonicBuffer;
    
    // crossfade
    float crossfadeGain = 0.0;
    bool crossfadeOver = true;
    
    // octave filter bank
    IIRFilter octaveFilterBank[NUM_OCTAVE_BANDS];
    std::vector<float> octaveFilterData[NUM_OCTAVE_BANDS];
    AudioBuffer<float> octaveFilterBufferTemp;
    AudioBuffer<float> octaveFilterBuffer;
    std::vector< Array<float> > absorptionCoefs; // buffer for input data
    
private:

    
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
    // prepare buffers
    crossFadeBuffer.setSize(1, samplesPerBlockExpected);
    crossFadeBuffer.clear();
    sourceImageBufferTemp = crossFadeBuffer;
    octaveFilterBuffer = crossFadeBuffer;
    octaveFilterBufferTemp = crossFadeBuffer;
    
    ambisonicBufferTemp = crossFadeBuffer;
    ambisonicCrossfadeBufferTemp = crossFadeBuffer;
    
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
    
    octaveFilterBuffer.setSize(1, samplesPerBlockExpected);
    octaveFilterBuffer.clear();
    octaveFilterBufferTemp = octaveFilterBuffer;
}
    
    
// get max source image delay in seconds
float getMaxDelayFuture()
{
    float maxDelay = *std::max_element(std::begin(delaysFuture), std::end(delaysFuture));
    return maxDelay;
}

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
    

    // main: loop over sources images, apply delay + room coloration + spatialization
    AudioBuffer<float> getNextAudioBlock (DelayLine* delayLine)
    {
        ambisonicBuffer.clear();
        
        for( int j = 0; j < IDs.size(); j++ )
        {
            
            //==========================================================================
            // CROSSFADE DELAYs
            float delayInFractionalSamples = 0.0;
            if( !crossfadeOver ) // TODO: BEware, this could happend in multithread while delay have been updated here and not yet taken into account above
            {
                // Add old and new tapped delayed buffers with gain crossfade
                
                // old delay
                delayInFractionalSamples = delaysCurrent[j] * localSampleRate;
                
                sourceImageBufferTemp.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
                
                sourceImageBufferTemp.applyGain(1.0 - crossfadeGain);
                
                // new delay
                delayInFractionalSamples = delaysFuture[j] * localSampleRate;
                
                crossFadeBuffer.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
                
                crossFadeBuffer.applyGain(crossfadeGain);
                
                // add both
                sourceImageBufferTemp.addFrom(0, 0, crossFadeBuffer, 0, 0, localSamplesPerBlockExpected);
                
                
                // if (j==0) DBG(String(j) + String(": ") + String(sourceImageFutureDelaysInSeconds[j] - sourceImageDelaysInSeconds[j] ));
            }
            else // simple update
            {
                delayInFractionalSamples = (delaysCurrent[j] * localSampleRate);
                
                sourceImageBufferTemp.copyFrom(0, 0, delayLine->getInterpolatedChunk(localSamplesPerBlockExpected, delayInFractionalSamples), 0, 0, localSamplesPerBlockExpected);
            }
            
            
            // apply gain based on source image path length
            float gainDelayLine = fmin( 1.0, fmax( 0.0, 1.0/pathLengths[j] ));
            sourceImageBufferTemp.applyGain(gainDelayLine);
            
            // DEBUG: sum sources images signals (without filter bank)
            // sourceImageBuffer.addFrom(0, 0, sourceImageBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            
            //==========================================================================
            // octave filer bank decomposition
            octaveFilterBuffer.clear();
            
            
            for( int k = 0; k < NUM_OCTAVE_BANDS; k++ )
            {
                
                // duplicate to get working copy
                octaveFilterBufferTemp.copyFrom(0, 0, sourceImageBufferTemp, 0, 0, localSamplesPerBlockExpected);
                
                // decompose
                octaveFilterBank[k].processSamples(octaveFilterBufferTemp.getWritePointer(0), localSamplesPerBlockExpected);
                
                // get gain
                float octaveFreqGain = fmin(abs(1.0 - absorptionCoefs[j][k]), 1.0);
                // DBG(octaveFreqGain);
                
                // apply frequency specific absorption gains
                octaveFilterBufferTemp.applyGain(octaveFreqGain);
                
                // sum to output (recompose)
                octaveFilterBuffer.addFrom(0, 0, octaveFilterBufferTemp, 0, 0, localSamplesPerBlockExpected);
            }
            
            
            // sourceImageBuffer.addFrom(0, 0, octaveFilterBuffer, 0, 0, localAudioBuffer.getNumSamples());
            
            // sourceImageBufferTemp.clear(); // no need to clear when using .copyFrom (rather than .addFrom)
            
            
            //==========================================================================
            // Spatialization: Ambisonic encoding
            
            for( int k = 0; k < N_AMBI_CH; k++ )
            {
                // create working copy
                ambisonicBufferTemp = octaveFilterBuffer;
                
                if( !crossfadeOver )
                {
                    // apply ambisonic gain past
                    ambisonicBufferTemp.applyGain((1.0 - crossfadeGain)*ambisonicGainsCurrent[j][k]);
                    
                    // create 2nd working copy
                    ambisonicCrossfadeBufferTemp = octaveFilterBuffer;
                    
                    // apply ambisonic gain future
                    ambisonicCrossfadeBufferTemp.applyGain(crossfadeGain*ambisonicGainsFuture[j][k]);
                    
                    // add past / future buffers
                    ambisonicBufferTemp.addFrom(0, 0, ambisonicCrossfadeBufferTemp, 0, 0, localSamplesPerBlockExpected);
                    
                    //                    if(j==2 && k==2) DBG(String(crossfadeGain) + String(" ") + String(sourceImageAmbisonicGains[j][k]) + String(" ") + String(sourceImageAmbisonicFutureGains[j][k]));
                }
                else
                {
                    // apply ambisonic gain
                    ambisonicBufferTemp.applyGain(ambisonicGainsCurrent[j][k]);
                }
                
                // fill in general ambisonic buffer
                ambisonicBuffer.addFrom(k, 0, ambisonicBufferTemp, 0, 0, localSamplesPerBlockExpected);
            }
            
        }
        
        return ambisonicBuffer;
    }
    
// update local attributes based on latest received OSC info
void updateFromOscHandler(OSCHandler& oscHandler)
{
    // lame mecanism to avoid changing future gains if crossfade not over yet
    while(!crossfadeOver) sleep(0.001);
    
    // save new source image data, ready to be used in next audio loop
    IDs = oscHandler.getSourceImageIDs();
    
    absorptionCoefs.resize(IDs.size());
    for (int j = 0; j < IDs.size(); j++)
    {
        absorptionCoefs[j] = oscHandler.getSourceImageAbsorbtion(IDs[j]);
    }
    
    delaysFuture = oscHandler.getSourceImageDelays();
    delaysCurrent.resize(delaysFuture.size(), 0.0f);
    pathLengths = oscHandler.getSourceImagePathsLength();
    
    // compute new ambisonic gains
    auto sourceImageDOAs = oscHandler.getSourceImageDOAs();
    ambisonicGainsCurrent.resize(sourceImageDOAs.size());
    ambisonicGainsFuture.resize(sourceImageDOAs.size());
    for (int i = 0; i < sourceImageDOAs.size(); i++)
    {
        ambisonicGainsFuture[i] = ambisonicEncoder.calcParams(sourceImageDOAs[i](0), sourceImageDOAs[i](1));
    }
    
    // TODO: update absorption coefficients here as well to be consistent
    
    // reset crossfade gain
    crossfadeGain = 0.0;
    crossfadeOver = false;
}
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceImagesHandler)
    
};

#endif // SOURCEIMAGESHANDLER_H_INCLUDED
