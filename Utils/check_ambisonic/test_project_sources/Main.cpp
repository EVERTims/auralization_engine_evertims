/*
 ==============================================================================
 
 This file was auto-generated!
 
 It contains the basic startup code for a Juce application.
 
 ==============================================================================
 */

#include "../JuceLibraryCode/JuceHeader.h"
# include "AmbixEncoder.h"

#include <cmath>

double deg2rad(double deg) {
    return (float)deg * M_PI / 180.0;
}

#include <iomanip>

//==============================================================================
int main (int argc, char* argv[])
{
    
    // init
    AmbixEncoder enc;
    Array<float> g;
    cout.precision(3);
    
    for( int elev = -90; elev <= 90; elev += 10 ){
        for( int azim = 0; azim < 360; azim += 10 ){
            
            // update encoder gains
            enc.azimuth = deg2rad(azim);
            enc.elevation = deg2rad(elev);
            enc.calcParams();
            
            // print output
            cout << azim << "\t" << setw(3) << elev << setw(3) << "\t";
            for( int i = 0; i < enc.ambi_gain.size(); i++ ){
                cout << setw(6) << enc.ambi_gain[i] << "\t";
            }
            cout << endl;
            
        }
    }
    
    
    return 0;
}
