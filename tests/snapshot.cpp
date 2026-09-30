#include "../Source/PluginProcessor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    SmadderProcessor proc;
    proc.prepareToPlay (48000, 512);
    if (argc > 2) proc.loadPreset (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (argv[1]); out.deleteFile();
    juce::FileOutputStream os (out); juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
