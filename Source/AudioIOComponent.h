#include "../JuceLibraryCode/JuceHeader.h"

class AudioIOComponent:
public Component,
public Button::Listener,
public ChangeListener
{
    
//==========================================================================
// ATTRIBUTES
    
public:
    
AudioTransportSource transportSource;
    
private:

// AUDIO FILE READER ATTRIBUTES
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

// AUDIO FILE WRITER ATTRIBUTES

    
// GUI ELEMENTS
TextButton audioFileOpenButton;
TextButton audioFilePlayButton;
TextButton audioFileStopButton;
ToggleButton audioFileLoopToogle;
Label audioFileCurrentPositionLabel;
Slider gainMasterSlider;
    
//==========================================================================
// METHODS
    
public:
    
AudioIOComponent()
{
    
    // INIT AUDIO READER ELEMENTS
    
    formatManager.registerBasicFormats();
    transportSource.addChangeListener (this);
    
    // INIT AUDIO WRITE ELEMENTS
    
    // INIT GUI ELEMENTS
    
    addAndMakeVisible (&audioFileOpenButton);
    audioFileOpenButton.setButtonText ("Open...");
    audioFileOpenButton.addListener (this);
    audioFileOpenButton.setColour (TextButton::buttonColourId, Colours::darkgrey);
    
    addAndMakeVisible (&audioFilePlayButton);
    audioFilePlayButton.setButtonText ("Play");
    audioFilePlayButton.addListener (this);
    audioFilePlayButton.setColour (TextButton::buttonColourId, Colours::darkgreen);
    audioFilePlayButton.setEnabled (false);
    
    addAndMakeVisible (&audioFileStopButton);
    audioFileStopButton.setButtonText ("Stop");
    audioFileStopButton.addListener (this);
    audioFileStopButton.setColour (TextButton::buttonColourId, Colours::darkred);
    audioFileStopButton.setEnabled (false);
    
    addAndMakeVisible (&gainMasterSlider);
    gainMasterSlider.setRange(0.1, 4.0);
    gainMasterSlider.setValue(1.0);
    gainMasterSlider.setSliderStyle(Slider::LinearHorizontal);
    gainMasterSlider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    gainMasterSlider.setColour(Slider::textBoxTextColourId, Colours::white);
    gainMasterSlider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    gainMasterSlider.setTextBoxStyle(Slider::TextBoxRight, true, 70, 20);
    
    addAndMakeVisible (&audioFileLoopToogle);
    audioFileLoopToogle.setButtonText ("Loop");
    audioFileLoopToogle.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    audioFileLoopToogle.setEnabled(true);
    audioFileLoopToogle.addListener (this);
    
    addAndMakeVisible (&audioFileCurrentPositionLabel);
    audioFileCurrentPositionLabel.setText ("Making progress, every day :)", dontSendNotification);
    audioFileCurrentPositionLabel.setColour(Label::textColourId, Colours::whitesmoke);
    
}

~AudioIOComponent() {}

//==========================================================================
// AUDIO INPUT METHODS

bool openAudioFile()
{
    bool fileOpenedSucess = false;
    
    FileChooser chooser ("Select a Wave file to play...",
                         File::nonexistent,
                         "*.wav");
    
    if (chooser.browseForFileToOpen())
    {
        File file (chooser.getResult());
        AudioFormatReader* reader = formatManager.createReaderFor (file);
        
        if (reader != nullptr)
        {
            ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
            transportSource.setSource (newSource, 0, nullptr, reader->sampleRate);
            readerSource = newSource.release();
            fileOpenedSucess = true;
        }
    }
    return fileOpenedSucess;
}


void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // check if audiofile loaded
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    
    // fill buffer with audiofile data
    transportSource.getNextAudioBlock (bufferToFill);
    
    // stereo downmix to mono
    bufferToFill.buffer->applyGain(0.5f);
    bufferToFill.buffer->addFrom(0, 0, bufferToFill.buffer->getWritePointer(1), bufferToFill.buffer->getNumSamples());
    
    // apply master gain
    bufferToFill.buffer->applyGain(gainMasterSlider.getValue());
}

//==========================================================================
// AUDIO SAVE METHODS

void saveIR(const AudioBuffer<float> &source, double sampleRate)
{
    const File file (File::getSpecialLocation (File::userDesktopDirectory)
                     .getNonexistentChildFile ("Evertims_IR_Recording", ".wav"));
    
    // Create an OutputStream to write to our destination file...
    file.deleteFile();
    ScopedPointer<FileOutputStream> fileStream (file.createOutputStream());
    
    if (fileStream != nullptr)
    {
        // Now create a WAV writer object that writes to our output stream...
        WavAudioFormat wavFormat;
        AudioFormatWriter* writer = wavFormat.createWriterFor (fileStream, sampleRate, source.getNumChannels(), 24, StringPairArray(), 0);
        
        if (writer != nullptr)
        {
            fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)
            
            writer->writeFromAudioSampleBuffer(source, 0, source.getNumSamples());
            delete(writer);
            
        }
    }

}
    
private:
    
//==========================================================================
// GUI METHODS

void paint(Graphics& g) override
{
    float w = static_cast<float>( getWidth() );
    float h = static_cast<float>( getHeight() );
    
    g.setOpacity(1.0f);
    g.setColour(Colours::whitesmoke);
    g.drawRoundedRectangle(0.f, 0.f, w, h, 0.0f, 2.0f);
    
}

void resized() override
{
    auto pos = getPosition();
    
    DBG(String(pos.getY() + 10));
    
    int thirdWidth = (int)(getWidth() / 3) - 20;
    audioFileOpenButton.setBounds (pos.getX() + 10, pos.getY() + 10, thirdWidth, 40);
    audioFilePlayButton.setBounds (30 + thirdWidth, pos.getY() + 10, thirdWidth, 40);
    audioFileStopButton.setBounds (40 + 2*thirdWidth, pos.getY() + 10, thirdWidth, 40);
    
    audioFileLoopToogle.setBounds (pos.getX() + 10, pos.getY() + 60, 60, 20);
    gainMasterSlider.setBounds    (pos.getX() + 70 + 10, audioFileLoopToogle.getY(), getWidth() - thirdWidth - 120, 20);
    
    audioFileCurrentPositionLabel.setBounds (10, 130, getWidth() - 20, 20);
}

//==========================================================================
// LISTENER METHODS

void buttonClicked (Button* button) override
{
    if (button == &audioFileOpenButton)
    {
        bool fileOpenedSucess = openAudioFile();
        audioFilePlayButton.setEnabled (fileOpenedSucess);
    }
    
    if (button == &audioFilePlayButton)
    {
        if (readerSource != nullptr)
            readerSource->setLooping (audioFileLoopToogle.getToggleState());
        changeState(Starting);
    }
    
    if (button == &audioFileStopButton)
    {
        changeState (Stopping);
    }
    
    if (button == &audioFileLoopToogle)
    {
        if (readerSource != nullptr)
        {
            readerSource->setLooping (audioFileLoopToogle.getToggleState());
        }
    }
    
}

void changeState (TransportState newState)
{
    if (audioPlayerState != newState)
    {
        audioPlayerState = newState;
        
        switch (audioPlayerState)
        {
            case Stopped:
                audioFileStopButton.setEnabled (false);
                audioFilePlayButton.setEnabled (true);
                transportSource.setPosition (0.0);
                break;
                
            case Loaded:
                break;
                
            case Starting:
                audioFilePlayButton.setEnabled (false);
                transportSource.start();
                break;
                
            case Playing:
                audioFileStopButton.setEnabled (true);
                break;
                
            case Stopping:
                transportSource.stop();
                break;
        }
    }
}

void changeListenerCallback (ChangeBroadcaster* broadcaster) override
{
    if (broadcaster == &transportSource)
    {
        if (broadcaster == &transportSource)
        {
            if (transportSource.isPlaying())
                changeState (Playing);
            else
                changeState (Stopped);
        }
    }
}

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioIOComponent)

};
