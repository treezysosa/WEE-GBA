#include "PluginProcessor.h"
#include <iostream>
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    WeeProcessor original;
    auto* gain = original.state.getParameter("gain");
    gain->setValueNotifyingHost(gain->convertTo0to1(0.67f));
    juce::MemoryBlock saved; original.getStateInformation(saved);
    WeeProcessor restored; restored.setStateInformation(saved.getData(), int(saved.getSize()));
    if (std::abs(restored.state.getRawParameterValue("gain")->load()-0.67f)>0.0001f) return 1;
    const char invalid[] = "invalid"; restored.setStateInformation(invalid, sizeof invalid);
    if (std::abs(restored.state.getRawParameterValue("gain")->load()-0.67f)>0.0001f) return 2;
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("wee-state-test", ".wee");
    if (!original.savePreset(file) || !restored.loadPreset(file)) return 3;
    file.deleteFile();
    std::cout << "State and preset round trips passed\n";
}
