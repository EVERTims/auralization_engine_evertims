
#include "MainComponent.h"
#include "Utils.h"


MainContentComponent::MainContentComponent():
oscHandler(),
audioIOComponent(),
delayLine(),
sourceImagesHandler(),
ambi2binContainer()
{
    // set window dimensions
    setSize (650, 700);
    
    // specify the required number of input and output channels
    setAudioChannels (2, 2);
    
    // add to change listeners
    oscHandler.addChangeListener(this);
    
    // add audioIOComponent as addAudioCallback for adc input
    deviceManager.addAudioCallback(&audioIOComponent);
    
    //==========================================================================
    // INIT GUI ELEMENTS
    
    // add GUI sub-components
    addAndMakeVisible(audioIOComponent);
    
    // setup logo image
    logoImage = ImageCache::getFromMemory(BinaryData::evertims_logo_512_png, BinaryData::evertims_logo_512_pngSize);
    logoImage = logoImage.rescaled(logoImage.getWidth()/2, logoImage.getHeight()/2);
    
    // local GUI elements
    saveIrButton.setButtonText ("Save IR to Desktop");
    saveIrButton.addListener (this);
    saveIrButton.setColour (TextButton::buttonColourId, Colours::darkgrey);
    saveIrButton.setEnabled (true);
    addAndMakeVisible (&saveIrButton);
    
    addAndMakeVisible (logTextBox);
    logTextBox.setMultiLine (true);
    logTextBox.setReturnKeyStartsNewLine (true);
    logTextBox.setReadOnly (true);
    logTextBox.setScrollbarsShown (true);
    logTextBox.setCaretVisible (false);
    logTextBox.setPopupMenuEnabled (true);
    logTextBox.setColour (TextEditor::textColourId, Colours::whitesmoke);
    logTextBox.setColour (TextEditor::backgroundColourId, Colour(PixelARGB(200,30,30,30)));
    logTextBox.setColour (TextEditor::outlineColourId, Colours::whitesmoke);
    logTextBox.setColour (TextEditor::shadowColourId, Colours::darkorange);
    
    addAndMakeVisible(numFrequencyBandsComboBox);
    numFrequencyBandsComboBox.setEditableText (false);
    numFrequencyBandsComboBox.setJustificationType (Justification::centred);
    numFrequencyBandsComboBox.setColour(ComboBox::backgroundColourId, Colour(PixelARGB(200,30,30,30)));
    numFrequencyBandsComboBox.setColour(ComboBox::buttonColourId, Colour(PixelARGB(200,30,30,30)));
    numFrequencyBandsComboBox.setColour(ComboBox::outlineColourId, Colour(PixelARGB(200,30,30,30)));
    numFrequencyBandsComboBox.setColour(ComboBox::textColourId, Colours::whitesmoke);
    numFrequencyBandsComboBox.addListener(this);
    numFrequencyBandsComboBox.addItem("3", 1);
    numFrequencyBandsComboBox.addItem("10", 2);
    numFrequencyBandsComboBox.setSelectedId(1);
    
    addAndMakeVisible(gainReverbTailSlider);
    gainReverbTailSlider.setRange(0.0, 2.0);
    gainReverbTailSlider.setValue(1.0);
    gainReverbTailSlider.setSliderStyle(Slider::LinearHorizontal);
    gainReverbTailSlider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    gainReverbTailSlider.setColour(Slider::textBoxTextColourId, Colours::white);
    gainReverbTailSlider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    gainReverbTailSlider.setTextBoxStyle(Slider::TextBoxRight, true, 70, 20);
    gainReverbTailSlider.addListener(this);
    
    addAndMakeVisible(gainDirectPathSlider);
    gainDirectPathSlider.setRange(0.0, 2.0);
    gainDirectPathSlider.setValue(1.0);
    gainDirectPathSlider.setSliderStyle(Slider::LinearHorizontal);
    gainDirectPathSlider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    gainDirectPathSlider.setColour(Slider::textBoxTextColourId, Colours::white);
    gainDirectPathSlider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    gainDirectPathSlider.setTextBoxStyle(Slider::TextBoxRight, true, 70, 20);
    gainDirectPathSlider.addListener(this);
    
    addAndMakeVisible (numFrequencyBandsLabel);
    numFrequencyBandsLabel.setText ("Num. absorption freq. bands", dontSendNotification);
    numFrequencyBandsLabel.setColour(Label::textColourId, Colours::whitesmoke);

    addAndMakeVisible (inputLabel);
    inputLabel.setText ("Inputs", dontSendNotification);
    inputLabel.setColour(Label::textColourId, Colours::whitesmoke);
    inputLabel.setColour(Label::backgroundColourId, Colour(30, 30, 30));

    addAndMakeVisible (parameterLabel);
    parameterLabel.setText ("Parameters", dontSendNotification);
    parameterLabel.setColour(Label::textColourId, Colours::whitesmoke);
    parameterLabel.setColour(Label::backgroundColourId, Colour(30, 30, 30));

    addAndMakeVisible (logLabel);
    logLabel.setText ("Logs", dontSendNotification);
    logLabel.setColour(Label::textColourId, Colours::whitesmoke);
    logLabel.setColour(Label::backgroundColourId, Colour(30, 30, 30));
    
    addAndMakeVisible (directPathLabel);
    directPathLabel.setText ("Direct path", dontSendNotification);
    directPathLabel.setColour(Label::textColourId, Colours::whitesmoke);
    directPathLabel.setColour(Label::backgroundColourId, Colours::transparentBlack);
    
    addAndMakeVisible (&reverbTailToggle);
    reverbTailToggle.setButtonText ("Reverb tail");
    reverbTailToggle.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    reverbTailToggle.setEnabled(true);
    reverbTailToggle.addListener(this);
    reverbTailToggle.setToggleState(true, juce::sendNotification);

    addAndMakeVisible (&enableDirectToBinaural);
    enableDirectToBinaural.setButtonText ("Direct to binaural");
    enableDirectToBinaural.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    enableDirectToBinaural.setEnabled(true);
    enableDirectToBinaural.addListener(this);
    enableDirectToBinaural.setToggleState(true, juce::sendNotification);
    
    addAndMakeVisible (&enableLog);
    enableLog.setButtonText ("Enable logs");
    enableLog.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    enableLog.setEnabled(true);
    enableLog.addListener(this);
    enableLog.setToggleState(true, juce::sendNotification);

}

MainContentComponent::~MainContentComponent()
{
    // fix denied access at close when sound playing,
    // see https://forum.juce.com/t/tutorial-playing-sound-files-raises-an-exception-on-2nd-load/15738/2
    audioIOComponent.transportSource.setSource(nullptr);
    
    shutdownAudio();
}

//==============================================================================
void MainContentComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.
    // Called on the audio thread, not the GUI thread.
    
    // audio file reader & adc input
    audioIOComponent.prepareToPlay (samplesPerBlockExpected, sampleRate);
    
    // working buffer
    workingBuffer.setSize(1, samplesPerBlockExpected);
    
    // keep track of sample rate
    localSampleRate = sampleRate;
    localSamplesPerBlockExpected = samplesPerBlockExpected;
    
    // init delay line
    delayLine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    sourceImagesHandler.prepareToPlay (samplesPerBlockExpected, sampleRate);
    
    // init ambi 2 bin decoding: fill in data in ABIR filtered and ABIR filter themselves
    for( int i = 0; i < N_AMBI_CH; i++ )
    {
        ambi2binFilters[ 2*i ].init(samplesPerBlockExpected, AMBI2BIN_IR_LENGTH);
        ambi2binFilters[ 2*i ].setImpulseResponse(ambi2binContainer.ambi2binIrDict[i][0].data()); // [ch x ear x sampID]
        ambi2binFilters[2*i+1].init(samplesPerBlockExpected, AMBI2BIN_IR_LENGTH);
        ambi2binFilters[2*i+1].setImpulseResponse(ambi2binContainer.ambi2binIrDict[i][1].data()); // [ch x ear x sampID]
    }
}

// Audio Processing (Dummy)
void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // fill buffer with audiofile data
    audioIOComponent.getNextAudioBlock(bufferToFill);
    
    // execute main audio processing
    if( !isRecordingIr )
    {
        getNextAudioBlockProcess( bufferToFill.buffer );
    }
    else
    {
        bufferToFill.clearActiveBufferRegion();
    }
}

// Audio Processing: split from getNextAudioBlock to use it for recording IR
void MainContentComponent::getNextAudioBlockProcess( AudioBuffer<float> *const audioBufferToFill )
{
    workingBuffer.copyFrom(0, 0, audioBufferToFill->getWritePointer(0), workingBuffer.getNumSamples());
    
    //==========================================================================
    // SOURCE IMAGE PROCESSING
    
    if ( sourceImagesHandler.IDs.size() > 0 )
    {
        
        //==========================================================================
        // DELAY LINE
        
        // update delay line size if need be (TODO: MOVE THIS .SIZE() OUTSIDE OF AUDIO PROCESSING LOOP
        if ( requireDelayLineSizeUpdate )
        {
            // get maximum required delay line duration
            float maxDelay = sourceImagesHandler.getMaxDelayFuture();
            
            // get associated required delay line buffer length
            int updatedDelayLineLength = (int)( 1.5 * maxDelay * localSampleRate); // longest delay creates noisy sound if delay line is exactly 1* its duration
            
            // update delay line size
            delayLine.setSize(1, updatedDelayLineLength);
            
            // unflag update required
            requireDelayLineSizeUpdate = false;
        }
        
        // add current audio buffer to delay line
        delayLine.copyFrom(0, workingBuffer, 0, 0, workingBuffer.getNumSamples());
        
        // loop over sources images, apply delay + room coloration + spatialization
        ambisonicBuffer = sourceImagesHandler.getNextAudioBlock (&delayLine);
        // arbitrary: apply gain to compensate for loss from filterbank, cheaper to apply
        // here than in sourceImagesHandler on each source image individually
        ambisonicBuffer.applyGain( 2.f );
        
        // increment delay line write position
        delayLine.incrementWritePosition(workingBuffer.getNumSamples());
        
        
        //==========================================================================
        // Spatialization: Ambisonic decoding + virtual speaker approach + binaural
        
        // DEBUG: check Ambisonic gains
        // String debugLog = "";
        // for (int k = 0; k < N_AMBI_CH; k++) {// N_AMBI_CH
        //     debugLog += String(k) + String(": ") + String(round2(sourceImageAmbisonicGains[0][k], 2)) + String("\t ");
        // }
        // DBG(debugLog);
        
        // duplicate channel before filtering for two ears
        ambisonicBuffer2ndEar = ambisonicBuffer;
        
        // loop over Ambisonic channels
        for (int k = 0; k < N_AMBI_CH; k++)
        {
            ambi2binFilters[ 2*k ].process(ambisonicBuffer.getWritePointer(k+2)); // left
            ambi2binFilters[2*k+1].process(ambisonicBuffer2ndEar.getWritePointer(k+2)); // right
            
            // collapse left channel, collapse right channel
            ambisonicBuffer.addFrom(0, 0, ambisonicBuffer.getWritePointer(2+k), workingBuffer.getNumSamples());
            ambisonicBuffer2ndEar.addFrom(1, 0, ambisonicBuffer2ndEar.getWritePointer(2+k), workingBuffer.getNumSamples());
        }
        
        // final rewrite to output buffer
        audioBufferToFill->copyFrom(0, 0, ambisonicBuffer, 0, 0, workingBuffer.getNumSamples());
        audioBufferToFill->copyFrom(1, 0, ambisonicBuffer2ndEar, 1, 0, workingBuffer.getNumSamples());
    }
    
    else
    {
        //==========================================================================
        // if no source image, simply rewrite to output buffer (TODO: remove stupid double copy)
        audioBufferToFill->copyFrom(0, 0, workingBuffer, 0, 0, workingBuffer.getNumSamples());
        audioBufferToFill->copyFrom(1, 0, workingBuffer, 0, 0, workingBuffer.getNumSamples());
    }
    
    
    //==========================================================================
    // get write pointer to output
    auto outL = audioBufferToFill->getWritePointer(0);
    auto outR = audioBufferToFill->getWritePointer(1);
    // Loop over samples
    for (int i = 0; i < workingBuffer.getNumSamples(); i++)
    {
        // DEBUG PRECAUTION
        outL[i] = clipOutput(outL[i]);
        outR[i] = clipOutput(outR[i]);
    }
    
}

// record current Room impulse Response to disk
void MainContentComponent::recordIr()
{
    // estimate output buffer size
    auto rt60 = oscHandler.getRT60Values();
    float maxDelay = getMaxValue(rt60);
    int maxDelayInSamp = ceil(maxDelay * localSampleRate);
    DBG(String("max estimated delay: ") + String(maxDelay));
    
    // init
    recordingBufferInput.setSize(2, localSamplesPerBlockExpected);
    recordingBufferInput.clear();
    recordingBufferOutput.setSize(2, 2*maxDelayInSamp);
    recordingBufferOutput.clear();
    
    // prepare impulse response buffer
    recordingBufferInput.getWritePointer(0)[0] = 1.0f;
    
    // clear delay lines / fdn buffers of main thread
    delayLine.clear();
    sourceImagesHandler.reverbTail.clear();
    
    // pass impulse imput into processing loop until IR faded below threshold
    float rms = 1.0f;
    int bufferId = 0;
    while( rms >= 0.0001f || bufferId*localSamplesPerBlockExpected < maxDelayInSamp )
    {
        // clear impulse after first round
        if( bufferId >= 1 ){ recordingBufferInput.clear(); }
        
        // execute main audio processing
        getNextAudioBlockProcess( &recordingBufferInput );
        
        // add to output buffer
        for( int k = 0; k < 2; k++ )
        {
            recordingBufferOutput.addFrom(k, bufferId*localSamplesPerBlockExpected, recordingBufferInput, k, 0, localSamplesPerBlockExpected);
        }
        
        // update current RMS level
        rms = recordingBufferInput.getRMSLevel(0, 0, localSamplesPerBlockExpected) + recordingBufferInput.getRMSLevel(0, 0, localSamplesPerBlockExpected);
        rms *= 0.5;
        
        // increment
        bufferId += 1;
    }
    
    // resize output
    recordingBufferOutput.setSize(2, bufferId*localSamplesPerBlockExpected, true);
    
    // save output
    audioIOComponent.saveIR(recordingBufferOutput, localSampleRate);
    
    // unlock main audio thread
    isRecordingIr = false;
}

void MainContentComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.
    
    audioIOComponent.transportSource.releaseResources();
    
    // clear all "delay line" like buffers
    delayLine.clear();
    sourceImagesHandler.reverbTail.clear();
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

void MainContentComponent::updateOnOscReveive(int sampleRate)
{
    
    // update source images attributes based on latest received OSC info
    sourceImagesHandler.updateFromOscHandler(oscHandler);
    
    // now that everything is ready: set update flag, to resize delay line at next audio loop
    requireDelayLineSizeUpdate = true;
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    // background
    g.fillAll (Colour(PixelARGB(240,30,30,30)));
    
    // parameters box
    g.setOpacity(1.0f);
    g.setColour(Colours::whitesmoke);
    g.drawRoundedRectangle(10.f, 155.f, getWidth()-20.f, 130.f, 0.0f, 1.0f);
    
    // logo image
    g.drawImageAt(logoImage, (int)( (getWidth()/2) - (logoImage.getWidth()/2) ), (int)( ( getHeight()/ 1.45) - (logoImage.getHeight()/2) ));
    
    // signature
    g.setColour(Colours::white);
    g.setFont(11.f);
    g.drawFittedText("Designed by D. Poirier-Quinot & M. Noisternig, IRCAM, 2017", getWidth() - 285, getHeight()-15, 275, 15, Justification::right, 2);
    
}

void MainContentComponent::resized()
{
    // audio IO box
    inputLabel.setBounds(30, 3, 50, 15);
    audioIOComponent.setBounds(10, 10, getWidth()-20, 130);
    
    // parameters box
    int thirdWidthIoComponent = (int)((getWidth() - 20)/ 3) - 20; // lazy to change all to add that to audioIOComponent GUI for now
    
    parameterLabel.setBounds(30, 147, 80, 15);
    
    reverbTailToggle.setBounds(30, 170, 120, 20);
    gainReverbTailSlider.setBounds (180, 170, 440, 20);
    
    directPathLabel.setBounds(30, 200, 120, 20);
    gainDirectPathSlider.setBounds (140, 200, 330, 20);
    enableDirectToBinaural.setBounds (480, 200, 140, 20);

    numFrequencyBandsLabel.setBounds(30, 245, 220, 20);
    numFrequencyBandsComboBox.setBounds(210, 245, 80, 20);

    saveIrButton.setBounds(getWidth() - thirdWidthIoComponent - 30, 240, thirdWidthIoComponent, 30);
    
    // log box
    logLabel.setBounds(30, 286, 40, 20);
    logTextBox.setBounds (8, 300, getWidth() - 16, getHeight() - 316);
    enableLog.setBounds(getWidth() - 110, 300, 100, 30);
}

void MainContentComponent::changeListenerCallback (ChangeBroadcaster* broadcaster)
{
    if (broadcaster == &oscHandler)
    {
        if( enableLog.getToggleState() )
        {
            logTextBox.setText(oscHandler.getMapContent());
        }
        updateOnOscReveive(localSampleRate);
    }
}

//==============================================================================

void MainContentComponent::buttonClicked (Button* button)
{
    if (button == &saveIrButton)
    {
        if ( sourceImagesHandler.IDs.size() > 0 )
        {
            isRecordingIr = true;
            recordIr();
            // audioIOComponent.saveIR(sourceImagesHandler.getCurrentIR(), localSampleRate);
        }
        else {
            AlertWindow::showMessageBoxAsync ( AlertWindow::NoIcon, "Impulse Response not saved", "No source images registered from raytracing client \n(Empty IR)", "OK");
        }
    }
    if( button == &reverbTailToggle )
    {
        sourceImagesHandler.enableReverbTail = reverbTailToggle.getToggleState();
        updateOnOscReveive(localSampleRate); // require delay line size update
        gainReverbTailSlider.setEnabled(reverbTailToggle.getToggleState());
    }
    if( button == &enableDirectToBinaural )
    {
        sourceImagesHandler.enableDirectToBinaural = enableDirectToBinaural.getToggleState();
    }
    if( button == &enableLog )
    {
        if( button->getToggleState() ){ logTextBox.setText(oscHandler.getMapContent()); }
        else{ logTextBox.setText( String("") ); };
    }

}

void MainContentComponent::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == &numFrequencyBandsComboBox)
    {
        int numFreqBands;
        if( numFrequencyBandsComboBox.getSelectedId() == 1 ) numFreqBands = 3;
        else numFreqBands = 10;
        
        sourceImagesHandler.setFilterBankSize(numFreqBands);
    }
}

void MainContentComponent::sliderValueChanged(Slider* slider)
{
    if( slider == &gainReverbTailSlider )
    {
        sourceImagesHandler.reverbTailGain = gainReverbTailSlider.getValue();
    }
    if( slider == &gainDirectPathSlider )
    {
        sourceImagesHandler.directPathGain = gainDirectPathSlider.getValue();
    }
}

//==============================================================================
// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
