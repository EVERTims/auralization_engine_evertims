
#include "MainComponent.h"
#include "Utils.h"

MainContentComponent::MainContentComponent()
    : oscHandler()
    // , liveAudioScroller(new LiveScrollingAudioDisplay())
{
    setSize (650, 700);
    
    // specify the number of input and output channels that we want to open
    setAudioChannels (2, 2);
    
    // specify audio file reader format
    formatManager.registerBasicFormats();
    transportSource.addChangeListener (this);
    
    // add to change listeners
    oscHandler.addChangeListener(this);
    
    //==========================================================================
    // init GUi
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
    
    addAndMakeVisible (&audioFileLoopToogle);
    audioFileLoopToogle.setButtonText ("Loop");
    audioFileLoopToogle.addListener (this);
    
    addAndMakeVisible (&audioFileCurrentPositionLabel);
    audioFileCurrentPositionLabel.setText ("Stopped", dontSendNotification);
    
    addAndMakeVisible (logTextBox);
    logTextBox.setMultiLine (true);
    logTextBox.setReturnKeyStartsNewLine (true);
    logTextBox.setReadOnly (true);
    logTextBox.setScrollbarsShown (true);
    logTextBox.setCaretVisible (false);
    logTextBox.setPopupMenuEnabled (true);
    logTextBox.setColour (TextEditor::backgroundColourId, Colour (0x32ffffff));
    logTextBox.setColour (TextEditor::outlineColourId, Colour (0x1c000000));
    logTextBox.setColour (TextEditor::shadowColourId, Colour (0x16000000));
    
    // addAndMakeVisible (liveAudioScroller); // = new LiveScrollingAudioDisplay());
    // MainAppWindow::getSharedAudioDeviceManager().addAudioCallback (liveAudioScroller);
    
    //==========================================================================
}

MainContentComponent::~MainContentComponent()
{
    shutdownAudio();
}

//==============================================================================
void MainContentComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.
    
    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.
    
    // For more details, see the help for AudioProcessor::prepareToPlay()
    
    transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    
    // init delay line (define single channel buffer and dummy number of samples)
    // updateSourceImageDelayLineSize(sampleRate);
    sourceImageDelayLineBuffer.setSize(1, 0);
    sourceImageDelayLineBuffer.clear();
    sourceImageDelayLineBufferReplacement = sourceImageDelayLineBuffer;
    
    // keep track of sample rate
    localSampleRate = sampleRate;
    
    vectorBufferOut[0].resize(samplesPerBlockExpected);
    vectorBufferOut[1].resize(samplesPerBlockExpected);
}

void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!
    
    // For more details, see the help for AudioProcessor::getNextAudioBlock()
    
    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    
    // check if audiofile loaded
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    
    // fill buffer with audiofile data
    transportSource.getNextAudioBlock (bufferToFill);
    
    //==========================================================================
    // get buffer
    AudioSampleBuffer localAudioBuffer = *bufferToFill.buffer;
    
    // Stereo downmix to mono
    localAudioBuffer.addFrom(0, 0, localAudioBuffer.getWritePointer(1), localAudioBuffer.getNumSamples());
    localAudioBuffer.applyGain(0.5f);
    auto monoInBufferPointer = localAudioBuffer.getWritePointer(0);
    auto bufferLength = localAudioBuffer.getNumSamples();
    
    // copy buffer in local (vector) copy (needed?)
    memcpy(vectorBufferOut[0].data(), monoInBufferPointer, bufferLength * sizeof(float));
    // memcpy(vectorBufferOut[1].data(), monoInBufferPointer, bufferLength * sizeof(float));
    
    //==========================================================================
    // init delay lines (for sourceImages)
    if (updateSourceImageDelayLineReady)
    {
        // sourceImageDelayLineBuffer.setSize(1, needToResizeSourceImageDelayLineWithThis, true, true);
        sourceImageDelayLineBuffer = sourceImageDelayLineBufferReplacement;
        updateSourceImageDelayLineReady = false;
    }
    auto sourceImagePathLengthsInMeter = oscHandler.getSourceImagePathsLength();
    auto sourceImageDelaysInSeconds = oscHandler.getSourceImageDelays();
    auto sourceImageDelayLineWritePointer = sourceImageDelayLineBuffer.getWritePointer(0);
    
    //==========================================================================
    // get write pointer to output
    auto outL = localAudioBuffer.getWritePointer(0);
    auto outR = localAudioBuffer.getWritePointer(1);
    // Loop over samples
    for (int i = 0; i < bufferLength; i++)
    {
        //==========================================================================
        // DELAY LINES
        float delayLineOut = 0.0;
        float gainDelayLine = 0.0;
        

        // apply circular shift on write position if needed
        if ( sourceImageDelaysInSeconds.size() > 0 )
            if (++sourceImageDelayLineWriteIndex >= sourceImageDelayLineBuffer.getNumSamples())
                sourceImageDelayLineWriteIndex = 0;
        
        for (int j = 0; j < sourceImageDelaysInSeconds.size(); j++)
        {
    
            // write current sample in delay line (to be played later)
            sourceImageDelayLineWritePointer[sourceImageDelayLineWriteIndex] = vectorBufferOut[0][i];
            
            // get current read position
            int readPos = (sourceImageDelayLineWriteIndex - (int)(sourceImageDelaysInSeconds[j] * localSampleRate));
            
//            DBG(String(sourceImageDelayLineWritePositions[j]) + String(" ")
//                + String((int) (sourceImageDelaysInSeconds[j] * localSampleRate)) + String(" ")
//                + String(readPos) + String(" ")
//                + String(sourceImageDelayLineBuffer.getNumSamples())
//                );
            
            // apply circular shift on read position if needed
            if (readPos < 0) readPos = sourceImageDelayLineBuffer.getNumSamples() + readPos;
            
            // get current delay line gain based on source image path length
            gainDelayLine = fmin(1.0, fmax(0.0, 1/sourceImagePathLengthsInMeter[j]));
            
            // sum delay lines outputs
            delayLineOut += gainDelayLine * sourceImageDelayLineWritePointer[readPos];
        }
        

        

        
        //==========================================================================
        
        // copy to output
        outL[i] = vectorBufferOut[0][i] + delayLineOut;
        outR[i] = outL[i];
        
        // DEBUG PRECAUTION
        outL[i] = clipOutput(outL[i]);
        outR[i] = clipOutput(outR[i]);
    }
    
}

void MainContentComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.
    
    // For more details, see the help for AudioProcessor::releaseResources()
    
    transportSource.releaseResources();
    
    // clear delay line buffer
    sourceImageDelayLineBuffer.clear();
}


//==============================================================================
// ADDITIONAL AUDIO ROUTINES

// CRUDE DEBUG PRECAUTION - restrain output in [-1.0; 1.0]
float MainContentComponent::clipOutput(float input)
{
    if (std::abs(input) > 1.0f)
        return sign(input)*fmin(std::abs(input), 1.0f);
    else
        return input;
}

void MainContentComponent::updateSourceImageDelayLineSize(int sampleRate)
{
    // DBG("START TO RESIZE");
//    updateSourceImageDelayLineRequested = true;
    
    // get maximum required delay line duration
    double maxDelay = 1.0;
    auto sourceImageDelays = oscHandler.getSourceImageDelays();
    if (sourceImageDelays.size() > 0)
        maxDelay = *std::max_element(std::begin(sourceImageDelays), std::end(sourceImageDelays)); // in sec
    
    // get associated required delay line buffer length
    int delayBufferLength = fmax( 1, (int)( maxDelay * sampleRate) );
    needToResizeSourceImageDelayLineWithThis = delayBufferLength;
    
    // copy current delay line in temporary buffer
    sourceImageDelayLineBufferReplacement = sourceImageDelayLineBuffer;
    // sourceImageDelayLineBufferReplacement.copyFrom(0, 0, sourceImageDelayLineBuffer, 0, 0, sourceImageDelayLineBuffer.getNumSamples());
    
    // resize temporary buffer
    sourceImageDelayLineBufferReplacement.setSize(1, delayBufferLength, true, true);
    
    // notify audio loop that temporary buffer is ready to use
    updateSourceImageDelayLineReady = true;
    
//    // get total number of source image, to define number of required delay line buffers
//    int numDelayLines = sourceImageDelays.size();
//    
    // update delay line buffers duration
    // sourceImageDelayLineBuffer.setSize(1, delayBufferLength, true, true);

    // update associated attributes
    // sourceImageDelayLineWritePositions.resize(numDelayLines, (int)(delayBufferLength/2));
    
//    DBG("FINISHIED TO RESIZE");
    
    
    
//    updateSourceImageDelayLineRequested = false;
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::darkgrey);
    
    
    // You can add your drawing code here!
}

void MainContentComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    audioFileOpenButton.setBounds (10, 10, getWidth() - 20, 40);
    audioFilePlayButton.setBounds (10, 60, getWidth() - 20, 40);
    audioFileStopButton.setBounds (10, 110, getWidth() - 20, 40);
    audioFileLoopToogle.setBounds (10, 160, getWidth() - 20, 20);
    audioFileCurrentPositionLabel.setBounds (10, 190, getWidth() - 20, 20);
    
    // liveAudioScroller->setBounds (10, 220, getWidth() - 20, 64);
    
    logTextBox.setBounds (8, 220, getWidth() - 16, getHeight() - 230);
    
    
}

void MainContentComponent::changeListenerCallback (ChangeBroadcaster* broadcaster)
{
    if (broadcaster == &oscHandler)
    {
        logTextBox.setText(oscHandler.getMapContent());
        updateSourceImageDelayLineSize(localSampleRate);
    }
    
    else if (broadcaster == &transportSource)
    {
        if (broadcaster == &transportSource)
        {
            if (transportSource.isPlaying())
                changeState (Playing);
            else
                changeState (Stopped);
        }
    }
//    else if (broadcaster == &audioFileReader)
//    {
//        AudioFileReader::TransportState state = audioFileReader.getState();
//        if ( state == AudioFileReader::Loaded ) audioFilePlayButton.setEnabled (true);
//        if ( state == AudioFileReader::Starting ) audioFilePlayButton.setEnabled (false);
//        if ( state == AudioFileReader::Stopped )
//        {
//            audioFileStopButton.setEnabled (false);
//            audioFilePlayButton.setEnabled (true);
//        }
//        if ( state == AudioFileReader::Playing ) audioFileStopButton.setEnabled (true);
//    }
}

//==============================================================================

void MainContentComponent::buttonClicked (Button* button)
{
    if (button == &audioFileOpenButton)
    {
        bool fileOpenedSucess = openAudioFile();
        audioFilePlayButton.setEnabled (fileOpenedSucess);
    }

//    if (button == &audioFilePlayButton) audioFileReader.playAudioFile();
    if (button == &audioFilePlayButton)
    {
        if (readerSource != nullptr)
            readerSource->setLooping (audioFileLoopToogle.getToggleState());
        changeState(Starting);
    }
    
    if (button == &audioFileStopButton)
        changeState (Stopping);
    
    if (button == &audioFileLoopToogle)
        if (readerSource != nullptr)
            readerSource->setLooping (audioFileLoopToogle.getToggleState());
}

//==============================================================================

void MainContentComponent::changeState (TransportState newState)
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

//==============================================================================
bool MainContentComponent::openAudioFile()
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

//==============================================================================
// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
