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

    // ADC INPUT
    AudioBuffer<float> adcBuffer;
        
    // GUI ELEMENTS
    TextButton audioFileOpenButton;
    TextButton audioFilePlayButton;
    TextButton audioFileStopButton;
    ToggleButton audioFileLoopToggle;
    ToggleButton adcToggle;
    Slider gainAudioFileSlider;
    Slider gainAdcSlider;
    
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
    audioFileOpenButton.setButtonText ("OPEN...");
    audioFileOpenButton.addListener (this);
    audioFileOpenButton.setColour (TextButton::buttonColourId, Colours::antiquewhite);
    
    addAndMakeVisible (&audioFilePlayButton);
    audioFilePlayButton.setButtonText ("PLAY");
    audioFilePlayButton.addListener (this);
    audioFilePlayButton.setColour (TextButton::buttonColourId, Colours::seagreen);
    audioFilePlayButton.setEnabled (false);
    
    addAndMakeVisible (&audioFileStopButton);
    audioFileStopButton.setButtonText ("STOP");
    audioFileStopButton.addListener (this);
    audioFileStopButton.setColour (TextButton::buttonColourId, Colours::firebrick);
    audioFileStopButton.setEnabled (false);
    
    addAndMakeVisible (&gainAudioFileSlider);
    gainAudioFileSlider.setRange(0.0, 4.0);
    gainAudioFileSlider.setValue(1.0);
    gainAudioFileSlider.setSliderStyle(Slider::LinearHorizontal);
    gainAudioFileSlider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    gainAudioFileSlider.setColour(Slider::textBoxTextColourId, Colours::white);
    gainAudioFileSlider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    gainAudioFileSlider.setTextBoxStyle(Slider::TextBoxRight, true, 70, 20);
   
    addAndMakeVisible (&gainAdcSlider);
    gainAdcSlider.setRange(0.0, 2.0);
    gainAdcSlider.setValue(1.0);
    gainAdcSlider.setSliderStyle(Slider::LinearHorizontal);
    gainAdcSlider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    gainAdcSlider.setColour(Slider::textBoxTextColourId, Colours::white);
    gainAdcSlider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    gainAdcSlider.setTextBoxStyle(Slider::TextBoxRight, true, 70, 20);
    
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
    
    // open audio file "impulse" at startup
    auto thisDir = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory();
    File resourceDir = thisDir.getParentDirectory().getChildFile("Resources");
    File audioFile = resourceDir.getChildFile("impulse.wav").getFullPathName();
    bool fileOpenedSucess = loadAudioFile( audioFile );
    audioFilePlayButton.setEnabled ( fileOpenedSucess );
}

~AudioIOComponent() {}

//==========================================================================
// AUDIO INPUT METHODS

// open audio file from GUI
bool openAudioFile()
{
    bool fileOpenedSucess = false;
    
    FileChooser chooser ("Select a Wave file to play...", File::nonexistent, "*.wav");
    
    if (chooser.browseForFileToOpen())
    {
        File file( chooser.getResult() );
        fileOpenedSucess = loadAudioFile( file );
    }
    
    return fileOpenedSucess;
}

// load audio file to transport source
bool loadAudioFile(File file)
{
    bool fileOpenedSucess = false;
    
    AudioFormatReader* reader = formatManager.createReaderFor (file);
    
    if (reader != nullptr)
    {
        ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
        transportSource.setSource (newSource, 0, nullptr, reader->sampleRate);
        readerSource = newSource.release();
        fileOpenedSucess = true;
    }
    
    return fileOpenedSucess;
}

// local equivalent of prepareToPlay
void prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    adcBuffer.setSize(1, samplesPerBlockExpected);
}
    
// fill in bufferToFill with data from audio file or adc
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
        
        // apply gain
        bufferToFill.buffer->applyGain(gainAudioFileSlider.getValue());
    }
    
    // copy adc inputs (stored in adcBuffer) to output
    if (adcToggle.getToggleState())
    {
        // apply gain
        adcBuffer.applyGain(gainAdcSlider.getValue());
        // add to output
        bufferToFill.buffer->addFrom(0, 0, adcBuffer, 0, 0, bufferToFill.buffer->getNumSamples());
    }
    
    return;
}

//==========================================================================
// AUDIO WRITE METHODS

void saveIR(const AudioBuffer<float> &source, double sampleRate, String fileName)
{
    const File file (File::getSpecialLocation (File::userDesktopDirectory)
                     .getNonexistentChildFile (fileName, ".wav"));
    
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

void audioDeviceAboutToStart (AudioIODevice*) override
{
    adcBuffer.clear();
}

void audioDeviceStopped() override
{
    adcBuffer.clear();
}

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
    {
        if (outputChannelData[i] != nullptr)
        {
            FloatVectorOperations::clear (outputChannelData[i], numberOfSamples);
        }
    }
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
    
    int thirdWidth = (int)(getWidth() / 3) - 20;
    audioFileOpenButton.setBounds (pos.getX() + 10, pos.getY() + 10, thirdWidth, 40);
    audioFilePlayButton.setBounds (30 + thirdWidth, pos.getY() + 10, thirdWidth, 40);
    audioFileStopButton.setBounds (40 + 2*thirdWidth, pos.getY() + 10, thirdWidth, 40);
    
    audioFileLoopToggle.setBounds (pos.getX() + 10, pos.getY() + 60, 60, 20);
    gainAudioFileSlider.setBounds (pos.getX() + 160, audioFileLoopToggle.getY(), 440, 20);
    
    adcToggle.setBounds (pos.getX() + 10, pos.getY() + 90, 120, 20);
    gainAdcSlider.setBounds (pos.getX() + 160, adcToggle.getY(), 440, 20);
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
        {
            readerSource->setLooping (audioFileLoopToggle.getToggleState());
        }
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
