#include "AudioInputComponent.h"

AudioInputComponent::AudioInputComponent()
{
    //==========================================================================
    // INIT AUDIO READER ELEMENTS
    
    formatManager.registerBasicFormats();
    transportSource.addChangeListener (this);
 
    //==========================================================================
    // INIT GUI ELEMENTS

    addAndMakeVisible (&audioFileOpenButton);
    audioFileOpenButton.setButtonText ("Open...");
    audioFileOpenButton.addListener (this);
    
    addAndMakeVisible (&audioFilePlayButton);
    audioFilePlayButton.setButtonText ("Play");
    audioFilePlayButton.addListener (this);
    audioFilePlayButton.setColour (TextButton::buttonColourId, Colours::green);
    audioFilePlayButton.setEnabled (false);
    
    addAndMakeVisible (&audioFileStopButton);
    audioFileStopButton.setButtonText ("Stop");
    audioFileStopButton.addListener (this);
    audioFileStopButton.setColour (TextButton::buttonColourId, Colours::red);
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

AudioInputComponent::~AudioInputComponent() {}

//==========================================================================
// GUI METHODS

void AudioInputComponent::paint(Graphics& g)
{
    float w = static_cast<float>( getWidth() );
    float h = static_cast<float>( getHeight() );
    
    g.setOpacity(1.0f);
    g.setColour(Colours::whitesmoke);
    g.drawRoundedRectangle(0.f, 0.f, w, h, 10.0f, 3.0f);
}

void AudioInputComponent::resized()
{
    auto pos = getPosition();
    
    DBG(String(pos.getY() + 10));
    
    int thirdWidth = (int)(getWidth() / 3) - 20;
    audioFileOpenButton.setBounds (20, pos.getY() + 10, thirdWidth, 40);
    audioFilePlayButton.setBounds (30 + thirdWidth, pos.getY() + 10, thirdWidth, 40);
    audioFileStopButton.setBounds (40 + 2*thirdWidth, pos.getY() + 10, thirdWidth, 40);
    audioFileLoopToogle.setBounds (10, pos.getY() + 60, getWidth() - thirdWidth, 30);
    
    gainMasterSlider.setBounds    (10, pos.getX() + 100, getWidth() - 20, 20);
    
    audioFileCurrentPositionLabel.setBounds (10, 130, getWidth() - 20, 20);
}

//==========================================================================
// LISTENER METHODS

void AudioInputComponent::buttonClicked (Button* button)
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

void AudioInputComponent::changeState (TransportState newState)
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

void AudioInputComponent::changeListenerCallback (ChangeBroadcaster* broadcaster)
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

//==========================================================================
// AUDIO FILE READER METHODS

bool AudioInputComponent::openAudioFile()
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


void AudioInputComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
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
