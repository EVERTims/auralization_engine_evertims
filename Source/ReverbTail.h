#ifndef REVERBTAIL_H_INCLUDED
#define REVERBTAIL_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class ReverbTail
{
    
    //==========================================================================
    // ATTRIBUTES
    
public:
    
    std::vector<float> valuesRT60; // in sec
    
    static const int numOctaveBands = 3;
    static const int MAX_FDN_ORDER = 16;
    static const int fdnOrder = 16;
    
private:
    
    // local delay line
    DelayLine delayLine;
    
    // setup FDN (static FDN order of 16 is max for now)
    std::array<unsigned int, MAX_FDN_ORDER> fdnDelays; // in samples
    std::array< std::array < float, MAX_FDN_ORDER>, numOctaveBands > fdnGains; // S.I.
    std::array< std::array < float, MAX_FDN_ORDER>, MAX_FDN_ORDER > fdnFeedbackMatrix; // S.I.
    
    // audio buffers
    AudioBuffer<float> reverbBusBuffers; // working buffer
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> tailBuffer;
    
    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    
    //==========================================================================
    // METHODS
    
public:
    
    ReverbTail() {
        
        // init local attributes
        valuesRT60.resize( numOctaveBands, 0.0f );
        
        // define FDN parameters
        defineFdnFeedbackMatrix();
    }
    
    ~ReverbTail() {}
    
    // local equivalent of prepareToPlay
    void prepareToPlay( const unsigned int samplesPerBlockExpected, const double sampleRate )
    {
        // prepare buffers
        reverbBusBuffers.setSize(fdnOrder*numOctaveBands, samplesPerBlockExpected);
        reverbBusBuffers.clear();
        tailBuffer.setSize(fdnOrder, samplesPerBlockExpected);
        workingBuffer.setSize(1, samplesPerBlockExpected, false, true);
        
        // init delay line
        delayLine.prepareToPlay(samplesPerBlockExpected, sampleRate);
        delayLine.setSize(fdnOrder*numOctaveBands, sampleRate); // debug: set delay line max size
        
        // keep local copies
        localSampleRate = sampleRate;
        localSamplesPerBlockExpected = samplesPerBlockExpected;
        
        // update FDN parameters
        updateFdnParameters();
    }
    
    // update FDN gains and cie based on new RT60 values
    void updateInternals( const std::vector<float> & rt60Values )
    {
        // store new RT60 values
        valuesRT60 = from10to3bands( rt60Values );
        
        // increase fdn delay line length if need be
        float maxDelay = getMaxValue(valuesRT60);
        delayLine.setSize(fdnOrder*numOctaveBands, maxDelay*localSampleRate);
        
        // update FDN parameters
        updateFdnParameters();
    }
    
    // add source image to reverberation bus for latter use
    void addToBus( const unsigned int busId, const AudioBuffer<float> & source )
    {
        // If main thread operates with 3 bands
        if( source.getNumChannels() == 3 )
        {
            for( int k = 0; k < source.getNumChannels(); k++ )
            {
                reverbBusBuffers.addFrom(k*fdnOrder+busId, 0, source, k, 0, localSamplesPerBlockExpected);
            }
        }
        // If main thread operates with 10 bands (reduce to 3 here)
        else{
            // low frequencies
            for( int k = 0; k < 5; k++ )
            {
                reverbBusBuffers.addFrom(0*fdnOrder+busId, 0, source, k, 0, localSamplesPerBlockExpected);
            }
            // mid frequencies
            for( int k = 5; k < 9; k++ )
            {
                reverbBusBuffers.addFrom(1*fdnOrder+busId, 0, source, k, 0, localSamplesPerBlockExpected);
            }
            // last band
            reverbBusBuffers.addFrom(2*fdnOrder+busId, 0, source, 9, 0, localSamplesPerBlockExpected);
        }
    }
    
    // process reverb tail from bus tail, copy obtained reverb buffer to destination
    void extractBusToBuffer( AudioBuffer<float> & destination )
    {
        // loop over FDN bus to write direct input to / read output from delay line
        destination.clear();
        workingBuffer.clear();
        
        int bufferIndex = 0;
        for (int fdnId = 0; fdnId < fdnOrder; fdnId++)
        {
            for (int bandId = 0; bandId < numOctaveBands; bandId++)
            {
                bufferIndex = bandId*fdnOrder + fdnId;
                
                // clear delay line head (copy cleared buffer): necessary, to recursively use the delayLine addFrom afterwards
                delayLine.copyFrom( bufferIndex, workingBuffer, 0, 0, localSamplesPerBlockExpected );
                
                // write input to delay line
                delayLine.addFrom( bufferIndex, reverbBusBuffers, bufferIndex, 0, localSamplesPerBlockExpected );
                
                // read output from delay line (erase current content of reverbBuffers)
                delayLine.fillBufferWithDelayedChunk( reverbBusBuffers, bufferIndex, 0, bufferIndex, fdnDelays[fdnId], localSamplesPerBlockExpected );
                
                // apply FDN gains
                reverbBusBuffers.applyGain(bufferIndex, 0, localSamplesPerBlockExpected, fdnGains[bandId][fdnId]);
                
                // sum FDN to output
                destination.addFrom(fdnId, 0, reverbBusBuffers, bufferIndex, 0, localSamplesPerBlockExpected);
            }
            
        }
        
        // write fdn outputs to delay lines (with cross-feedback matrix)
        // had to put this in a different loop to await for reverbBusBuffers fill
        for (int fdnId = 0; fdnId < fdnOrder; fdnId++)
        {
            for (int fdnFedId = 0; fdnFedId < fdnOrder; fdnFedId++)
            {
                for (int bandId = 0; bandId < numOctaveBands; bandId++)
                {
                    // get fdnFedId output, apply cross-feedback gain
                    workingBuffer.copyFrom(0, 0, reverbBusBuffers, fdnFedId + bandId*fdnOrder, 0, localSamplesPerBlockExpected);
                    workingBuffer.applyGain(fdnFeedbackMatrix[fdnId][fdnFedId]);
                    
                    // write to fdnId (delayLine)
                    delayLine.addFrom(fdnId + bandId*fdnOrder, workingBuffer, 0, 0, localSamplesPerBlockExpected );
                }
            }
        }
        
        // increment delay line write position
        delayLine.incrementWritePosition(localSamplesPerBlockExpected);
        
        // clear reverb bus
        reverbBusBuffers.clear();
    }
    
    // clear content from FDN buffer
    void clear()
    {
        reverbBusBuffers.clear();
        delayLine.clear();
    }
    
private:
    
    void updateFdnParameters(){
        
        // Define FDN delays (put here even if static define to be ready for TODO)
        // TODO: delay values should be based on dist min / max (Sabine formula).
        // would require estimation of room volume / surface in EVERTims
        fdnDelays[0] = 529;
        fdnDelays[1] = 841;
        fdnDelays[2] = 961;
        fdnDelays[3] = 1369;
        fdnDelays[4] = 1681;
        fdnDelays[5] = 1849;
        fdnDelays[6] = 2209;
        fdnDelays[7] = 2809;
        fdnDelays[8] = 3481;
        fdnDelays[9] = 3721;
        fdnDelays[10] = 4489;
        fdnDelays[11] = 5041;
        fdnDelays[12] = 5329;
        fdnDelays[13] = 6241;
        fdnDelays[14] = 6889;
        fdnDelays[15] = 7921;
        
        // Define FDN gains based on new delays
        for (int bandId = 0; bandId < numOctaveBands; bandId++)
        {
            for (int fdnId = 0; fdnId < fdnOrder; fdnId++)
            {
                fdnGains[bandId][fdnId] = pow( 10, -3*( fdnDelays[fdnId] / localSampleRate ) / valuesRT60[bandId] );
            }
        }
    }
    
    // householder matrix as proposed in Jot's thesis, see also
    // https://ccrma.stanford.edu/~jos/pasp/Householder_Feedback_Matrix.html
    // (direct copy of reverbTailv3.m output)
    void defineFdnFeedbackMatrix(){
        fdnFeedbackMatrix[0][0] = 0.250;
        fdnFeedbackMatrix[0][1] = -0.250;
        fdnFeedbackMatrix[0][2] = -0.250;
        fdnFeedbackMatrix[0][3] = -0.250;
        fdnFeedbackMatrix[0][4] = -0.250;
        fdnFeedbackMatrix[0][5] = 0.250;
        fdnFeedbackMatrix[0][6] = 0.250;
        fdnFeedbackMatrix[0][7] = 0.250;
        fdnFeedbackMatrix[0][8] = -0.250;
        fdnFeedbackMatrix[0][9] = 0.250;
        fdnFeedbackMatrix[0][10] = 0.250;
        fdnFeedbackMatrix[0][11] = 0.250;
        fdnFeedbackMatrix[0][12] = -0.250;
        fdnFeedbackMatrix[0][13] = 0.250;
        fdnFeedbackMatrix[0][14] = 0.250;
        fdnFeedbackMatrix[0][15] = 0.250;
        fdnFeedbackMatrix[1][0] = -0.250;
        fdnFeedbackMatrix[1][1] = 0.250;
        fdnFeedbackMatrix[1][2] = -0.250;
        fdnFeedbackMatrix[1][3] = -0.250;
        fdnFeedbackMatrix[1][4] = 0.250;
        fdnFeedbackMatrix[1][5] = -0.250;
        fdnFeedbackMatrix[1][6] = 0.250;
        fdnFeedbackMatrix[1][7] = 0.250;
        fdnFeedbackMatrix[1][8] = 0.250;
        fdnFeedbackMatrix[1][9] = -0.250;
        fdnFeedbackMatrix[1][10] = 0.250;
        fdnFeedbackMatrix[1][11] = 0.250;
        fdnFeedbackMatrix[1][12] = 0.250;
        fdnFeedbackMatrix[1][13] = -0.250;
        fdnFeedbackMatrix[1][14] = 0.250;
        fdnFeedbackMatrix[1][15] = 0.250;
        fdnFeedbackMatrix[2][0] = -0.250;
        fdnFeedbackMatrix[2][1] = -0.250;
        fdnFeedbackMatrix[2][2] = 0.250;
        fdnFeedbackMatrix[2][3] = -0.250;
        fdnFeedbackMatrix[2][4] = 0.250;
        fdnFeedbackMatrix[2][5] = 0.250;
        fdnFeedbackMatrix[2][6] = -0.250;
        fdnFeedbackMatrix[2][7] = 0.250;
        fdnFeedbackMatrix[2][8] = 0.250;
        fdnFeedbackMatrix[2][9] = 0.250;
        fdnFeedbackMatrix[2][10] = -0.250;
        fdnFeedbackMatrix[2][11] = 0.250;
        fdnFeedbackMatrix[2][12] = 0.250;
        fdnFeedbackMatrix[2][13] = 0.250;
        fdnFeedbackMatrix[2][14] = -0.250;
        fdnFeedbackMatrix[2][15] = 0.250;
        fdnFeedbackMatrix[3][0] = -0.250;
        fdnFeedbackMatrix[3][1] = -0.250;
        fdnFeedbackMatrix[3][2] = -0.250;
        fdnFeedbackMatrix[3][3] = 0.250;
        fdnFeedbackMatrix[3][4] = 0.250;
        fdnFeedbackMatrix[3][5] = 0.250;
        fdnFeedbackMatrix[3][6] = 0.250;
        fdnFeedbackMatrix[3][7] = -0.250;
        fdnFeedbackMatrix[3][8] = 0.250;
        fdnFeedbackMatrix[3][9] = 0.250;
        fdnFeedbackMatrix[3][10] = 0.250;
        fdnFeedbackMatrix[3][11] = -0.250;
        fdnFeedbackMatrix[3][12] = 0.250;
        fdnFeedbackMatrix[3][13] = 0.250;
        fdnFeedbackMatrix[3][14] = 0.250;
        fdnFeedbackMatrix[3][15] = -0.250;
        fdnFeedbackMatrix[4][0] = -0.250;
        fdnFeedbackMatrix[4][1] = 0.250;
        fdnFeedbackMatrix[4][2] = 0.250;
        fdnFeedbackMatrix[4][3] = 0.250;
        fdnFeedbackMatrix[4][4] = 0.250;
        fdnFeedbackMatrix[4][5] = -0.250;
        fdnFeedbackMatrix[4][6] = -0.250;
        fdnFeedbackMatrix[4][7] = -0.250;
        fdnFeedbackMatrix[4][8] = -0.250;
        fdnFeedbackMatrix[4][9] = 0.250;
        fdnFeedbackMatrix[4][10] = 0.250;
        fdnFeedbackMatrix[4][11] = 0.250;
        fdnFeedbackMatrix[4][12] = -0.250;
        fdnFeedbackMatrix[4][13] = 0.250;
        fdnFeedbackMatrix[4][14] = 0.250;
        fdnFeedbackMatrix[4][15] = 0.250;
        fdnFeedbackMatrix[5][0] = 0.250;
        fdnFeedbackMatrix[5][1] = -0.250;
        fdnFeedbackMatrix[5][2] = 0.250;
        fdnFeedbackMatrix[5][3] = 0.250;
        fdnFeedbackMatrix[5][4] = -0.250;
        fdnFeedbackMatrix[5][5] = 0.250;
        fdnFeedbackMatrix[5][6] = -0.250;
        fdnFeedbackMatrix[5][7] = -0.250;
        fdnFeedbackMatrix[5][8] = 0.250;
        fdnFeedbackMatrix[5][9] = -0.250;
        fdnFeedbackMatrix[5][10] = 0.250;
        fdnFeedbackMatrix[5][11] = 0.250;
        fdnFeedbackMatrix[5][12] = 0.250;
        fdnFeedbackMatrix[5][13] = -0.250;
        fdnFeedbackMatrix[5][14] = 0.250;
        fdnFeedbackMatrix[5][15] = 0.250;
        fdnFeedbackMatrix[6][0] = 0.250;
        fdnFeedbackMatrix[6][1] = 0.250;
        fdnFeedbackMatrix[6][2] = -0.250;
        fdnFeedbackMatrix[6][3] = 0.250;
        fdnFeedbackMatrix[6][4] = -0.250;
        fdnFeedbackMatrix[6][5] = -0.250;
        fdnFeedbackMatrix[6][6] = 0.250;
        fdnFeedbackMatrix[6][7] = -0.250;
        fdnFeedbackMatrix[6][8] = 0.250;
        fdnFeedbackMatrix[6][9] = 0.250;
        fdnFeedbackMatrix[6][10] = -0.250;
        fdnFeedbackMatrix[6][11] = 0.250;
        fdnFeedbackMatrix[6][12] = 0.250;
        fdnFeedbackMatrix[6][13] = 0.250;
        fdnFeedbackMatrix[6][14] = -0.250;
        fdnFeedbackMatrix[6][15] = 0.250;
        fdnFeedbackMatrix[7][0] = 0.250;
        fdnFeedbackMatrix[7][1] = 0.250;
        fdnFeedbackMatrix[7][2] = 0.250;
        fdnFeedbackMatrix[7][3] = -0.250;
        fdnFeedbackMatrix[7][4] = -0.250;
        fdnFeedbackMatrix[7][5] = -0.250;
        fdnFeedbackMatrix[7][6] = -0.250;
        fdnFeedbackMatrix[7][7] = 0.250;
        fdnFeedbackMatrix[7][8] = 0.250;
        fdnFeedbackMatrix[7][9] = 0.250;
        fdnFeedbackMatrix[7][10] = 0.250;
        fdnFeedbackMatrix[7][11] = -0.250;
        fdnFeedbackMatrix[7][12] = 0.250;
        fdnFeedbackMatrix[7][13] = 0.250;
        fdnFeedbackMatrix[7][14] = 0.250;
        fdnFeedbackMatrix[7][15] = -0.250;
        fdnFeedbackMatrix[8][0] = -0.250;
        fdnFeedbackMatrix[8][1] = 0.250;
        fdnFeedbackMatrix[8][2] = 0.250;
        fdnFeedbackMatrix[8][3] = 0.250;
        fdnFeedbackMatrix[8][4] = -0.250;
        fdnFeedbackMatrix[8][5] = 0.250;
        fdnFeedbackMatrix[8][6] = 0.250;
        fdnFeedbackMatrix[8][7] = 0.250;
        fdnFeedbackMatrix[8][8] = 0.250;
        fdnFeedbackMatrix[8][9] = -0.250;
        fdnFeedbackMatrix[8][10] = -0.250;
        fdnFeedbackMatrix[8][11] = -0.250;
        fdnFeedbackMatrix[8][12] = -0.250;
        fdnFeedbackMatrix[8][13] = 0.250;
        fdnFeedbackMatrix[8][14] = 0.250;
        fdnFeedbackMatrix[8][15] = 0.250;
        fdnFeedbackMatrix[9][0] = 0.250;
        fdnFeedbackMatrix[9][1] = -0.250;
        fdnFeedbackMatrix[9][2] = 0.250;
        fdnFeedbackMatrix[9][3] = 0.250;
        fdnFeedbackMatrix[9][4] = 0.250;
        fdnFeedbackMatrix[9][5] = -0.250;
        fdnFeedbackMatrix[9][6] = 0.250;
        fdnFeedbackMatrix[9][7] = 0.250;
        fdnFeedbackMatrix[9][8] = -0.250;
        fdnFeedbackMatrix[9][9] = 0.250;
        fdnFeedbackMatrix[9][10] = -0.250;
        fdnFeedbackMatrix[9][11] = -0.250;
        fdnFeedbackMatrix[9][12] = 0.250;
        fdnFeedbackMatrix[9][13] = -0.250;
        fdnFeedbackMatrix[9][14] = 0.250;
        fdnFeedbackMatrix[9][15] = 0.250;
        fdnFeedbackMatrix[10][0] = 0.250;
        fdnFeedbackMatrix[10][1] = 0.250;
        fdnFeedbackMatrix[10][2] = -0.250;
        fdnFeedbackMatrix[10][3] = 0.250;
        fdnFeedbackMatrix[10][4] = 0.250;
        fdnFeedbackMatrix[10][5] = 0.250;
        fdnFeedbackMatrix[10][6] = -0.250;
        fdnFeedbackMatrix[10][7] = 0.250;
        fdnFeedbackMatrix[10][8] = -0.250;
        fdnFeedbackMatrix[10][9] = -0.250;
        fdnFeedbackMatrix[10][10] = 0.250;
        fdnFeedbackMatrix[10][11] = -0.250;
        fdnFeedbackMatrix[10][12] = 0.250;
        fdnFeedbackMatrix[10][13] = 0.250;
        fdnFeedbackMatrix[10][14] = -0.250;
        fdnFeedbackMatrix[10][15] = 0.250;
        fdnFeedbackMatrix[11][0] = 0.250;
        fdnFeedbackMatrix[11][1] = 0.250;
        fdnFeedbackMatrix[11][2] = 0.250;
        fdnFeedbackMatrix[11][3] = -0.250;
        fdnFeedbackMatrix[11][4] = 0.250;
        fdnFeedbackMatrix[11][5] = 0.250;
        fdnFeedbackMatrix[11][6] = 0.250;
        fdnFeedbackMatrix[11][7] = -0.250;
        fdnFeedbackMatrix[11][8] = -0.250;
        fdnFeedbackMatrix[11][9] = -0.250;
        fdnFeedbackMatrix[11][10] = -0.250;
        fdnFeedbackMatrix[11][11] = 0.250;
        fdnFeedbackMatrix[11][12] = 0.250;
        fdnFeedbackMatrix[11][13] = 0.250;
        fdnFeedbackMatrix[11][14] = 0.250;
        fdnFeedbackMatrix[11][15] = -0.250;
        fdnFeedbackMatrix[12][0] = -0.250;
        fdnFeedbackMatrix[12][1] = 0.250;
        fdnFeedbackMatrix[12][2] = 0.250;
        fdnFeedbackMatrix[12][3] = 0.250;
        fdnFeedbackMatrix[12][4] = -0.250;
        fdnFeedbackMatrix[12][5] = 0.250;
        fdnFeedbackMatrix[12][6] = 0.250;
        fdnFeedbackMatrix[12][7] = 0.250;
        fdnFeedbackMatrix[12][8] = -0.250;
        fdnFeedbackMatrix[12][9] = 0.250;
        fdnFeedbackMatrix[12][10] = 0.250;
        fdnFeedbackMatrix[12][11] = 0.250;
        fdnFeedbackMatrix[12][12] = 0.250;
        fdnFeedbackMatrix[12][13] = -0.250;
        fdnFeedbackMatrix[12][14] = -0.250;
        fdnFeedbackMatrix[12][15] = -0.250;
        fdnFeedbackMatrix[13][0] = 0.250;
        fdnFeedbackMatrix[13][1] = -0.250;
        fdnFeedbackMatrix[13][2] = 0.250;
        fdnFeedbackMatrix[13][3] = 0.250;
        fdnFeedbackMatrix[13][4] = 0.250;
        fdnFeedbackMatrix[13][5] = -0.250;
        fdnFeedbackMatrix[13][6] = 0.250;
        fdnFeedbackMatrix[13][7] = 0.250;
        fdnFeedbackMatrix[13][8] = 0.250;
        fdnFeedbackMatrix[13][9] = -0.250;
        fdnFeedbackMatrix[13][10] = 0.250;
        fdnFeedbackMatrix[13][11] = 0.250;
        fdnFeedbackMatrix[13][12] = -0.250;
        fdnFeedbackMatrix[13][13] = 0.250;
        fdnFeedbackMatrix[13][14] = -0.250;
        fdnFeedbackMatrix[13][15] = -0.250;
        fdnFeedbackMatrix[14][0] = 0.250;
        fdnFeedbackMatrix[14][1] = 0.250;
        fdnFeedbackMatrix[14][2] = -0.250;
        fdnFeedbackMatrix[14][3] = 0.250;
        fdnFeedbackMatrix[14][4] = 0.250;
        fdnFeedbackMatrix[14][5] = 0.250;
        fdnFeedbackMatrix[14][6] = -0.250;
        fdnFeedbackMatrix[14][7] = 0.250;
        fdnFeedbackMatrix[14][8] = 0.250;
        fdnFeedbackMatrix[14][9] = 0.250;
        fdnFeedbackMatrix[14][10] = -0.250;
        fdnFeedbackMatrix[14][11] = 0.250;
        fdnFeedbackMatrix[14][12] = -0.250;
        fdnFeedbackMatrix[14][13] = -0.250;
        fdnFeedbackMatrix[14][14] = 0.250;
        fdnFeedbackMatrix[14][15] = -0.250;
        fdnFeedbackMatrix[15][0] = 0.250;
        fdnFeedbackMatrix[15][1] = 0.250;
        fdnFeedbackMatrix[15][2] = 0.250;
        fdnFeedbackMatrix[15][3] = -0.250;
        fdnFeedbackMatrix[15][4] = 0.250;
        fdnFeedbackMatrix[15][5] = 0.250;
        fdnFeedbackMatrix[15][6] = 0.250;
        fdnFeedbackMatrix[15][7] = -0.250;
        fdnFeedbackMatrix[15][8] = 0.250;
        fdnFeedbackMatrix[15][9] = 0.250;
        fdnFeedbackMatrix[15][10] = 0.250;
        fdnFeedbackMatrix[15][11] = -0.250;
        fdnFeedbackMatrix[15][12] = -0.250;
        fdnFeedbackMatrix[15][13] = -0.250;
        fdnFeedbackMatrix[15][14] = -0.250;
        fdnFeedbackMatrix[15][15] = 0.250;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbTail)
    
};

#endif // REVERBTAIL_H_INCLUDED
