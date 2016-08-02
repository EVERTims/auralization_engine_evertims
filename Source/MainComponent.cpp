
#include "MainComponent.h"
#include "Utils.h"
#include <iomanip> // for debug, to remove

MainContentComponent::MainContentComponent()
: oscHandler()
, ambi2binContainer()
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
    
    localAudioBuffer.setSize(1, samplesPerBlockExpected);
    
    // init delay line (define single channel buffer and dummy number of samples)
    // updateSourceImageDelayLineSize(sampleRate);
    sourceImageDelayLineBuffer.setSize(1, (int)(samplesPerBlockExpected));
    sourceImageDelayLineBuffer.clear();
    // sourceImageDelayLineBufferReplacement = sourceImageDelayLineBuffer;
    sourceImageBufferTemp.clear();
    sourceImageBuffer.clear();
    
    // init ambi 2 bin decoding
    // fill in data in ABIR filtered and ABIR filter themselves
    for (int i = 0; i < N_AMBI_CH; i++)
    {
        ambi2binFilters[ 2*i ].init(samplesPerBlockExpected, AMBI2BIN_IR_LENGTH);
        ambi2binFilters[ 2*i ].setImpulseResponse(ambi2binContainer.ambi2binIrDict[i][0].data()); // [ch x ear x sampID]
        ambi2binFilters[2*i+1].init(samplesPerBlockExpected, AMBI2BIN_IR_LENGTH);
        ambi2binFilters[2*i+1].setImpulseResponse(ambi2binContainer.ambi2binIrDict[i][1].data()); // [ch x ear x sampID]
    }
    
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
    ambisonicBuffer2ndEar.setSize(N_AMBI_CH, samplesPerBlockExpected);
    
    
    // USEFULL ???
    vectorBufferOut[0].resize(samplesPerBlockExpected);
    vectorBufferOut[1].resize(samplesPerBlockExpected);
    
    //==========================================================================
    // init filter bank
    octaveFilterBuffer.setSize(1, samplesPerBlockExpected);
    octaveFilterBuffer.clear();
    octaveFilterBufferTemp = octaveFilterBuffer;
    
    //==========================================================================
    // init delay lines (for sourceImages)
    sourceImageBuffer.setSize(1, samplesPerBlockExpected);
    sourceImageBuffer.clear();
    sourceImageBufferTemp = sourceImageBuffer;
    
    //==========================================================================
    // init ambisonic
    // ambisonicBuffer = localAudioBuffer; // set size
    // ambisonicBuffer.setSize(N_AMBI_CH, localAudioBuffer.getNumSamples()); // to make sure correct number of samples
    ambisonicBuffer.clear();
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
    
    // auto bufferLength = localAudioBuffer.getNumSamples();
    
    // get working buffer (stereo downmix to mono)
    // localAudioBuffer = *bufferToFill.buffer;
    localAudioBuffer.copyFrom(0, 0, bufferToFill.buffer->getWritePointer(0), localAudioBuffer.getNumSamples());

    localAudioBuffer.addFrom(0, 0, bufferToFill.buffer->getWritePointer(1), localAudioBuffer.getNumSamples());
    localAudioBuffer.applyGain(0.5f);
 
    // apply master gain
    localAudioBuffer.applyGain(0, 0, localAudioBuffer.getNumSamples(), gainMasterSlider.getValue());
    
    //==========================================================================
    // SOURCE IMAGE PROCESSING
    sourceImageBuffer.clear();
    ambisonicBuffer.clear();
    
    if ( sourceImageDelaysInSeconds.size() > 0 )
    {
        
        //==========================================================================
        // DELAY LINE
        
        // update delay line size if need be (TODO: MOVE THIS .SIZE() OUTSIDE OF AUDIO PROCESSING LOOP
        if (requireSourceImageDelayLineSizeUpdate)
        {
            // get maximum required delay line duration
            float maxDelay = *std::max_element(std::begin(sourceImageDelaysInSeconds), std::end(sourceImageDelaysInSeconds)); // in sec
            
            // get associated required delay line buffer length
            int updatedDelayLineLength = (int)( 1.5 * maxDelay * localSampleRate); // longest delay creates noisy sound if delay line is exactly 1* its duration
            
            // update delay line size if need be (no shrinking delay line for now)
            if (updatedDelayLineLength > sourceImageDelayLineBuffer.getNumSamples())
            {
                sourceImageDelayLineBuffer.setSize(1, updatedDelayLineLength, true, true);
            }
            
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
        //==========================================================================
        
        //==========================================================================
        // LOOP OVER SOURCE IMAGES (to apply delay + room coloration + spatialization)
        
        for (int j = 0; j < sourceImageDelaysInSeconds.size(); j++) // sourceImageDelaysInSeconds.size()
        {
         
            //==========================================================================
            // get delayed buffer corresponding to current source image out of delay line
            
            int writePos = sourceImageDelayLineWriteIndex - (int) (sourceImageDelaysInSeconds[j] * localSampleRate);
            
            if ( writePos < 0 )
            {
                writePos = sourceImageDelayLineBuffer.getNumSamples() + writePos;
                if ( writePos < 0 ) // if after an update the first delay force to go fetch far to far: not best option yet (to set write pointer to zero)
                    writePos = 0;
            }
            
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
            
            //==========================================================================
            
            // apply gain based on source image path length
            float gainDelayLine = fmin(1.0, fmax(0.0, 1.0 / sourceImagePathLengthsInMeter[j] ) );
            sourceImageBufferTemp.applyGain(0, 0, localAudioBuffer.getNumSamples(), gainDelayLine);
            
            // DEBUG: sum sources images signals (without filter bank)
            // sourceImageBuffer.addFrom(0, 0, sourceImageBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            
            //==========================================================================
            // octave filer bank decomposition
            octaveFilterBuffer.clear();
            auto sourceImageabsorption = oscHandler.getSourceImageAbsorbtion(sourceImageIDs[j]);
            // DBG("----");
            
            for( int k = 0; k < NUM_OCTAVE_BANDS; k++ )
            {
                
                // duplicate to get working copy
                octaveFilterBufferTemp.copyFrom(0, 0, sourceImageBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
                
                // decompose
                octaveFilterBank[k].processSamples(octaveFilterBufferTemp.getWritePointer(0), localAudioBuffer.getNumSamples());
                
                // get gain
                float octaveFreqGain = fmin(abs(1.0 - sourceImageabsorption[k]), 1.0);
                // DBG(octaveFreqGain);
                
                // apply frequency specific absorption gains
                octaveFilterBufferTemp.applyGain(0, 0, localAudioBuffer.getNumSamples(), octaveFreqGain);
                
                // sum to output (recompose)
                octaveFilterBuffer.addFrom(0, 0, octaveFilterBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            }
            
            
            // sourceImageBuffer.addFrom(0, 0, octaveFilterBuffer, 0, 0, localAudioBuffer.getNumSamples());
        
            // sourceImageBufferTemp.clear(); // no need to clear when using .copyFrom (rather than .addFrom)

            
            //==========================================================================
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
            
        }
        

        //==========================================================================
        // Delay line: increment write position, apply circular shift if needed
        sourceImageDelayLineWriteIndex += localAudioBuffer.getNumSamples();
        if (sourceImageDelayLineWriteIndex >= sourceImageDelayLineBuffer.getNumSamples())
            sourceImageDelayLineWriteIndex = sourceImageDelayLineWriteIndex - sourceImageDelayLineBuffer.getNumSamples();
        
        //==========================================================================
        // Spatialization: Ambisonic decoding + virtual speaker approach + binaural

        // DEBUG: check Ambisonic gains
         String debugLog = "";
         for (int k = 0; k < N_AMBI_CH; k++) {// N_AMBI_CH
             debugLog += String(k) + String(": ") + String(round2(sourceImageAmbisonicGains[0][k], 2)) + String("\t ");
         }
         DBG(debugLog);

        
        // duplicate channel before filtering for two ears
        ambisonicBuffer2ndEar = ambisonicBuffer;
        
        // loop over Ambisonic channels
        for (int k = 0; k < N_AMBI_CH; k++)
        {
            ambi2binFilters[ 2*k ].process(ambisonicBuffer.getWritePointer(k)); // left
            ambi2binFilters[2*k+1].process(ambisonicBuffer2ndEar.getWritePointer(k)); // right

            // collapse left channel, collapse right channel
            if (k > 0)
            {
                ambisonicBuffer.addFrom(0, 0, ambisonicBuffer.getWritePointer(k), localAudioBuffer.getNumSamples());
                ambisonicBuffer2ndEar.addFrom(0, 0, ambisonicBuffer2ndEar.getWritePointer(k), localAudioBuffer.getNumSamples());
            }
        }

        
        //==========================================================================

        
        // DEBUG: recopy non modified signal
        // bufferToFill.buffer->copyFrom(0, 0, localAudioBuffer, 0, 0, localAudioBuffer.getNumSamples());
        // bufferToFill.buffer->copyFrom(1, 0, localAudioBuffer, 0, 0, localAudioBuffer.getNumSamples());
        
        // DEBUG: sum sources images signals (delays)
        // bufferToFill.buffer->copyFrom(0, 0, sourceImageBuffer, 0, 0, localAudioBuffer.getNumSamples());
        // bufferToFill.buffer->copyFrom(1, 0, sourceImageBuffer, 0, 0, localAudioBuffer.getNumSamples());
        
        // DEBUG: sum sources images signals (delays + absorption filter)
        // bufferToFill.buffer->copyFrom(0, 0, octaveFilterBuffer, 0, 0, localAudioBuffer.getNumSamples());
        // bufferToFill.buffer->copyFrom(1, 0, octaveFilterBuffer, 0, 0, localAudioBuffer.getNumSamples());
        
        // DEBUG: ouptut ambisonic gains (delay + absorption filter + ambisonic encode + crude downmix to L/R channels)
        
        // final rewrite to output buffer
        bufferToFill.buffer->copyFrom(0, 0, ambisonicBuffer, 0, 0, localAudioBuffer.getNumSamples());
        bufferToFill.buffer->copyFrom(1, 0, ambisonicBuffer2ndEar, 0, 0, localAudioBuffer.getNumSamples());
        

        
    }
    else
    {
        //==========================================================================
        // if no source image, simply rewrite to output buffer (DAMN USELES NO?)
        bufferToFill.buffer->copyFrom(0, 0, localAudioBuffer, 0, 0, localAudioBuffer.getNumSamples());
        bufferToFill.buffer->copyFrom(1, 0, localAudioBuffer, 0, 0, localAudioBuffer.getNumSamples());
    }
    
    // DEBUG: check output buffer magnitude
    // auto mag = bufferToFill.buffer->getMagnitude(0, localAudioBuffer.getNumSamples());
    // if ( mag > 0.5 ) DBG(mag);
    
    //==========================================================================
    // get write pointer to output
    auto outL = bufferToFill.buffer->getWritePointer(0);
    auto outR = bufferToFill.buffer->getWritePointer(1);
    // Loop over samples
    for (int i = 0; i < localAudioBuffer.getNumSamples(); i++)
    {
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
    {
        return sign(input)*fmin(std::abs(input), 1.0f);
        DBG(String("clip: ") + String(input));
    }
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
    int thirdWidth = (int)(getWidth() / 3) - 20;
    audioFileOpenButton.setBounds (20, 10, thirdWidth, 40);
    audioFilePlayButton.setBounds (30 + thirdWidth, 10, thirdWidth, 40);
    audioFileStopButton.setBounds (40 + 2*thirdWidth, 10, thirdWidth, 40);
    audioFileLoopToogle.setBounds (10, 60, getWidth() - 20, 20);
    gainMasterSlider.setBounds    (10, 90, getWidth() - 20, 20);
    
    audioFileCurrentPositionLabel.setBounds (10, 120, getWidth() - 20, 20);
    
    // liveAudioScroller->setBounds (10, 220, getWidth() - 20, 64);
    
    logTextBox.setBounds (8, 150, getWidth() - 16, getHeight() - 160);
    
    
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
