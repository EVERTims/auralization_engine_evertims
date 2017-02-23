#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "OSCHandler.h"
#include "AudioIOComponent.h"
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
    public ComboBox::Listener,
    public ChangeListener,
    public Slider::Listener
{

    
public:
    
    MainContentComponent();
    ~MainContentComponent();
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    
    void getNextAudioBlockProcess( AudioBuffer<float> *const audioBufferToFill );
    void recordIr();
    
    void paint (Graphics& g) override;
    void resized() override;
    
    void buttonClicked (Button* button) override;
    void comboBoxChanged(ComboBox* comboBox) override;
    void sliderValueChanged(Slider* slider) override;
    
    
private:
    
    void changeListenerCallback (ChangeBroadcaster* source) override;
    void updateOnOscReveive(int sampleRate);
    float clipOutput(float input);
    
    //==========================================================================
    // MISC.
    double localSampleRate;
    int localSamplesPerBlockExpected;
    OSCHandler oscHandler; // receive OSC messages, ready them for other components
    bool isRecordingIr = false;
    
    //==========================================================================
    // GUI ELEMENTS
    TextButton saveIrButton;
    TextEditor logTextBox;
    Image logoImage;
    ComboBox numFrequencyBandsComboBox;
    Label numFrequencyBandsLabel;
    ToggleButton reverbTailToggle;
    ToggleButton enableDirectToBinaural;
    ToggleButton enableLog;
    Slider gainReverbTailSlider;
    Slider gainDirectPathSlider;
    Label inputLabel;
    Label parameterLabel;
    Label directPathLabel;
    Label logLabel;
    
    //==========================================================================
    // AUDIO COMPONENTS

    // buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> recordingBufferOutput; // recording buffer
    AudioBuffer<float> recordingBufferInput; // recording buffer
    
    // audio player (GUI + audio reader + adc input)
    AudioIOComponent audioIOComponent;
    
    // delay line
    DelayLine delayLine;
    bool requireDelayLineSizeUpdate = false;
    
    // sources images
    SourceImagesHandler sourceImagesHandler;
    
    // Ambisonic to binaural decoding
    AudioBuffer<float> ambisonicBuffer;
    AudioBuffer<float> ambisonicBuffer2ndEar;
    Ambi2binIRContainer ambi2binContainer;
    FIRFilter ambi2binFilters[2*N_AMBI_CH]; // holds current ABIR (room reverb) filters
   
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


#endif  // MAINCOMPONENT_H_INCLUDED
