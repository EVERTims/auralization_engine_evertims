/*
 ==============================================================================
 
 This file is part of the ambix Ambisonic plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com
 
 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)
 
 Details of these licenses can be found at: www.gnu.org/licenses
 
 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 
 ==============================================================================
 */

#ifndef __ambix_encoder__AmbixEncoder__
#define __ambix_encoder__AmbixEncoder__

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>
#include <array>
#include "../JuceLibraryCode/JuceHeader.h"
#include "SphericalHarmonic/SphericalHarmonic.h"
#include "Utils.h"
#include "ambi_weight_lookup.h"
#include <Eigen/Eigen>

class AmbixEncoder {

//==========================================================================
// ATTRIBUTES
    
public:

std::array<double, N_AMBI_CH> ambiWeightUpTo2ndOrder;
float size;
Array<float> ambi_gain;  // actual gain
Array<float> _ambi_gain; // buffer for gain ramp (last status)
SphericalHarmonic sph_h;

private:

float _azimuth, _elevation, _size; // buffer to realize changes
    
    
//==========================================================================
// METHODS
    
public:
    
AmbixEncoder() :
size(0.f),
_azimuth(0.1f),
_elevation(0.1f),
_size(0.1f)
{
    sph_h.Init(AMBI_ORDER);

    // NOT PROUD OF THIS, temporary fix until the ongoing normalization study
    // proposes new normalization gains scheme
    ambiWeightUpTo2ndOrder = {{ 0.2821, 0.3455, 0.4886, 0.3455, 0.1288, 0.2575, 0.33, 0.2575, 0.1288 }};
}

~AmbixEncoder() {}

Array<float> calcParams(double azimuth, double elevation)
{
    // save last status
    _ambi_gain = ambi_gain;
    
    if (_azimuth != azimuth || _elevation != elevation || _size != size)
    {
        sph_h.Calc(azimuth, elevation);
        
        for( int i=0; i < N_AMBI_CH; ++i ) {
            ambi_gain.set(i, (float)( sph_h.Ymn(i) * ambiWeightUpTo2ndOrder[i] ));
        }
    }
    
    _azimuth = azimuth;
    _elevation = elevation;
    _size = size;
    
    return ambi_gain;
}

    
};

#endif /* defined(__ambix_encoder__AmbixEncoder__) */
