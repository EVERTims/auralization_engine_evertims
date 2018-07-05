#include "../JuceLibraryCode/JuceHeader.h"
#include "DelayLine.h"

class AudioRecorder
{
    
//==========================================================================
// ATTRIBUTES
    
public:

    
    
private:
    // ring buffer used to write audio data in main audio loop, awaiting writing to disk
    DelayLine delayLine;
    
    // the thread that will write our audio data to disk
    TimeSliceThread backgroundThread { "Audio Recorder Thread" };
    
    // the FIFO used to buffer the incoming data
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> threadedWriter;
    
    CriticalSection writerLock;
    std::atomic<AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    
    double localSampleRate = 0;
    unsigned int localNumChannel = N_AMBI_CH;
//==========================================================================
// METHODS
    
public:
    
AudioRecorder()
{
    backgroundThread.startThread();
}

~AudioRecorder() {
    _stopRecording();
}
    
void prepareToPlay(const unsigned int samplesPerBlockExpected, const double sampleRate){

    localSampleRate = sampleRate;
    
    delayLine.prepareToPlay(samplesPerBlockExpected, sampleRate);
    delayLine.setSize(localNumChannel, 10 * sampleRate);
}


void startRecording()
{
//    if (! RuntimePermissions::isGranted (RuntimePermissions::writeExternalStorage))
//    {
//        SafePointer<AudioRecorder> safeThis (this);
//        
//        RuntimePermissions::request (RuntimePermissions::writeExternalStorage,
//                                     [safeThis] (bool granted) mutable
//                                     {
//                                         if (granted)
//                                             safeThis->startRecording();
//                                     });
//        return;
//    }
    
    auto parentDir = File::getSpecialLocation (File::userDocumentsDirectory);
    File lastRecording = parentDir.getNonexistentChildFile ("JUCE Demo Audio Recording", ".wav");
    _startRecording (lastRecording);
}

void stopRecording()
{
    _stopRecording();
}

void _startRecording (const File& file)
{
    _stopRecording();
    
    if (localSampleRate > 0)
    {
        // Create an OutputStream to write to our destination file...
        file.deleteFile();
        std::unique_ptr<FileOutputStream> fileStream (file.createOutputStream());
        
        if (fileStream.get() != nullptr)
        {
            // Now create a WAV writer object that writes to our output stream...
            WavAudioFormat wavFormat;
            auto* writer = wavFormat.createWriterFor (fileStream.get(), localSampleRate, localNumChannel, 16, {}, 0);
            
            if (writer != nullptr)
            {
                fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)
                
                // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                // write the data to disk on our background thread.
                threadedWriter.reset (new AudioFormatWriter::ThreadedWriter (writer, backgroundThread, 32768));
                
                // And now, swap over our active writer pointer so that the audio callback will start using it..
                const ScopedLock sl (writerLock);
                activeWriter = threadedWriter.get();
            }
        }
    }
}

void _stopRecording()
{
    // First, clear this pointer to stop the audio callback from using our writer object..
    {
        const ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }
    
    // Now we can delete the writer object. It's done in this order because the deletion could
    // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
    // the audio callback while this happens.
    threadedWriter.reset();
}

bool isRecording() const
{
    return activeWriter.load() != nullptr;
}

void recordBuffer (const float** inputChannelData, int numInputChannels, int numSamples)
{
    const ScopedLock sl (writerLock);
    
    if (activeWriter.load() != nullptr && numInputChannels >= localNumChannel)
    {
        activeWriter.load()->write (inputChannelData, numSamples);
    }
    
}

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)

};
