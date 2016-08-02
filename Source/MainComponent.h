#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "OSCHandler.h"
//#include "AudioFileReader.h"
// #include "AudioLiveScrollingDisplay.h"
#include "AmbixEncode/AmbixEncoder.h"
#include "Ambi2binIRContainer.h"
#include "FIRFilter/FIRFilter.h"
#include "Utils.h" // used to define constants
#include "DelayLine.h"

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
    
//    friend class AudioFileReader;
    
public:
    
    //==========================================================================
    MainContentComponent();
    ~MainContentComponent();
    //==========================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    //==========================================================================
    void paint (Graphics& g) override;
    void resized() override;
    //==========================================================================
    void buttonClicked (Button* button) override;
    
    //==========================================================================

    //==========================================================================
    OSCHandler oscHandler;
    
//    ScopedPointer<AudioFileReader> audioFileReader;
//     AudioFileReader audioFileReader;

    
private:
    
    void changeListenerCallback (ChangeBroadcaster* source) override;
    
    //==========================================================================
    // GUI ELEMENTS
    TextButton audioFileOpenButton;
    TextButton audioFilePlayButton;
    TextButton audioFileStopButton;
    ToggleButton audioFileLoopToogle;
    Label audioFileCurrentPositionLabel;
    TextEditor logTextBox;
    Slider gainMasterSlider;
    
    // ScopedPointer<LiveScrollingAudioDisplay> liveAudioScroller;
    
    //==========================================================================
    // AUDIO FILE PLAYER
    bool shouldLoopAudioFile = false;
    AudioFormatManager formatManager;
    ScopedPointer<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    enum TransportState
    {
        Stopped,
        Loaded,
        Starting,
        Playing,
        Stopping
    };
    TransportState audioPlayerState;
    void changeState (TransportState newState);
    bool openAudioFile();
    AudioBuffer<float> localAudioBuffer;
    
    //==========================================================================
    // AUDIO DELAY LINE
    DelayLine delayLine;
    
    
    AudioBuffer<float> sourceImageBufferTemp;
    AudioBuffer<float> sourceImageBuffer;
    

    void updateSourceImageDelayLineSize(int sampleRate);
    
    bool requireSourceImageDelayLineSizeUpdate = false;
    
    int localSampleRate;
    
    std::vector<float> sourceImageIDs;
    std::vector<float> sourceImageDelaysInSeconds;
    std::vector<float> sourceImagePathLengthsInMeter;
    //==========================================================================
    // AUDIO FILTER BANK
    IIRFilter octaveFilterBank[NUM_OCTAVE_BANDS];
    std::vector<float> octaveFilterData[NUM_OCTAVE_BANDS];
    AudioBuffer<float> octaveFilterBufferTemp;
    AudioBuffer<float> octaveFilterBuffer;
    
    //==========================================================================
    // AUDIO MANIPULATIONS
    float clipOutput(float input);
    std::vector<float> vectorBufferOut[2]; // buffer for input data
    
    //==========================================================================
    // AMBISONIC
    AmbixEncoder ambisonicEncoder;
    std::vector< Array<float> > sourceImageAmbisonicGains; // buffer for input data
    AudioBuffer<float> ambisonicBufferTemp;
    AudioBuffer<float> ambisonicBuffer;
    AudioBuffer<float> ambisonicBuffer2ndEar;
    
    
    //==========================================================================
    // AMBI2BIN
    Ambi2binIRContainer ambi2binContainer;
    FIRFilter ambi2binFilters[2*N_AMBI_CH]; // holds current ABIR (room reverb) filters
    
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


#endif  // MAINCOMPONENT_H_INCLUDED
