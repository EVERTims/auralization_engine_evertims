#pragma once

#include <vector>
#include <math.h>
#include <cmath>

#include "../JuceLibraryCode/JuceHeader.h"

#include <Eigen/Eigen>

#define SOUND_SPEED 343 // speed of sound in m.s-1
#define NUM_OCTAVE_BANDS 10 // number of octave bands used in filter bank for room absorption
#define AMBI_ORDER 2 // Ambisonic order
#define N_AMBI_CH 9 // Associated number of Ambisonic channels [pow(AMBI_ORDER+1,2)]
#define AMBI2BIN_IR_LENGTH 221 // length of loaded filters (in time samples)

//==========================================================================
// EVERTIMS STRUCTURES

struct EL_ImageSource
{
    int ID;
    int reflectionOrder;
    Eigen::Vector3f positionRelectionFirst;
    Eigen::Vector3f positionRelectionLast;
    float totalPathDistance;
    Array<float> absorption;
};

struct EL_Source
{
    String name;
    Eigen::Vector3f position;
};

struct EL_Listener
{
    String name;
    Eigen::Vector3f position;
    Eigen::Matrix3f rotationMatrix;
};

//==========================================================================
// FIR AND OouraFFT ELEMENTS

#include <complex>

template <typename T>
using ComplexVector = std::vector<std::complex<T>>;


inline bool isPowerOf2(size_t val)
{
    return (val == 1 || (val & (val - 1)) == 0);
}

inline int nextPowerOf2(int x)
{
    if (x < 0)
        return 0;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

//==========================================================================
// MATHS METHODS

// SPAT convention: azimuth in (xOy) 0° is facing y, clockwise,
// elevation in (zOx), 0° is on (xOy), 90° on z+, -90° on z-
inline Eigen::Vector3f cartesianToSpherical(const Eigen::Vector3f& p)
{
    float radius = std::sqrt(p(0) * p(0) + p(1) * p(1) + p(2) * p(2));
    float elevation = std::asin(p(2) / radius);
    float azimuth = std::atan2(p(0), p(1));
    if (p(0) < 0 && p(2) < 0)
        elevation += 2 * M_PI;
    
    return Eigen::Vector3f (azimuth, elevation, radius);
}

template <typename Type>
inline Type sign(Type x)
{
    if (x >= 0.0)
        return 1.0;

    if (x < 0.0)
        return -1.0;

    return 1.0; // to avoid c++ control may reach end of non void function in some compiler
}

template <typename Type>
inline Type deg2rad(Type deg) { return deg * M_PI / 180.0; }

template <typename Type>
inline Type rad2deg(Type rad) { return rad * 180.0 / M_PI; }

template <typename Type>
inline Type round2(Type x, int numberOfDecimals){
    return round(x * pow(10,numberOfDecimals)) / pow(10,numberOfDecimals);
}

// return max value of vector
inline float getMaxValue(std::vector<float> vectIn)
{
    if( vectIn.size() == 0 ) { return 0; } // not sure this is wise..
    else{
        float maxValue = *std::max_element(std::begin(vectIn), std::end(vectIn));
        return maxValue;
    }
}