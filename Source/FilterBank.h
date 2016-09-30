#ifndef FILTERBANK_H_INCLUDED
#define FILTERBANK_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class FilterBank
{

//==========================================================================
// ATTRIBUTES
    
public:

double localSampleRate = 44100; // default sampling rate, to be set in program that uses filter bank
    
private:

int numOctaveBands;
std::vector<std::array<IIRFilter, NUM_OCTAVE_BANDS-1> > octaveFilterBanks;
    
AudioBuffer<float> bufferFiltered;
AudioBuffer<float> bufferRemains;
    
//==========================================================================
// METHODS
    
public:
    
FilterBank() {}

~FilterBank() {}

// local equivalent of prepareToPlay
void prepareToPlay( int samplesPerBlockExpected, double sampleRate )
{
    localSampleRate = sampleRate;
    bufferFiltered.setSize(1, samplesPerBlockExpected);
    bufferRemains = bufferFiltered;
}

// Define number of frequency bands in filter-bank ( only choice is betwen 3 or 10 )
// NOTE: a filter is stateful, and needs to be given a continuous stream of audio, so each source image
// needs its own separate filter bank (see https://forum.juce.com/t/iirfilter-help/1733/7).
void setNumFilters( int numBands, int numSourceImages )
{
    double fc; // refers to cutoff frequency
    octaveFilterBanks.resize( numSourceImages );
    
    for( int j = 0; j < numSourceImages; j++ )
    {
        if( numBands == 10 ) // 10-filter-bank
        {
            numOctaveBands = numBands;
            
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
            numOctaveBands = 3;

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
    
void processBuffer( AudioBuffer<float> &source, Array<float> absorptionCoefsSingleSource, int sourceImageId )
{
    // prepare buffers
    bufferRemains = source;
    source.clear();
    
    // recursive filtering for all but last band
    float octaveFreqGain;
    for( int i = 0; i < numOctaveBands-1; i++ )
    {
        // filter the remaining spectrum
        bufferFiltered = bufferRemains;
        octaveFilterBanks[sourceImageId][i].processSamples( bufferFiltered.getWritePointer(0), bufferFiltered.getNumSamples() );
        
        // either get gain related to frequency band: 3-bands
        if( numOctaveBands == 3 )
        {
            octaveFreqGain = 0.0f;
            
            if( i == 0 )
            {
                for( int m = 0; m < 5; m++ ) { octaveFreqGain += absorptionCoefsSingleSource[m]; }
                octaveFreqGain = fmin(abs(1.0 - octaveFreqGain/5.f), 1.f);
            }
            else if( i == 1 )
            {
                for( int m = 5; m < 9; m++ ) { octaveFreqGain += absorptionCoefsSingleSource[m]; }
                octaveFreqGain = fmin(abs(1.0 - octaveFreqGain/4.f), 1.f);
            }
        }
        
        // or get gain related to frequency band: 10-bands
        else{ octaveFreqGain = fmin( abs( 1.0 - absorptionCoefsSingleSource[i] ), 1.0 ); }
        
        // substract just processed band from remaining spectrum (after de-applying gain)
        bufferFiltered.applyGain( -1.f );
        bufferRemains.addFrom( 0, 0, bufferFiltered, 0, 0, source.getNumSamples() );
        
        // apply gain related to frequency band (and compensate for neg unit gain applyed above)
        bufferFiltered.applyGain( -octaveFreqGain );
        
        // add filtered band to output
        source.addFrom( 0, 0, bufferFiltered, 0, 0, source.getNumSamples() );
    }
    
    // add last band to output (after applying corresponding abs. coef)
    octaveFreqGain = fmin( abs( 1.0 - absorptionCoefsSingleSource[9] ), 1.0 );
    bufferRemains.applyGain(octaveFreqGain);
    source.addFrom( 0, 0, bufferRemains, 0, 0, source.getNumSamples() );
}
    
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterBank)
    
};

#endif // FILTERBANK_H_INCLUDED
