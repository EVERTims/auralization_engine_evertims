#ifndef DELAYLINE_H_INCLUDED
#define DELAYLINE_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class DelayLine
{
    
public:
    
    DelayLine();
    ~DelayLine();
    
    
    void setSize(int newNumSamples);
    
    void addFrom(const AudioBuffer<float> &source, int sourceChannel, int sourceStartSample, int numSamples);
    
    void initBufferSize(int newNumSamples);
    
    AudioBuffer<float> getChunk(int numSamples, int delayInSamples);
    
    void incrementWritePosition(int numSamples);
    
    
    AudioBuffer<float> buffer;
    
    int writeIndex;
    
    AudioBuffer<float> chunkBuffer;
    
private:

    int chunkReadIndex;
    
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayLine)
};

#endif // DELAYLINE_H_INCLUDED
