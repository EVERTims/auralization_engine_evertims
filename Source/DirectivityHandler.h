#ifndef DIRECTIVITYHANDLER_H_INCLUDED
#define DIRECTIVITYHANDLER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

#include <iostream>
#include <math.h>

#include <mysofa.h>

class DirectivityHandler
{
    
//=========================================================================
// ATTRIBUTES
    
public:
    
private:
    
    const static int FILTER_LENGTH = 10; // num frequency bands
    int filter_length = FILTER_LENGTH;
    float sampleRate = 48000; // dummy, just made it fit .sofa file to avoid resampling
    struct MYSOFA_EASY *hrtf;
    
    // to comply with libmysofa notations
    float leftIR[FILTER_LENGTH]; // gain real
    float rightIR[FILTER_LENGTH]; // gain imag
    float leftDelay; // dummy
    float rightDelay; // dummy
    
    Array<float> dirGains; // [gainReal0 gainReal1 .. gainRealFILTER_LENGTH-1 gainImag0 gainImag1 .. gainImagFILTER_LENGTH-1]
    
//==========================================================================
// METHODS
    
public:
    
DirectivityHandler()
{

    
    // print info
    // printGains( 2, 15 );
    
    // resize
    dirGains.resize( 2 * FILTER_LENGTH );
}
        
~DirectivityHandler()
{
    // free sofa structure
    mysofa_close( hrtf );
}
  
void loadFile( string filenameStr )
{
    // get file path
    File hrirFile = getFileFromString(filenameStr);
    
    // convert to fit libmysofa expected input format
    String path = hrirFile.getFullPathName();
    const char *filename = path.getCharPointer();
    
    // load
    int err;
    hrtf = mysofa_open(filename, sampleRate, &filter_length, &err);
    
    // warn if error
    if(hrtf==NULL)
    {
        AlertWindow::showMessageBoxAsync ( AlertWindow::WarningIcon, "failed to file", filenameStr, "OK");
    }
}

Array<float>  getGains(double azim, double elev)
{
    // make sure values are in expected range
    jassert(azim >= -M_PI && azim <= M_PI);
    jassert(elev >= -M_PI/2 && elev <= M_PI/2);
    
    // sph to cart
    float x = cosf(elev)*cosf(azim);
    float y = cosf(elev)*sinf(azim);
    float z = sinf(elev);
    
    // get interpolated gain value
    mysofa_getfilter_float(hrtf, x, y, z, leftIR, rightIR, &leftDelay, &rightDelay);
    // mysofa_getfilter_float(hrtf, x, y, z, gainsReal, gainsImag, &leftDelay, &rightDelay);
    
    // fill output
    for( int i = 0; i < FILTER_LENGTH; i++ )
    {
        dirGains.set(i, (float)( leftIR[i] ));
        dirGains.set(FILTER_LENGTH + i, (float)( rightIR[i] ));
    }
    
    return dirGains;
}
    
void printGains(int bandId, int step)
{
    // query
    float leftIR[filter_length];
    float rightIR[filter_length];
    float leftDelay;          // unit is samples
    float rightDelay;         // unit is samples

    float azim = 0;
    float elev = 0;

    float x;
    float y;
    float z;

    for( int el = -90; el <= 90; el+=step )
    {
        for( int az = 0; az < 360; az+=step )
        {
            azim = az * (M_PI / 180.f);
            elev = el * (M_PI / 180.f);

            x = cosf(elev)*cosf(azim);
            y = cosf(elev)*sinf(azim);
            z = sinf(elev);

            mysofa_getfilter_float(hrtf, x, y, z, leftIR, rightIR, &leftDelay, &rightDelay);
            
            std::cout << az << " " << el << " " << 1 << " " << leftIR[bandId] << " " << rightIR[bandId] << std::endl;
        }
    }
}
    
private:
    
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DirectivityHandler)
    
};

#endif // BINAURALENCODER_H_INCLUDED