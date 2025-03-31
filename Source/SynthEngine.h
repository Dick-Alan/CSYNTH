#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h> // Needed for Filter and ProcessSpec
#include <atomic>

// Forward declare MainComponent just in case (though not strictly needed by header now)
class MainComponent;

//==============================================================================
/*
    Handles the core sound generation (oscillator + filter + ADSR) for one synth voice.
*/
class SynthEngine
{
public:
    SynthEngine();
    double getCurrentFrequency() const; // <-- ADD THIS DECLARATION
    // --- Setup ---
    // Prepare engine for playback with audio specs
    void prepareToPlay(double sampleRate, int maximumBlockSize, int numChannels); // <-- Updated signature

    // --- Parameter Setters called by MainComponent ---
    void setParameters(const juce::ADSR::Parameters& params); // For ADSR
    void setWaveform(int waveformTypeId);
    void setFrequency(double frequencyHz);
    void setFilterParameters(float cutoffHz, float resonance); // <-- NEW for Filter

    // --- Triggers ---
    void noteOn();
    void noteOff();
    bool isActive() const; // Keep this

    // --- Audio Processing ---
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);


private:
    // Audio State
    double currentSampleRate = 0.0;
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double frequency = 440.0;

    // Parameters
    std::atomic<int> currentWaveformType{ 1 }; // Default Sine

    // DSP Modules
    juce::dsp::StateVariableTPTFilter<float> filter; // <-- ADDED Filter object
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams; // Store local copy
};