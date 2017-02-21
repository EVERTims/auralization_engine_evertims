
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
    
    addAndMakeVisible (numFrequencyBandsLabel);
    numFrequencyBandsLabel.setText ("Num. absorption freq. bands", dontSendNotification);
    numFrequencyBandsLabel.setColour(Label::textColourId, Colours::whitesmoke);
    
    addAndMakeVisible (&reverbTailToggle);
    reverbTailToggle.setButtonText ("Reverb tail");
    reverbTailToggle.setColour(ToggleButton::textColourId, Colours::whitesmoke);
    reverbTailToggle.setEnabled(true);
    reverbTailToggle.addListener(this);
    reverbTailToggle.setToggleState(true, juce::sendNotification);
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

// Audio Processing
void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    
    // fill buffer with audiofile data
    audioIOComponent.getNextAudioBlock(bufferToFill);
    
    workingBuffer.copyFrom(0, 0, bufferToFill.buffer->getWritePointer(0), workingBuffer.getNumSamples());
    
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
            ambi2binFilters[ 2*k ].process(ambisonicBuffer.getWritePointer(k)); // left
            ambi2binFilters[2*k+1].process(ambisonicBuffer2ndEar.getWritePointer(k)); // right

            // collapse left channel, collapse right channel
            if (k > 0)
            {
                ambisonicBuffer.addFrom(0, 0, ambisonicBuffer.getWritePointer(k), workingBuffer.getNumSamples());
                ambisonicBuffer2ndEar.addFrom(0, 0, ambisonicBuffer2ndEar.getWritePointer(k), workingBuffer.getNumSamples());
            }
        }
        
        // final rewrite to output buffer
        bufferToFill.buffer->copyFrom(0, 0, ambisonicBuffer, 0, 0, workingBuffer.getNumSamples());
        bufferToFill.buffer->copyFrom(1, 0, ambisonicBuffer2ndEar, 0, 0, workingBuffer.getNumSamples());
        
    }
    else
    {
        //==========================================================================
        // if no source image, simply rewrite to output buffer (TODO: remove stupid double copy)
        bufferToFill.buffer->copyFrom(0, 0, workingBuffer, 0, 0, workingBuffer.getNumSamples());
        bufferToFill.buffer->copyFrom(1, 0, workingBuffer, 0, 0, workingBuffer.getNumSamples());
    }
    
    
    //==========================================================================
    // get write pointer to output
    auto outL = bufferToFill.buffer->getWritePointer(0);
    auto outR = bufferToFill.buffer->getWritePointer(1);
    // Loop over samples
    for (int i = 0; i < workingBuffer.getNumSamples(); i++)
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
    
    audioIOComponent.transportSource.releaseResources();
    
    // clear delay line buffers
    delayLine.buffer.clear();
    delayLine.chunkBuffer.clear();
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
    
    // logo image
    g.drawImageAt(logoImage, (int)( (getWidth()/2) - (logoImage.getWidth()/2) ), (int)( ( getHeight()/1.7) - (logoImage.getHeight()/2) ));
    
    // signature
    g.setColour(Colours::white);
    g.setFont(11.f);
    g.drawFittedText("Designed by D. Poirier-Quinot & M. Noisternig, IRCAM, 2016", getWidth() - 285, getHeight()-15, 275, 15, Justification::right, 2);
    
}

void MainContentComponent::resized()
{
    // resize sub-components
    audioIOComponent.setBounds(10, 10, getWidth()-20, 160);
    
    // resize local GUI elements
    
    int thirdWidthIoComponent = (int)((getWidth() - 20)/ 3) - 20; // lazy to change all to add that to audioIOComponent GUI for now
    saveIrButton.setBounds(getWidth() - thirdWidthIoComponent - 30, 80, thirdWidthIoComponent, 40);
    logTextBox.setBounds (8, 180, getWidth() - 16, getHeight() - 195);
    
    numFrequencyBandsComboBox.setBounds(getWidth() - 70, 140, 50, 20);
    numFrequencyBandsLabel.setBounds(getWidth() - 250, 140, 180, 20);
    
    reverbTailToggle.setBounds(getWidth() - 380, 140, 120, 20);
    
    gainReverbTailSlider.setBounds (getWidth() - 500, 110, 250, 20);
}

void MainContentComponent::changeListenerCallback (ChangeBroadcaster* broadcaster)
{
    if (broadcaster == &oscHandler)
    {
        logTextBox.setText(oscHandler.getMapContent());
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
            audioIOComponent.saveIR(sourceImagesHandler.getCurrentIR(), localSampleRate);
        }
        else {
            AlertWindow::showMessageBoxAsync ( AlertWindow::NoIcon, "Impulse Response not saved", "No source images registered from raytracing client \n(Empty IR)", "OK");
        }
    }
    if( button == &reverbTailToggle )
    {
        sourceImagesHandler.enableReverbTail = reverbTailToggle.getToggleState();
        updateOnOscReveive(localSampleRate); // require delay line size update
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
}

//==============================================================================
// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
