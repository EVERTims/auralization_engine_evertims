#ifndef AMBI2BINIRCONTAINER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "Utils.h"
#include <array>

class Ambi2binIRContainer
{
    
//==========================================================================
// ATTRIBUTES
public:
    std::array< std::array < std::array<float, AMBI2BIN_IR_LENGTH>, 2>, N_AMBI_CH> ambi2binIrDict;
    

//==========================================================================
// METHODS
public:

    Ambi2binIRContainer()
    {
        
        // open HRIR file
        auto thisDir = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory();
        
        DBG(thisDir.getFullPathName());
        
        File resourceDir; bool resourceDirDefined = false;
        if (SystemStats::getOperatingSystemName().startsWithIgnoreCase("Mac"))
        {
            resourceDir = thisDir.getParentDirectory().getChildFile("Resources");
            resourceDirDefined = true;
        }
        else if (SystemStats::getOperatingSystemName().startsWithIgnoreCase("Win"))
        {
            resourceDir = thisDir.getChildFile("data");
            resourceDirDefined = true;
        }
        else
        {
            DBG("OS not handled yet");
        }
        
        if (!resourceDirDefined) // skip loading
        {
            DBG("Ambi2bin IR not loaded: resource directory not found");
        }
        
        // load HRIR, ITD, ILD, etc.
        try
        {
            loadIR(resourceDir.getChildFile("hoa2bin_order2_IRC_1008_R_HRIR.bin").getFullPathName());
        }
        catch (std::ios_base::failure&)
        {
            DBG("Ambi2bin IR not loaded: failed to open file");
        }
    }
    
    ~Ambi2binIRContainer(){}

    // load ABIRs from file
    void loadIR(String filename)
    {
        FileInputStream istream(filename);
        if (istream.openedOk())
        {
            for (int j = 0; j < N_AMBI_CH; ++j) // loop ambi channels
            {
                for (int i = 0; i < AMBI2BIN_IR_LENGTH; ++i) // extract left ear IRs
                {
                    istream.read(&ambi2binIrDict[j][0][i], sizeof(float));
                }
                
                for (int i = 0; i < AMBI2BIN_IR_LENGTH; ++i) // extract right ear IRs
                {
                    istream.read(&ambi2binIrDict[j][1][i], sizeof(float));
                }
            }
        }
        else
            throw std::ios_base::failure("Failed to open ABIR file");
        
    }
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ambi2binIRContainer)
    
};

#endif // AMBI2BINIRCONTAINER_H_INCLUDED