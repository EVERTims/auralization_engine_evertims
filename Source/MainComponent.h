#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "OSCHandler.h"
#include "AudioIOComponent.h"
#include "AudioRecorder.h"
#include "Ambi2binIRContainer.h"
#include "FIRFilter/FIRFilter.h"
#include "Utils.h" // used to define constants
#include "DelayLine.h"
#include "SourceImagesHandler.h"
#include "LedComponent.h"

#include <vector>
#include <array>
#include <unordered_map>

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
    
    // output clipping
    bool isClipped = false;
    
    MainContentComponent();
    ~MainContentComponent();
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    
    void processAmbisonicBuffer( AudioBuffer<float> *const audioBufferToFill );
    void fillNextAudioBlock( AudioBuffer<float> *const audioBufferToFill );
    void recordAmbisonicBuffer();
    void recordIr();
    
    void paint (Graphics& g) override;
    void resized() override;
    
    void buttonClicked (Button* button) override;
    void comboBoxChanged(ComboBox* comboBox) override;
    void sliderValueChanged(Slider* slider) override;
    
private:
    
    void changeListenerCallback (ChangeBroadcaster* source) override;
    void updateOnOscReceive();
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
    TextButton clearSourceImageButton;
    TextButton saveOscButton;
    TextEditor logTextBox;
    Image logoImage;
    ComboBox numFrequencyBandsComboBox;
    Label numFrequencyBandsLabel;
    ComboBox srcDirectivityComboBox;
    Label srcDirectivityLabel;
    ToggleButton reverbTailToggle;
    ToggleButton enableDirectToBinaural;
    ToggleButton enableLog;
    ToggleButton enableRecord;
    Slider gainReverbTailSlider;
    Slider gainDirectPathSlider;
    Slider gainEarlySlider;
    Slider crossfadeStepSlider;
    Label inputLabel;
    Label parameterLabel;
    Label directPathLabel;
    Label earlyLabel;
    Label logLabel;
    Label crossfadeLabel;
    LedComponent clippingLed;
    Label clippingLedLabel;
    
    std::unordered_map<Button*, std::string> buttonMap;
    std::unordered_map<ComboBox*, Array< std::string > > comboBoxMap;
    std::unordered_map<Slider*, Array< double > > sliderMap;
    std::unordered_map<Label*, std::string> labelMap;
    std::unordered_map<ToggleButton*, std::string> toggleMap;
    //==========================================================================
    // AUDIO COMPONENTS

    // buffers
    AudioBuffer<float> workingBuffer; // working buffer
    AudioBuffer<float> recordingBufferOutput; // recording buffer
    AudioBuffer<float> recordingBufferAmbisonicOutput; // recording buffer
    AudioBuffer<float> recordingBufferInput; // recording buffer
    
    // audio player (GUI + audio reader + adc input)
    AudioIOComponent audioIOComponent;
    
    // audio stream recorder
    AudioRecorder audioRecorder;
    
    // delay line
    DelayLine delayLine;
    bool requireDelayLineSizeUpdate = false;
    
    // sources images
    SourceImagesHandler sourceImagesHandler;
    bool sourceImageHandlerNeedsUpdate = false;
    
    // Ambisonic to binaural decoding
    AudioBuffer<float> ambisonicBuffer;
    AudioBuffer<float> ambisonicRecordBuffer;
    AudioBuffer<float> ambisonicBuffer2ndEar;
    Ambi2binIRContainer ambi2binContainer;
    FIRFilter ambi2binFilters[2*N_AMBI_CH]; // holds current ABIR (room reverb) filters
    
    // frequency band
    int numFreqBands = 0;
    bool updateNumFreqBandrequired = false;
   
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


#endif  // MAINCOMPONENT_H_INCLUDED
