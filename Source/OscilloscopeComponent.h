#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    A simple component that draws an audio waveform.
*/
class OscilloscopeComponent : public juce::Component,
    public juce::Timer // Inherit from Timer for periodic updates
{
public:
    OscilloscopeComponent();
    ~OscilloscopeComponent() override; // Standard override for virtual destructor

    // Component overrides
    void paint(juce::Graphics&) override; // Standard override
    void resized() override;               // Standard override

    // Public method called by the audio thread to provide new samples
    void copySamples(const float* samples, int numSamples);

private:
    // Timer override
    void timerCallback() override; // Standard override

    juce::AudioBuffer<float> displayBuffer; // Buffer to store samples for display
    juce::SpinLock           bufferLock;    // Lock to protect thread-safe access to displayBuffer
    int                      bufferSize = 512; // How many samples to store/draw in the buffer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};