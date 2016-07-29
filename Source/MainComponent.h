#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "OSCHandler.h"
//#include "AudioFileReader.h"
// #include "AudioLiveScrollingDisplay.h"
#include "AmbixEncode/AmbixEncoder.h"

#include <vector>

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
    
    // ScopedPointer<LiveScrollingAudioDisplay> liveAudioScroller;
    
    //==========================================================================
    // AUDIO FILER PLAYER
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
    //==========================================================================
    // AUDIO DELAY LINE
    AudioSampleBuffer sourceImageDelayLineBuffer; // (used as circular buffer)
    AudioSampleBuffer sourceImageDelayLineBufferReplacement;
    AudioSampleBuffer sourceImageBufferTemp;
    AudioSampleBuffer sourceImageBuffer;
    int sourceImageDelayLineWriteIndex = 0;
//    std::vector<int> sourceImageDelayLineWritePositions;
    void updateSourceImageDelayLineSize(int sampleRate);
    bool updateSourceImageDelayLineReady = false;
    // int needToResizeSourceImageDelayLineWithThis = 0;
    bool requireSourceImageDelayLineSizeUpdate = false;
    
    int localSamplePerBlockExpected;
    int localSampleRate;
    
    std::vector<float> sourceImageDelaysInSeconds;
    std::vector<float> sourceImagePathLengthsInMeter;
    //==========================================================================
    // AUDIO MANIPULATIONS
    float clipOutput(float input);
    std::vector<float> vectorBufferOut[2]; // buffer for input data
    
    //==========================================================================
    // AMBISONIC
    AmbixEncoder ambisonicEncoder;
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


#endif  // MAINCOMPONENT_H_INCLUDED
