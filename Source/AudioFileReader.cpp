//#include "AudioFileReader.h"
//
//AudioFileReader::AudioFileReader()
//{
//    formatManager.registerBasicFormats();
//    // transportSource.addChangeListener (this);
//}
//
//AudioFileReader::~AudioFileReader() {}
//
//void AudioFileReader::openAudioFile(AudioTransportSource& transportSource, AudioFormatReaderSource readerSource)
//{
//    FileChooser chooser ("Select a Wave file to play...",
//                         File::nonexistent,
//                         "*.wav");
//    
//    if (chooser.browseForFileToOpen())
//    {
//        File file (chooser.getResult());
//        AudioFormatReader* reader = formatManager.createReaderFor (file);
//        
//        if (reader != nullptr)
//        {
//            ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
//            transportSource.setSource (newSource, 0, nullptr, reader->sampleRate);
//            &readerSource = newSource.release();
//
//     
////            changeState (Loaded);
////            sendChangeMessage ();
//        }
//    }
//}

//void AudioFileReader::playAudioFile()
//{
//    updateLoopState(loopState);
//    changeState (Starting);
//}
//
//void AudioFileReader::stopAudioFile()
//{
//    changeState (Stopping);
//}
//
//void AudioFileReader::setLoopState(bool shouldLoop)
//{
//    loopState = shouldLoop;
//    updateLoopState(loopState);
//}
//
//void AudioFileReader::updateLoopState(bool shouldLoop)
//{
//    if (readerSource != nullptr)
//        readerSource->setLooping (shouldLoop);
//}
//
//void AudioFileReader::changeListenerCallback (ChangeBroadcaster* broadcaster)
//{
//    if (broadcaster == &transportSource)
//    {
//        if (transportSource.isPlaying())
//            changeState (Playing);
//        else
//            changeState (Stopped);
//    }
//}
//
//void AudioFileReader::changeState (TransportState newState)
//{
//    if (state != newState)
//    {
//        state = newState;
//        
//        switch (state)
//        {
//            case Stopped:
//                // mainContentComponent->audioFileStopButton.setEnabled (false);
//                // mainContentComponent->audioFilePlayButton.setEnabled (true);
//                sendChangeMessage ();
//                transportSource.setPosition (0.0);
//                break;
//                
//            case Loaded:
//                break;
//                
//            case Starting:
//                // mainContentComponent->audioFilePlayButton.setEnabled (false);
//                sendChangeMessage ();
//                transportSource.start();
//                break;
//                
//            case Playing:
//                // mainContentComponent->audioFileStopButton.setEnabled (true);
//                sendChangeMessage ();
//                break;
//                
//            case Stopping:
//                transportSource.stop();
//                break;
//        }
//    }
//}

//AudioFileReader::TransportState AudioFileReader::getState () { return state; }
