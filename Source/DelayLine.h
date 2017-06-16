#ifndef DELAYLINE_H_INCLUDED
#define DELAYLINE_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class DelayLine
{

//==========================================================================
// ATTRIBUTES
    
public:

private:

    int writeIndex;
    int chunkReadIndex;
    
    AudioBuffer<float> buffer;
    AudioBuffer<float> chunkBuffer;
    
    AudioBuffer<float> chunkBufferPrev;
    AudioBuffer<float> chunkBufferNext;

//==========================================================================
// METHODS
    
public:
    
DelayLine()
{
    writeIndex = 0;
    chunkReadIndex = 0;
}

~DelayLine() {}

// local equivalent of prepareToPlay
void prepareToPlay (const unsigned int samplesPerBlockExpected, const double sampleRate)
{
    buffer.setSize(buffer.getNumChannels(), 2*samplesPerBlockExpected);
    buffer.clear();
    
    chunkBuffer.setSize(1, samplesPerBlockExpected);
    chunkBuffer.clear();
    chunkBufferPrev = chunkBuffer;
    chunkBufferNext = chunkBuffer;
}

// increase delay line size if need be (no shrinking delay line size for now)
void setSize(const unsigned int newNumChannels, unsigned int newNumSamples)
{
    newNumSamples = fmax(buffer.getNumSamples(), newNumSamples);
    
    if( newNumSamples > buffer.getNumSamples() || newNumChannels != buffer.getNumChannels() )
    {
        buffer.setSize(newNumChannels, newNumSamples, true, true);
    }
}

// add samples from buffer to delay line (replace)
void copyFrom(const unsigned int destChannel, const juce::AudioBuffer<float> & source, const unsigned int sourceChannel, const unsigned int sourceStartSample, const unsigned int numSamples)
{
    // either simple copy
    if ( writeIndex + numSamples <= buffer.getNumSamples() )
    {
        buffer.copyFrom(destChannel, writeIndex, source, sourceChannel, 0, numSamples);
    }
    
    // or circular copy (last samples of audio buffer will go at delay line buffer begining)
    else
    {
        int numSamplesTail = buffer.getNumSamples() - writeIndex;
        buffer.copyFrom(destChannel, writeIndex, source, sourceChannel, 0, numSamplesTail);
        buffer.copyFrom(destChannel, 0, source, sourceChannel, numSamplesTail, numSamples - numSamplesTail);
    }
}

// add samples from buffer to delay line (add)
void addFrom(const unsigned int destChannel, const AudioBuffer<float> & source, const unsigned int sourceChannel, const unsigned int sourceStartSample, const unsigned int numSamples)
{
    // either simple copy
    if ( writeIndex + numSamples <= buffer.getNumSamples() )
    {
        buffer.addFrom(destChannel, writeIndex, source, sourceChannel, 0, numSamples);
    }
    
    // or circular copy (last samples of audio buffer will go at delay line buffer begining)
    else
    {
        int numSamplesTail = buffer.getNumSamples() - writeIndex;
        buffer.addFrom(destChannel, writeIndex, source, sourceChannel, 0, numSamplesTail);
        buffer.addFrom(destChannel, 0, source, sourceChannel, numSamplesTail, numSamples - numSamplesTail);
    }
}

// increment write position, apply circular shift if need be
void incrementWritePosition(const unsigned int numSamples)
{
    writeIndex += numSamples;
    writeIndex %= buffer.getNumSamples();
}

// get delayed buffer out of delay line
AudioBuffer<float> getChunk(const unsigned int sourceChannel, const unsigned int numSamples, const unsigned int delayInSamples)
{
    
    int writePos = writeIndex - delayInSamples;
    
    if ( writePos < 0 )
    {
        writePos = buffer.getNumSamples() + writePos;
        if ( writePos < 0 ) // if after an update the first delay force to go fetch far to far: not best option yet (to set write pointer to zero)
            writePos = 0;
    }
    
    if ( ( writePos + numSamples ) < buffer.getNumSamples() )
    {
        // simple copy
        chunkBuffer.copyFrom(0, 0, buffer, sourceChannel, writePos, numSamples);
    }
    else
    {
        // circular loop
        int numSamplesTail = buffer.getNumSamples() - writePos;
        chunkBuffer.copyFrom(0, 0, buffer, sourceChannel, writePos, numSamplesTail );
        chunkBuffer.copyFrom(0, numSamplesTail, buffer, sourceChannel, 0, numSamples - numSamplesTail);
    }
    
    return chunkBuffer;
}

// get interpolated delayed buffer out of delay line (linear interpolation between previous and next)
AudioBuffer<float> getInterpolatedChunk(const unsigned int sourceChannel, const unsigned int numSamples, const float delayInSamples)
{
    // get previous and next positions in delay line
    chunkBufferPrev = getChunk(sourceChannel, numSamples, ceil(delayInSamples));
    chunkBufferNext = getChunk(sourceChannel, numSamples, floor(delayInSamples));
    
    // apply linear interpolation gains
    float gainPrev = (float)(delayInSamples-floor(delayInSamples));
    chunkBufferPrev.applyGain( gainPrev );
    chunkBufferNext.applyGain( 1.f - gainPrev );
    
    // sum buffer in output
    chunkBuffer = chunkBufferPrev;
    chunkBuffer.addFrom(0, 0, chunkBufferNext, 0, 0, numSamples);
    
    return chunkBuffer;
}

// remove all content from delay line main buffer
void clear()
{
    buffer.clear();
}

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayLine)
    
};

#endif // DELAYLINE_H_INCLUDED
