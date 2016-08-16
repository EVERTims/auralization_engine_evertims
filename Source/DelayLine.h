#ifndef DELAYLINE_H_INCLUDED
#define DELAYLINE_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class DelayLine
{
    
public:
    
    DelayLine();
    ~DelayLine();
    
    
    void setSize(int newNumSamples);
    void initBufferSize(int newNumSamples);
    void incrementWritePosition(int numSamples);
    
    void addFrom(const AudioBuffer<float> &source, int sourceChannel, int sourceStartSample, int numSamples);
    AudioBuffer<float> getChunk(int numSamples, int delayInSamples);
    AudioBuffer<float> getInterpolatedChunk(int numSamples, float delayInSamples);

    AudioBuffer<float> buffer;
    
    int writeIndex;
    
    AudioBuffer<float> chunkBuffer;
    
private:

    int chunkReadIndex;
    
    AudioBuffer<float> chunkBufferPrev;
    AudioBuffer<float> chunkBufferNext;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayLine)
};

#endif // DELAYLINE_H_INCLUDED
