#pragma once

#include <vector>
#include <math.h>
#include <cmath>

#include "../JuceLibraryCode/JuceHeader.h"

#define SOUND_SPEED 343 // speed of sound in m.s-1
#define NUM_OCTAVE_BANDS 10 // number of octave bands used in filter bank for room absorption
#define AMBI_ORDER 2 // Ambisonic order
#define N_AMBI_CH 9 // Associated number of Ambisonic channels [pow(AMBI_ORDER+1,2)]
#define AMBI2BIN_IR_LENGTH 161 // length of loaded filters (in time samples)

//==========================================================================
// GEOMETRY STRUCTURES AND ROUTINES
template <typename Type>
struct Point3Cartesian
{
    Type x;
    Type y;
    Type z;
};

template <typename Type>
struct Point3Spherical
{
    Type azimuth;
    Type elevation;
    Type radius;
};

template <typename Type>
Point3Cartesian<Type> operator -(const Point3Cartesian<Type>& p1, const Point3Cartesian<Type>& p2) {
    return Point3Cartesian <Type> { p1.x - p2.x, p1.y - p2.y, p1.z - p2.z  };
}


// SPAT convention: azimuth in (xOy) 0° is facing y, clockwise,
// elevation in (zOx), 0° is on (xOy), 90° on z+, -90° on z-
template <typename Type>
inline Point3Spherical<Type> cartesianToSpherical(const Point3Cartesian<Type>& p)
{
    Type radius = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    Type elevation = std::asin(p.z / radius);
    Type azimuth = std::atan2(p.x, p.y);
    if (p.x < 0 && p.z < 0)
        elevation += 2 * M_PI;
    
    return Point3Spherical < Type > { azimuth, elevation, radius };
}


//==========================================================================
// EVERTIMS STRUCTURES
struct EL_ImageSource
{
    int ID;
    int reflectionOrder;
    Point3Cartesian<float> positionRelectionFirst;
    Point3Cartesian<float> positionRelectionLast;
    float totalPathDistance;
    float absorption[10];
};

struct EL_Source
{
    String name;
    Point3Cartesian<float> position;
};

struct EL_Listener
{
    String name;
    Point3Cartesian<float> position;
    float rotationEuler[3];
};

//==========================================================================
// FIR and OouraFFT routines

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
inline Type round2(Type x, int numberOfDecimals) { return round(x * pow(10,numberOfDecimals)) / pow(10,numberOfDecimals); }