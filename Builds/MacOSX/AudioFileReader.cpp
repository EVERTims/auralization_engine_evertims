#include "AudioFileReader.h"


AudioFileReader::AudioFileReader()
{

}

AudioFileReader::~AudioFileReader() {}


void AudioFileReader::buttonClicked (Button* button)
{
    if (button == &audioFileOpenButton)  openButtonClicked();
}

void AudioFileReader::openButtonClicked()
{
    FileChooser chooser ("Select a Wave file to play...",
                         File::nonexistent,
                         "*.wav");
    
//    if (chooser.browseForFileToOpen())
//    {
//        File file (chooser.getResult());
//        AudioFormatReader* reader = formatManager.createReaderFor (file);
//        
//        if (reader != nullptr)
//        {
//            ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
//            //            transportSource.setSource (newSource, 0, nullptr, reader->sampleRate);
//            //            playButton.setEnabled (true);
//            //            readerSource = newSource.release();
//        }
//    }
}