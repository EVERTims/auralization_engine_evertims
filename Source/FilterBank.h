#ifndef FILTERBANK_H_INCLUDED
#define FILTERBANK_H_INCLUDED

class FilterBank
{

//==========================================================================
// ATTRIBUTES
    
public:

    int numOctaveBands = 0;
    int numIndptStream = 0;

private:

    double localSampleRate;
    int localSamplesPerBlockExpected;

    int _numOctaveBands = 0;
    int _numIndptStream = 0;
    bool updateRequired = false;
    
    std::vector<std::array<IIRFilter, NUM_OCTAVE_BANDS-1> > octaveFilterBanks;
        
    AudioBuffer<float> bufferFiltered;
    AudioBuffer<float> bufferRemains;
    
//==========================================================================
// METHODS
    
public:
    
FilterBank() {}

~FilterBank() {}

// local equivalent of prepareToPlay
void prepareToPlay( const unsigned int samplesPerBlockExpected, const double sampleRate )
{
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
    bufferFiltered.setSize(1, samplesPerBlockExpected);
    bufferRemains = bufferFiltered;
}

void setNumFilters( const unsigned int numBands, const unsigned int numSourceImages )
{   
    // skip if nothing has changed
    if( numOctaveBands == numBands && numIndptStream == numSourceImages ){ return; }
    
    // update future values
    numOctaveBands = numBands;
    numIndptStream = numSourceImages;
    
    // flag update required
    updateRequired = true;
}
    
// Define number of frequency bands in filter-bank (only choice is betwen 3 or 10)
// NOTE: a filter is stateful, and needs to be given a continuous stream of audio. Hence, each source
// image needs its own separate filter bank (see e.g. https://forum.juce.com/t/iirfilter-help/1733/7 ).
void _setNumFilters( const unsigned int numBands, const unsigned int numSourceImages )
{
    // resize band buffer
    _numOctaveBands = numBands;
    _numIndptStream = numSourceImages;
    octaveFilterBanks.resize( numSourceImages );
    
    // loop over bands of each filterbank
    double fc; // cutoff frequency
    double fcMid;
    for( int j = 0; j < numSourceImages; j++ )
    {
        if( numBands == 10 ) // 10-filter-bank
        {
            fc = 31.5;
            for( int i = 0; i < _numOctaveBands-1; i++ )
            {
                // get lowpass cut-off freq (in between "would be Fc" for bandpass, arbitrary choice)
                if( i < _numOctaveBands - 2 ){ fcMid = fc + ( 2*fc - fc )/2; }
                // last fcMid is not "mid between next and current" but "between max and current"
                else{ fcMid = fc + ( 20000 - fc )/2; }
                
                octaveFilterBanks[j][i].setCoefficients( IIRCoefficients::makeLowPass( localSampleRate, fcMid ) );
                // octaveFilterBanks[j][i].reset();
                fc *= 2;
            }
        }
        
        else // 3-filter-bank
        {
            fc = 480;
            octaveFilterBanks[j][0].setCoefficients( IIRCoefficients::makeLowPass( localSampleRate, fc ) );
            // octaveFilterBanks[j][0].reset(); // removed all resets to avoid zipper noise when changing existing filters, may need to re-add them

            fc = 8200;
            octaveFilterBanks[j][1].setCoefficients( IIRCoefficients::makeLowPass( localSampleRate, fc ) );
            // octaveFilterBanks[j][1].reset();
        }
    }
}

// Decompose source buffer into bands, return multi-channel buffer with one band per channel
void decomposeBuffer( const AudioBuffer<float> & source, AudioBuffer<float> & destination, const unsigned int sourceImageId )
{
    if( updateRequired ){
        // update filters
        _setNumFilters( numOctaveBands, numIndptStream );
        // flag update no longer required
        updateRequired = false;
    }
    
    // prepare buffers
    bufferRemains = source;
    
    // recursive filtering for all but last band
    for( int i = 0; i < _numOctaveBands-1; i++ )
    {
        // filter the remaining spectrum
        bufferFiltered = bufferRemains;
        octaveFilterBanks[sourceImageId][i].processSamples( bufferFiltered.getWritePointer(0), localSamplesPerBlockExpected );
        
        // substract just processed band from remaining spectrum
        bufferFiltered.applyGain( -1.f );
        bufferRemains.addFrom( 0, 0, bufferFiltered, 0, 0, localSamplesPerBlockExpected );
        bufferFiltered.applyGain( -1.f );
        
        // add filtered band to output
        destination.copyFrom( i, 0, bufferFiltered, 0, 0, localSamplesPerBlockExpected );
    }
    
    // last band
    destination.copyFrom( _numOctaveBands-1, 0, bufferRemains, 0, 0, localSamplesPerBlockExpected );
}
    
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterBank)
    
};

#endif // FILTERBANK_H_INCLUDED
