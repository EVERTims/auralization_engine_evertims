#ifndef BINAURALENCODER_H_INCLUDED
#define BINAURALENCODER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "FIRFilter/FIRFilter.h"
#include <math.h> // for M_PI

#define HRIR_LENGTH 200 // length of loaded HRIR (in time samples)
#define AZIM_STEP 5.0f // HRIR spatial grid step
#define ELEV_STEP 5.0f // HRIR spatial grid step
#define N_AZIM_VALUES int(360/AZIM_STEP) // total number of azimuth values in HRIR
#define N_ELEV_VALUES int(180/ELEV_STEP) + 1 // total number of elevation values in HRIR

class BinauralEncoder
{
    
//==========================================================================
// ATTRIBUTES
    
public:
    
    float crossfadeStep = 0.1f;
    
private:

    // format buffer to hold the HRIR of a given position
    using HrirBuffer = std::array < std::array<float, HRIR_LENGTH>, 2 >;

    // holds all HRIR as loaded from HRIR
    std::map<int, std::array<HrirBuffer, N_ELEV_VALUES>> hrirDict;

    // current HRIR data
    HrirBuffer hrir;

    // HRIR FIR filters
    FIRFilter hrirFir[2];
    FIRFilter hrirFirFuture[2];

    // array holding index for hrir linear interpolation
    std::array<int, 2> azimId;
    std::array<int, 2> elevId;

    // stereo output buffer
    AudioBuffer<float> stereoBuffer;
    AudioBuffer<float> stereoBufferCopy;

    // misc.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    float crossfadeGain = 0.f;
    bool crossfadeOver = true;

//==========================================================================
// METHODS
    
public:
    
BinauralEncoder()
{
    // load HRIR filters
    File hrirFile = getFileFromString("ClubFritz1_hrir.bin");
    loadHrir( hrirFile );
}

~BinauralEncoder() {}

// local equivalent of prepareToPlay
void prepareToPlay( int samplesPerBlockExpected, double sampleRate )
{
    // resize FIR
    for( int i = 0; i < 2; i++)
    {
        hrirFirFuture[i].init(samplesPerBlockExpected, HRIR_LENGTH);
        hrirFir[i].init(samplesPerBlockExpected, HRIR_LENGTH);
    }
    
    // resize buffers
    stereoBuffer.setSize(2, samplesPerBlockExpected);
    stereoBufferCopy = stereoBuffer;
    
    // keep local copies
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
}

// binaural encoding of source 1st channel (mono)
AudioBuffer<float> processBuffer( const AudioBuffer<float> & source )
{
    // update crossfade
    updateCrossfade();
    
    for( int i = 0; i < 2; i++)
    {
        // mono to stereo
        stereoBuffer.copyFrom(i, 0, source, 0, 0, localSamplesPerBlockExpected);
        
        if( crossfadeOver )
        {
            // simply apply FIR
            hrirFir[i].process(stereoBuffer.getWritePointer(i));
        }
        else
        {
            // duplicate input buffer
            stereoBufferCopy = stereoBuffer;
            
            // apply past and future FIRs
            hrirFir[i].process(stereoBuffer.getWritePointer(i));
            hrirFirFuture[i].process(stereoBufferCopy.getWritePointer(i));
            
            // crossfade mix
            stereoBuffer.applyGain( i, 0, localSamplesPerBlockExpected, 1.0f - crossfadeGain );
            stereoBufferCopy.applyGain( i, 0, localSamplesPerBlockExpected, crossfadeGain );
            stereoBuffer.addFrom(i, 0, stereoBufferCopy, i, 0, localSamplesPerBlockExpected);
        }
    }
    
    return stereoBuffer;
}

// update crossfade mechanism
void updateCrossfade()
{
    // either update crossfade
    if( crossfadeGain < 1.0 )
    {
        crossfadeGain = fmin( crossfadeGain + 0.1, 1.0 );
    }
    // or stop crossfade mechanism if not already stopped
    else if (!crossfadeOver)
    {
        // set past = future
        for( int earId = 0; earId < 2; earId++)
        {
           hrirFir[earId] = hrirFirFuture[earId];
        }
        
        // reset crossfade internals
        crossfadeGain = 1.0; // just to make sure for the last loop using crossfade gain
        crossfadeOver = true;
    }
}

// set current HRIR filters
void setPosition( double azim, double elev)
{
    // get azim / elev indices in hrir array along with associated gains for panning across HRIR
    // (panning across 4 nearest neighbors (in azim/elev) of current position
    
    // make sure values are in expected range
    jassert(azim >= -M_PI && azim <= M_PI);
    jassert(elev >= -M_PI/2 && elev <= M_PI/2);

    // rad 2 deg
    azim *= (180/M_PI);
    elev *= (180/M_PI);
    
    // mapping to hrir coordinates
    elev += 90.f;
    azim *= -1.f;
    if( azim < 0 ){ azim += 360.f; }
    
    // get azim / elev neighboring indices
    azimId[0] = fmod( std::floor(azim / AZIM_STEP), N_AZIM_VALUES);
    azimId[1] = azimId[0]+1;
    elevId[0] = fmod( std::floor(elev / ELEV_STEP), N_ELEV_VALUES);
    elevId[1] = elevId[0]+1;
    
    // deal with extrema
    if( elevId[0] == (N_ELEV_VALUES-1) ){ elevId[1] = elevId[0] - 1; }

    // get associated gains
    double azimGainHigh = fmod((azim / AZIM_STEP), N_AZIM_VALUES) - azimId[0];
    double elevGainHigh = fmod((elev / ELEV_STEP), N_ELEV_VALUES) - elevId[0];

    // fill hrir array
    for( int earId = 0; earId < 2; earId++)
    {
        for( int i = 0; i < hrir[0].size(); ++i)
        {
            hrir[earId][i] =
            (1.0f - azimGainHigh) * (1.0f - elevGainHigh) * hrirDict[ azimId[0] ][ elevId[0] ][earId][i] // low low
            + (1.0f - azimGainHigh) * elevGainHigh * hrirDict[ azimId[0] ][ elevId[1] ][earId][i] // low high
            + azimGainHigh * (1.0f - elevGainHigh) * hrirDict[ azimId[1] ][ elevId[0] ][earId][i] // high low
            + azimGainHigh * elevGainHigh * hrirDict[ azimId[1] ][ elevId[1] ][earId][i]; // high high
        }
        
        // update FIR content
        hrirFirFuture[earId].setImpulseResponse(hrir[earId].data());
    }
    
    // trigger crossfade mechanism
    crossfadeGain = 0.0f;
    crossfadeOver = false;
}
    
private:

// load a given HRIR set
void loadHrir(const File & hrirFile)
{
    // open file
    FileInputStream istream_hrir(hrirFile);
    if( istream_hrir.openedOk() )
    {
        // if hrirDict_ has already been filled with an HRIR, skip insert step below
        bool rewriteHrirDict = false;
        if (hrirDict.size() > 0){ rewriteHrirDict = true; }
        
        // loop over array first dimension (azim values)
        for (int j = 0; j < N_AZIM_VALUES; ++j) // loop azim
        {
            // resize
            if (!rewriteHrirDict)
            {
                hrirDict.insert(std::make_pair(j, std::array<HrirBuffer, N_ELEV_VALUES>()));
            }
            
            // loop over elev values
            for (int i = 0; i < N_ELEV_VALUES; ++i) // loop elev
            {
                istream_hrir.read(hrirDict[j][i][0].data(), HRIR_LENGTH * sizeof(float));
                istream_hrir.read(hrirDict[j][i][1].data(), HRIR_LENGTH * sizeof(float));
            }
        }
    }
    // if file failed to open
    else{ throw std::ios_base::failure("Failed to open HRIR file"); }
}
    
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BinauralEncoder)
    
};

#endif // BINAURALENCODER_H_INCLUDED