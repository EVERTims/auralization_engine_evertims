#ifndef AMBI2BINIRCONTAINER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

#include "Utils.h"

#include <array>

class Ambi2binIRContainer
{
public:
    
    // constructor & destructor
	Ambi2binIRContainer();
	~Ambi2binIRContainer();

    // local methods
    void loadIR(String filename);
    
  	// local variables
    std::array< std::array < std::array<float, AMBI2BIN_IR_LENGTH>, 2>, N_AMBI_CH> ambi2binIrDict;
    
    
private:

};

#endif // AMBI2BINIRCONTAINER_H_INCLUDED