#ifndef AUDIOFILEREADER_H_INCLUDED
#define AUDIOFILEREADER_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class AudioFileReader : public Button::Listener
{
    
public:
    
    AudioFileReader();
     ~AudioFileReader();

private:
    
    void buttonClicked(Button* button) override;
    void openButtonClicked();
    
    TextButton audioFileOpenButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileReader)
};

#endif // AUDIOFILEREADER_H_INCLUDED
