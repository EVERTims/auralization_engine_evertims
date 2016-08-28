#include "../JuceLibraryCode/JuceHeader.h"

class AudioIOComponent:
public Component,
public Button::Listener,
public ChangeListener,
public AudioIODeviceCallback
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

// ADC INPUT
AudioBuffer<float> adcBuffer;
    
// GUI ELEMENTS
TextButton audioFileOpenButton;
TextButton audioFilePlayButton;
TextButton audioFileStopButton;
ToggleButton audioFileLoopToggle;
ToggleButton adcToggle;
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
    
    // INIT GUI ELEMENTS
    
    addAndMakeVisible (&audioFileOpenButton);
    audioFileOpenButton.setButtonText ("Open...");
    audioFileOpenButton.addListener (this);
    audioFileOpenButton.setColour (TextButton::buttonColourId, Colours::grey);
    
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
    
    addAndMakeVisible (&audioFileLoopToggle);
    audioFileLoopToggle.setButtonText ("Loop");
    audioFileLoopToggle.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    audioFileLoopToggle.setEnabled(true);
    audioFileLoopToggle.addListener (this);
    
    addAndMakeVisible (&adcToggle);
    adcToggle.setButtonText ("Micro Input");
    adcToggle.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    adcToggle.setEnabled(true);
    adcToggle.addListener (this);
    
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

void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    adcBuffer.setSize(1, samplesPerBlockExpected);
}
    

void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // clear buffer
    bufferToFill.clearActiveBufferRegion();
                                 
    // check if audiofile loaded
    if (readerSource != nullptr)
    {
        // fill buffer with audiofile data
        transportSource.getNextAudioBlock (bufferToFill);
        
        // stereo downmix to mono
        bufferToFill.buffer->applyGain(0.5f);
        bufferToFill.buffer->addFrom(0, 0, bufferToFill.buffer->getWritePointer(1), bufferToFill.buffer->getNumSamples());
    }
    
    // copy adc inputs (stored in adcBuffer) to output
    if (adcToggle.getToggleState())
    {
        bufferToFill.buffer->addFrom(0, 0, adcBuffer, 0, 0, bufferToFill.buffer->getNumSamples());
    }
    
    // apply master gain
    bufferToFill.buffer->applyGain(gainMasterSlider.getValue());
    
    return;
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
// ADC INPUT

void audioDeviceAboutToStart (AudioIODevice*) override { adcBuffer.clear(); }

void audioDeviceStopped() override { adcBuffer.clear(); }

void audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                            float** outputChannelData, int numOutputChannels,
                            int numberOfSamples) override
{
    adcBuffer.setDataToReferTo(const_cast<float**> (inputChannelData), numInputChannels, numberOfSamples);
    
    /**
     1) We need to clear the output buffers, in case they're full of junk..
     2) I actually do not intend to output anything from that method, since
     outputChannelData will be summed with MainComponent IO device manager output
     anyway
     */
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            FloatVectorOperations::clear (outputChannelData[i], numberOfSamples);
}

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
    
    audioFileLoopToggle.setBounds (pos.getX() + 10, pos.getY() + 60, 60, 20);
    gainMasterSlider.setBounds    (pos.getX() + 70 + 10, audioFileLoopToggle.getY(), getWidth() - thirdWidth - 120, 20);
    
    adcToggle.setBounds (pos.getX() + 10, pos.getY() + 90, 120, 20);
    
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
            readerSource->setLooping (audioFileLoopToggle.getToggleState());
        changeState(Starting);
    }
    
    if (button == &audioFileStopButton)
    {
        changeState (Stopping);
    }
    
    if (button == &audioFileLoopToggle)
    {
        if (readerSource != nullptr)
        {
            readerSource->setLooping (audioFileLoopToggle.getToggleState());
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
