#ifndef OSCHANDLER_H_INCLUDED
#define OSCHANDLER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

#include "Utils.h"

#include <map>
#include <vector>

class OSCHandler :
    public Component,
    private juce::OSCReceiver,
    public OSCReceiver::Listener<OSCReceiver::MessageLoopCallback>,
    // public OSCReceiver::ListenerWithOSCAddress<OSCReceiver::MessageLoopCallback>
    public ChangeBroadcaster
{
    
public:
    
    OSCHandler();
    ~OSCHandler();

    String getMapContent();
    std::vector<float> getSourceImageIDs();
    std::vector<float> getSourceImageDelays();
    std::vector<float> getSourceImagePathsLength();
    std::vector<Point3Spherical<float>> getSourceImageDOAs();
    float* getSourceImageAbsorbtion(int sourceID);
    
    
private:
    
    int port = 3860;
    
    // std::map<int,float(*)[NUMBER_OF_ELEMENT_IN_CONSTRUCTIVE_OSC_MESSAGE-1]> SourceImageMap;
    std::map<int,EL_ImageSource> sourceImageMap;
    std::map<String,EL_Source> sourceMap;
    std::map<String,EL_Listener> listenerMap;
    
    void oscMessageReceived (const OSCMessage& message) override;
    void oscBundleReceived (const OSCBundle & bundle) override;
    

    void showConnectionErrorMessage (const String& messageText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OSCHandler)
};

#endif // OSCHANDLER_H_INCLUDED
