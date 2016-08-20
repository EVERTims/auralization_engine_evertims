#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "OSCHandler.h"
#include "AudioInputComponent.h"
#include "Ambi2binIRContainer.h"
#include "FIRFilter/FIRFilter.h"
#include "Utils.h" // used to define constants
#include "DelayLine.h"
#include "SourceImagesHandler.h"

#include <vector>
#include <array>

//==============================================================================
/*
 This component lives inside our window, and this is where you should put all
 your controls and content.
 */
class MainContentComponent:
    public AudioAppComponent,
    public Button::Listener,
    public ChangeListener
{

    
public:
    
    //==========================================================================
    MainContentComponent();
    ~MainContentComponent();
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    
    void paint (Graphics& g) override;
    void resized() override;
    
    void buttonClicked (Button* button) override;
    //==========================================================================

    
private:
    
    void changeListenerCallback (ChangeBroadcaster* source) override;

    //==========================================================================
    // MISC.
    int localSampleRate;
    
    //==========================================================================
    // GUI ELEMENTS
    TextButton saveIrButton;
    TextEditor logTextBox;
    
    //==========================================================================
    // AUDIO FILE PLAYER

    AudioBuffer<float> localAudioBuffer;

    //==========================================================================
    // SUB COMPONENTS
    AudioInputComponent audioInputComponent;
    
    //==========================================================================
    // OSC LISTENER
    OSCHandler oscHandler;
    void updateOnOscReveive(int sampleRate);
    
    //==========================================================================
    // AUDIO DELAY LINE
    DelayLine delayLine;
    bool requireDelayLineSizeUpdate = false;
    
    //==========================================================================
    // SOURCES IMAGES
    SourceImagesHandler sourceImagesHandler;
    
    AudioBuffer<float> ambisonicBuffer;
    
    //==========================================================================
    // AUDIO MANIPULATIONS
    float clipOutput(float input);
    
    //==========================================================================
    
    AudioBuffer<float> ambisonicBuffer2ndEar;
    
    //==========================================================================
    // AMBI2BIN
    Ambi2binIRContainer ambi2binContainer;
    FIRFilter ambi2binFilters[2*N_AMBI_CH]; // holds current ABIR (room reverb) filters
    
    //==========================================================================
    // SAVE IR
    void saveIR();
    
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


#endif  // MAINCOMPONENT_H_INCLUDED
