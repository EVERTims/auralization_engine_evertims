#ifndef FILTERBANK_H_INCLUDED
#define FILTERBANK_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class FilterBank
{

//==========================================================================
// ATTRIBUTES
    
public:

double localSampleRate; // default sampling rate, to be set in program that uses filter bank
int localSamplesPerBlockExpected;
    
private:

int numOctaveBands;
std::vector<std::array<IIRFilter, NUM_OCTAVE_BANDS-1> > octaveFilterBanks;
    
AudioBuffer<float> bufferFiltered;
AudioBuffer<float> bufferRemains;
AudioBuffer<float> bufferBands;
    
//==========================================================================
// METHODS
    
public:
    
FilterBank() {}

~FilterBank() {}

// local equivalent of prepareToPlay
void prepareToPlay( int samplesPerBlockExpected, double sampleRate )
{
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
    bufferFiltered.setSize(1, samplesPerBlockExpected);
    bufferRemains = bufferFiltered;
    bufferBands.setSize(NUM_OCTAVE_BANDS, samplesPerBlockExpected);
}

// Define number of frequency bands in filter-bank ( only choice is betwen 3 or 10 )
// NOTE: a filter is stateful, and needs to be given a continuous stream of audio, so each source image
// needs its own separate filter bank (see https://forum.juce.com/t/iirfilter-help/1733/7).
void setNumFilters( int numBands, int numSourceImages )
{
    // resize band buffer
    numOctaveBands = numBands;
    bufferBands.setSize(numBands, localSamplesPerBlockExpected);
    
    double fc; // refers to cutoff frequency
    octaveFilterBanks.resize( numSourceImages );
    
    for( int j = 0; j < numSourceImages; j++ )
    {
        if( numBands == 10 ) // 10-filter-bank
        {
            fc = 31.5; double fcMid;
            for( int i = 0; i < numOctaveBands-1; i++ )
            {
                // get lowpass cut-off freq (in between would be Fc for bandpass, arbitrary choice)
                if( i < numOctaveBands - 2 ) fcMid = fc + ( 2*fc - fc )/2;
                // last is not mid between next and current but between max and current
                else fcMid = fc + ( 20000 - fc )/2;
                
                octaveFilterBanks[j][i].setCoefficients( IIRCoefficients::makeLowPass( localSampleRate, fcMid ) );
                octaveFilterBanks[j][i].reset();
                fc *= 2;
            }
        }
        
        else // 3-filter-bank
        {
            fc = 480;
            octaveFilterBanks[j][0].setCoefficients( IIRCoefficients::makeLowPass( localSampleRate, fc ) );
            octaveFilterBanks[j][0].reset();

            fc = 8200;
            octaveFilterBanks[j][1].setCoefficients( IIRCoefficients::makeLowPass( localSampleRate, fc ) );
            octaveFilterBanks[j][1].reset();
        }
    }
}

int getNumFilters() { return numOctaveBands; }

// Decompose source buffer into bands, return multi-channel buffer with one band per channel
AudioBuffer<float> getBandBuffer( AudioBuffer<float> &source, int sourceImageId )
{
    // prepare buffers
    bufferRemains = source;
    
    // recursive filtering for all but last band
    for( int i = 0; i < numOctaveBands-1; i++ )
    {
        // filter the remaining spectrum
        bufferFiltered = bufferRemains;
        octaveFilterBanks[sourceImageId][i].processSamples( bufferFiltered.getWritePointer(0), localSamplesPerBlockExpected );
        
        // substract just processed band from remaining spectrum
        bufferFiltered.applyGain( -1.f );
        bufferRemains.addFrom( 0, 0, bufferFiltered, 0, 0, localSamplesPerBlockExpected );
        
        // add filtered band to output
        bufferBands.copyFrom( i, 0, bufferFiltered, 0, 0, localSamplesPerBlockExpected );
    }
    
    bufferBands.copyFrom( numOctaveBands-1, 0, bufferRemains, 0, 0, localSamplesPerBlockExpected );
    return bufferBands;
}
    
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterBank)
    
};

#endif // FILTERBANK_H_INCLUDED
