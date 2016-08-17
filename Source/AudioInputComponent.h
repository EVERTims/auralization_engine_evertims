#pragma once

#include "../JuceLibraryCode/JuceHeader.h"


class AudioInputComponent:
	public Component,
    public Button::Listener,
    public ChangeListener
{

    
public:

    // constructor & destructor
	AudioInputComponent();
	~AudioInputComponent();
    
    // weird non yet overrided methods (kept the name because it's what they do..)
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill);
    
    // attributes
    AudioTransportSource transportSource;

    
private:
    
    // overrided JUCE methods
    void paint(Graphics& g) override;
    void resized() override;
    void buttonClicked (Button* button) override;
    void changeListenerCallback (ChangeBroadcaster* source) override;
    
    // audio file reader methods
    bool openAudioFile();
    
    // audio file reader attributes
    bool shouldLoopAudioFile = false;
    AudioFormatManager formatManager;
    ScopedPointer<AudioFormatReaderSource> readerSource;
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
    
    // GUI elements
    TextButton audioFileOpenButton;
    TextButton audioFilePlayButton;
    TextButton audioFileStopButton;
    ToggleButton audioFileLoopToogle;
    Label audioFileCurrentPositionLabel;
    Slider gainMasterSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioInputComponent)
};
