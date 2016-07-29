
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
    audioFileLoopToogle.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    audioFileLoopToogle.addListener (this);
    
    addAndMakeVisible (&audioFileCurrentPositionLabel);
    audioFileCurrentPositionLabel.setText ("Stopped", dontSendNotification);
    audioFileCurrentPositionLabel.setColour(Label::textColourId, Colours::whitesmoke);
    
    addAndMakeVisible (logTextBox);
    logTextBox.setMultiLine (true);
    logTextBox.setReturnKeyStartsNewLine (true);
    logTextBox.setReadOnly (true);
    logTextBox.setScrollbarsShown (true);
    logTextBox.setCaretVisible (false);
    logTextBox.setPopupMenuEnabled (true);
    logTextBox.setColour (TextEditor::textColourId, Colours::whitesmoke);
    logTextBox.setColour (TextEditor::backgroundColourId, Colour(PixelARGB(240,30,30,30)));
    logTextBox.setColour (TextEditor::outlineColourId, Colours::grey);
    logTextBox.setColour (TextEditor::shadowColourId, Colours::darkorange);
    
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
    sourceImageDelayLineBuffer.setSize(1, samplesPerBlockExpected*2);
    sourceImageDelayLineBuffer.clear();
    // sourceImageDelayLineBufferReplacement = sourceImageDelayLineBuffer;
    sourceImageBufferTemp.clear();
    sourceImageBuffer.clear();
    
    // keep track of sample rate
    localSampleRate = sampleRate;
    localSamplePerBlockExpected = samplesPerBlockExpected;

    //==========================================================================
    // INIT FILTER BANK
    double f0 = 31.5;
    double Q = sqrt(2) / (2 - 1);
    float gainFactor = 1.0;
    for (int i = 0; i < NUM_OCTAVE_BANDS; i++)
    {
        f0 *= 2;
        octaveFilterBank[i].setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, f0, Q, gainFactor));
    }
    
    // INIT AMBISONIC
    ambisonicBuffer.setSize(N_AMBI_CH, samplesPerBlockExpected);
    

    // USEFULL ???
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
    // init filter bank
    octaveFilterBuffer = localAudioBuffer;  // set size
    octaveFilterBuffer.clear();
    octaveFilterBufferTemp = octaveFilterBuffer;
    
    //==========================================================================
    // init delay lines (for sourceImages)
    sourceImageBuffer = localAudioBuffer; // only need a mono version of this
    sourceImageBuffer.clear();
    sourceImageBufferTemp = sourceImageBuffer;
    
    //==========================================================================
    // init ambisonic
    // ambisonicBuffer = localAudioBuffer; // set size
    ambisonicBuffer.setSize(N_AMBI_CH, localAudioBuffer.getNumSamples()); // to make sure correct number of samples
    ambisonicBuffer.clear();
    
    if ( sourceImageDelaysInSeconds.size() > 0 )
    {
        
        // update delay line size if need be (TODO: MOVE THIS .SIZE() OUTSIDE OF AUDIO PROCESSING LOOP
        if (requireSourceImageDelayLineSizeUpdate)
        {
            // get maximum required delay line duration
            float maxDelay = *std::max_element(std::begin(sourceImageDelaysInSeconds), std::end(sourceImageDelaysInSeconds)); // in sec
            
            // get associated required delay line buffer length
            int updatedDelayLineLength = (int)( maxDelay * localSampleRate);
            
            // update delay line size if need be (no shrinking delay line for now)
            if (updatedDelayLineLength > sourceImageDelayLineBuffer.getNumSamples())
                sourceImageDelayLineBuffer.setSize(1, updatedDelayLineLength, true, true);
            
            // unflag update required
            requireSourceImageDelayLineSizeUpdate = false;
        }
        
        
        // update delay line with current buffer samples
        if ( sourceImageDelayLineWriteIndex + localAudioBuffer.getNumSamples() <= sourceImageDelayLineBuffer.getNumSamples() )
        { // simple copy
            sourceImageDelayLineBuffer.copyFrom(0, sourceImageDelayLineWriteIndex, localAudioBuffer, 0, 0, localAudioBuffer.getNumSamples());
        }
        else // circular copy (last samples of audio buffer will go at delay line buffer begining)
        {
            int numSamplesTail = sourceImageDelayLineBuffer.getNumSamples() - sourceImageDelayLineWriteIndex;
            sourceImageDelayLineBuffer.copyFrom(0, sourceImageDelayLineWriteIndex, localAudioBuffer, 0, 0, numSamplesTail );
            sourceImageDelayLineBuffer.copyFrom(0, 0, localAudioBuffer, 0, numSamplesTail, localAudioBuffer.getNumSamples() - numSamplesTail);
        }
        
        
        // loop over source images to apply delay + room coloration + spatialization
        for (int j = 0; j < sourceImageDelaysInSeconds.size(); j++)
        {
            
            // get delayed buffer corresponding to current source image out of delay line
            int writePos = sourceImageDelayLineWriteIndex - (int) (sourceImageDelaysInSeconds[j] * localSampleRate);
            
            if ( writePos < 0 )
                writePos = sourceImageDelayLineBuffer.getNumSamples() + writePos;
            
            if ( ( writePos + localAudioBuffer.getNumSamples() ) < sourceImageDelayLineBuffer.getNumSamples() )
            { // simple copy
                sourceImageBufferTemp.copyFrom(0, 0, sourceImageDelayLineBuffer, 0, writePos, localAudioBuffer.getNumSamples());
            }
            else
            { // circular loop
                int numSamplesTail = sourceImageDelayLineBuffer.getNumSamples() - writePos;
                sourceImageBufferTemp.copyFrom(0, 0, sourceImageDelayLineBuffer, 0, writePos, numSamplesTail );
                sourceImageBufferTemp.copyFrom(0, numSamplesTail, sourceImageDelayLineBuffer, 0, 0, localAudioBuffer.getNumSamples() - numSamplesTail);
            }
            
            
            // apply gain based on source image path length
            float gainDelayLine = fmin(1.0, fmax(0.0, 1.0 / sourceImagePathLengthsInMeter[j] ) );
            sourceImageBufferTemp.applyGain(0, 0, localAudioBuffer.getNumSamples(), gainDelayLine);
            
            
            // octave filer bank decomposition
            
            octaveFilterBuffer.clear();
            auto sourceImageabsorption = oscHandler.getSourceImageAbsorbtion(sourceImageIDs[j]);
            
            for (int k = 0; k < NUM_OCTAVE_BANDS; k++)
            {
                
                // duplicate to get working copy
                octaveFilterBufferTemp = sourceImageBufferTemp;
                
                // decompose
                octaveFilterBank[k].processSamples(octaveFilterBufferTemp.getWritePointer(0), localAudioBuffer.getNumSamples());
                
                // get gain
                float octaveFreqGain = sourceImageabsorption[k] / NUM_OCTAVE_BANDS; // DUMMY
                
                // apply frequency specific absorption gains
                octaveFilterBufferTemp.applyGain(0, 0, localAudioBuffer.getNumSamples(), octaveFreqGain);
                
                // sum to output (recompose)
                octaveFilterBuffer.addFrom(0, 0, octaveFilterBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            }
            
            // DEBUG: sum sources images signals (with filter bank)
            sourceImageBuffer.addFrom(0, 0, octaveFilterBuffer, 0, 0, localAudioBuffer.getNumSamples());
            
            
            // DEBUG: sum sources images signals (without filter bank)
            // sourceImageBuffer.addFrom(0, 0, sourceImageBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            
            
            // Spatialization: Ambisonic encoding
            for (int k = 0; k < N_AMBI_CH; k++)
            {
                // create working copy
                ambisonicBufferTemp = octaveFilterBuffer;
                
                // apply ambisonic gain
                ambisonicBufferTemp.applyGain(0, 0, localAudioBuffer.getNumSamples(), sourceImageAmbisonicGains[j][k]);
                
                // fill in general ambisonic buffer
                ambisonicBuffer.addFrom(k, 0, ambisonicBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            }
            
            // Spatialization: Ambisonic decoding + virtual speaker approach + binaural
            
            
            
            sourceImageBufferTemp.clear();
            
        }
        
        // increment write position, apply circular shift if needed
        
        
        sourceImageDelayLineWriteIndex += localAudioBuffer.getNumSamples();
        if (sourceImageDelayLineWriteIndex >= sourceImageDelayLineBuffer.getNumSamples())
            sourceImageDelayLineWriteIndex = sourceImageDelayLineWriteIndex - sourceImageDelayLineBuffer.getNumSamples();
    }

    // DEBUG: add delay to output
    localAudioBuffer.addFrom(0, 0, sourceImageBuffer, 0, 0, localAudioBuffer.getNumSamples());
    
    //==========================================================================
    // get write pointer to output
    auto outL = localAudioBuffer.getWritePointer(0);
    auto outR = localAudioBuffer.getWritePointer(1);
    // Loop over samples
    for (int i = 0; i < bufferLength; i++)
    {
        // DEBUG PRECAUTION
        outL[i] = clipOutput(outL[i]);
        outR[i] = clipOutput(outL[i]);
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
    // save source image data here, that way even if they change between now nd the next audio loop
    // these will be used
    sourceImageIDs = oscHandler.getSourceImageIDs();
    sourceImageDelaysInSeconds = oscHandler.getSourceImageDelays();
    sourceImagePathLengthsInMeter = oscHandler.getSourceImagePathsLength();
    
    // compute new ambisonic gains
    auto sourceImageDOAs = oscHandler.getSourceImageDOAs();
    sourceImageAmbisonicGains.resize(sourceImageDOAs.size());
    for (int i = 0; i < sourceImageDOAs.size(); i++)
    {
        sourceImageAmbisonicGains[i] = ambisonicEncoder.calcParams(sourceImageDOAs[i].azimuth, sourceImageDOAs[i].elevation);
    }
    
    // flag update for next audio loop run to eventually resize delay line
    requireSourceImageDelayLineSizeUpdate = true;
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colour(PixelARGB(240,30,30,30)));
    
    
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
