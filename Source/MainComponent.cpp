
#include "MainComponent.h"
#include "Utils.h"

MainContentComponent::MainContentComponent():
audioInputComponent(),
oscHandler(),
delayLine(),
ambi2binContainer()
{
    // set window dimensions
    setSize (650, 700);
    
    // specify the required number of input and output channels
    setAudioChannels (2, 2);
    
    // add to change listeners
    oscHandler.addChangeListener(this);
    
    //==========================================================================
    // INIT GUI ELEMENTS
    
    // add GUI sub-components
    addAndMakeVisible(audioInputComponent);
    
    // local GUI elements
    addAndMakeVisible (&saveIrButton);
    saveIrButton.setButtonText ("Save IR");
    saveIrButton.addListener (this);
    saveIrButton.setColour (TextButton::buttonColourId, Colours::grey);
    saveIrButton.setEnabled (false); // not yet implemented
    
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
}

MainContentComponent::~MainContentComponent()
{
    // fix denied access at close when sound playing,
    // see https://forum.juce.com/t/tutorial-playing-sound-files-raises-an-exception-on-2nd-load/15738/2
    audioInputComponent.transportSource.setSource(nullptr);
    
    shutdownAudio();
}

//==============================================================================
void MainContentComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.
    
    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.
    
    //==========================================================================
    // INIT MISC.
    
    // audio file reader
    audioInputComponent.transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    
    // working buffer
    localAudioBuffer.setSize(1, samplesPerBlockExpected);
    
    // source image buffers
    sourceImageBuffer.setSize(1, samplesPerBlockExpected);
    sourceImageBuffer.clear();
    sourceImageBufferTemp = sourceImageBuffer;
    delayLineCrossfadeBuffer = sourceImageBuffer;
    
    // keep track of sample rate
    localSampleRate = sampleRate;
    
    //==========================================================================
    // INIT DELAY LINE
    delayLine.initBufferSize(samplesPerBlockExpected);
    
    //==========================================================================
    // INIT FILTER BANK
    double f0 = 31.5;
    double Q = sqrt(2) / (2 - 1);
    float gainFactor = 1.0;
    for( int i = 0; i < NUM_OCTAVE_BANDS; i++ )
    {
        f0 *= 2;
        octaveFilterBank[i].setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, f0, Q, gainFactor));
    }
    
    octaveFilterBuffer.setSize(1, samplesPerBlockExpected);
    octaveFilterBuffer.clear();
    octaveFilterBufferTemp = octaveFilterBuffer;
    
    //==========================================================================
    // INIT AMBISONIC (encoder / decoder)
    ambisonicBuffer.setSize(N_AMBI_CH, samplesPerBlockExpected);
    ambisonicBuffer2ndEar.setSize(N_AMBI_CH, samplesPerBlockExpected);
    
    // init ambi 2 bin decoding
    // fill in data in ABIR filtered and ABIR filter themselves
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
    audioInputComponent.getNextAudioBlock (bufferToFill);
    
    localAudioBuffer.copyFrom(0, 0, bufferToFill.buffer->getWritePointer(0), localAudioBuffer.getNumSamples());
    
    //==========================================================================
    // SOURCE IMAGE PROCESSING
    sourceImageBuffer.clear();
    ambisonicBuffer.clear();
    
    if ( sourceImageIDs.size() > 0 )
    {
        
        //==========================================================================
        // DELAY LINE
        
        // update delay line size if need be (TODO: MOVE THIS .SIZE() OUTSIDE OF AUDIO PROCESSING LOOP
        if ( requireSourceImageDelayLineSizeUpdate )
        {
            // get maximum required delay line duration
            float maxDelay = *std::max_element(std::begin(sourceImageFutureDelaysInSeconds), std::end(sourceImageFutureDelaysInSeconds)); // in sec
            
            // get associated required delay line buffer length
            int updatedDelayLineLength = (int)( 1.5 * maxDelay * localSampleRate); // longest delay creates noisy sound if delay line is exactly 1* its duration
            
            // update delay line size
            delayLine.setSize(updatedDelayLineLength);
            
            // unflag update required
            requireSourceImageDelayLineSizeUpdate = false;
        }
        
        // add current audio buffer to delay line
        delayLine.addFrom(localAudioBuffer, 0, 0, localAudioBuffer.getNumSamples());
        
        // handle crossfade gain mecanism
        if( crossfadeGain < 1.0 )
        {
            // either increment delay line crossfade
            crossfadeGain += 0.1;
        }
        else if (!crossfadeOver)
        {
            // or stop crossfade mecanism
            sourceImageDelaysInSeconds = sourceImageFutureDelaysInSeconds;
            sourceImageAmbisonicGains = sourceImageAmbisonicFutureGains;
            crossfadeGain = 1.0; // just to make sure for the last loop using crossfade gain
            crossfadeOver = true;
        }
        
        //==========================================================================
        // LOOP OVER SOURCE IMAGES (to apply delay + room coloration + spatialization)
        
        for( int j = 0; j < sourceImageIDs.size(); j++ )
        {
         
            //==========================================================================
            // CROSSFADE DELAYs
            float delayInFractionalSamples = 0.0;
            if( !crossfadeOver ) // TODO: BEware, this could happend in multithread while delay have been updated here and not yet taken into account above
            {
                // Add old and new tapped delayed buffers with gain crossfade
                
                // old delay
                delayInFractionalSamples = sourceImageDelaysInSeconds[j] * localSampleRate;
                
                sourceImageBufferTemp.copyFrom(0, 0, delayLine.getInterpolatedChunk(localAudioBuffer.getNumSamples(), delayInFractionalSamples), 0, 0, localAudioBuffer.getNumSamples());
                
                sourceImageBufferTemp.applyGain(1.0 - crossfadeGain);
                
                // new delay
                delayInFractionalSamples = sourceImageFutureDelaysInSeconds[j] * localSampleRate;
                
                delayLineCrossfadeBuffer.copyFrom(0, 0, delayLine.getInterpolatedChunk(localAudioBuffer.getNumSamples(), delayInFractionalSamples), 0, 0, localAudioBuffer.getNumSamples());
                
                delayLineCrossfadeBuffer.applyGain(crossfadeGain);
                
                // add both
                sourceImageBufferTemp.addFrom(0, 0, delayLineCrossfadeBuffer, 0, 0, localAudioBuffer.getNumSamples());
                
                
                // if (j==0) DBG(String(j) + String(": ") + String(sourceImageFutureDelaysInSeconds[j] - sourceImageDelaysInSeconds[j] ));
            }
            else // simple update
            {
                delayInFractionalSamples = (sourceImageDelaysInSeconds[j] * localSampleRate);
                
                sourceImageBufferTemp.copyFrom(0, 0, delayLine.getInterpolatedChunk(localAudioBuffer.getNumSamples(), delayInFractionalSamples), 0, 0, localAudioBuffer.getNumSamples());
            }

            
            // apply gain based on source image path length
            float gainDelayLine = fmin(1.0, fmax(0.0, 1.0 / sourceImagePathLengthsInMeter[j] ) );
            sourceImageBufferTemp.applyGain(0, 0, localAudioBuffer.getNumSamples(), gainDelayLine);
            
            // DEBUG: sum sources images signals (without filter bank)
            // sourceImageBuffer.addFrom(0, 0, sourceImageBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            
            //==========================================================================
            // octave filer bank decomposition
            octaveFilterBuffer.clear();
            auto sourceImageabsorption = oscHandler.getSourceImageAbsorbtion(sourceImageIDs[j]);
            
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
            
            for( int k = 0; k < N_AMBI_CH; k++ )
            {
                // create working copy
                ambisonicBufferTemp = octaveFilterBuffer;
                
                if( !crossfadeOver )
                {
                    // apply ambisonic gain past
                    ambisonicBufferTemp.applyGain((1.0 - crossfadeGain)*sourceImageAmbisonicGains[j][k]);
                    
                    // create 2nd working copy
                    ambisonicCrossfadeBufferTemp = octaveFilterBuffer;
                    
                    // apply ambisonic gain future
                    ambisonicCrossfadeBufferTemp.applyGain(crossfadeGain*sourceImageAmbisonicFutureGains[j][k]);
                    
                    // add past / future buffers
                    ambisonicBufferTemp.addFrom(0, 0, ambisonicCrossfadeBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
//                    if(j==2 && k==2) DBG(String(crossfadeGain) + String(" ") + String(sourceImageAmbisonicGains[j][k]) + String(" ") + String(sourceImageAmbisonicFutureGains[j][k]));
                }
                else
                {
                    // apply ambisonic gain
                    ambisonicBufferTemp.applyGain(sourceImageAmbisonicGains[j][k]);
                }
                
                // fill in general ambisonic buffer
                ambisonicBuffer.addFrom(k, 0, ambisonicBufferTemp, 0, 0, localAudioBuffer.getNumSamples());
            }
            
        }
        
        
        // increment delay line write position
        delayLine.incrementWritePosition(localAudioBuffer.getNumSamples());
        
        
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
    
    audioInputComponent.transportSource.releaseResources();
    
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

// to rename, not only size but path length update + ambisonic gains.. finally it's an OSC update thingy
void MainContentComponent::updateSourceImageDelayLineSize(int sampleRate)
{
    // lame mecanism to avoid changing future gains if crossfade not over yet
    while(!crossfadeOver) sleep(0.001);
    
    // save new source image data, ready to be used in next audio loop
    sourceImageIDs = oscHandler.getSourceImageIDs();
    // sourceImageDelaysInSeconds = oscHandler.getSourceImageDelays();
    sourceImageFutureDelaysInSeconds = oscHandler.getSourceImageDelays();
    sourceImageDelaysInSeconds.resize(sourceImageFutureDelaysInSeconds.size(), 0.0f);
    sourceImagePathLengthsInMeter = oscHandler.getSourceImagePathsLength();
    
    // compute new ambisonic gains
    auto sourceImageDOAs = oscHandler.getSourceImageDOAs();
    sourceImageAmbisonicGains.resize(sourceImageDOAs.size());
    sourceImageAmbisonicFutureGains.resize(sourceImageDOAs.size());
    for (int i = 0; i < sourceImageDOAs.size(); i++)
    {
        sourceImageAmbisonicFutureGains[i] = ambisonicEncoder.calcParams(sourceImageDOAs[i](0), sourceImageDOAs[i](1));
    }
    
    // TODO: update absorption coefficients here as well to be consistent
    
    // reset crossfade gain
    crossfadeGain = 0.0;
    crossfadeOver = false;
    
    // now that everything is ready: set update flag, to resize delay line at next audio loop
    requireSourceImageDelayLineSizeUpdate = true;
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    g.fillAll (Colour(PixelARGB(240,30,30,30)));
}

void MainContentComponent::resized()
{
    // resize sub-components
    audioInputComponent.setBounds(10, 10, getWidth()-20, 160);
    
    // resize local GUI elements
    int thirdWidth = (int)(getWidth() / 3) - 20;
    saveIrButton.setBounds(getWidth() - thirdWidth - 20, 140, thirdWidth, 30);
    logTextBox.setBounds (8, 180, getWidth() - 16, getHeight() - 190);
}

void MainContentComponent::changeListenerCallback (ChangeBroadcaster* broadcaster)
{
    if (broadcaster == &oscHandler)
    {
        logTextBox.setText(oscHandler.getMapContent());
        updateSourceImageDelayLineSize(localSampleRate);
    }
}

//==============================================================================

void MainContentComponent::buttonClicked (Button* button)
{
    if (button == &saveIrButton)
    {
        saveIR();
    }
}
//==============================================================================

void MainContentComponent::saveIR ()
{
    
}

//==============================================================================
// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
