#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    A simple component that draws an audio waveform.
*/
class OscilloscopeComponent : public juce::Component,
    public juce::Timer
{
public:
    OscilloscopeComponent();
    ~OscilloscopeComponent() override;

    // Component overrides
    void paint(juce::Graphics&) override;
    void resized() override; // Keep override in declaration

    // Update signature to accept frequency
    void copySamples(const float* samples, int numSamples, float freqHz); // <-- MODIFIED SIGNATURE

private:
    // Timer override
    void timerCallback() override; // Keep override in declaration

    juce::AudioBuffer<float> displayBuffer; // Buffer to store samples for display
    juce::SpinLock           bufferLock;    // Lock to protect thread-safe access
    int                      bufferSize = 512; // Number of samples to store/draw

    // Member to store the frequency for display
    float frequencyHz = 0.0f; // <-- ADDED MEMBER

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};