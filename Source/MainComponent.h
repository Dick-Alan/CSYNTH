#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_dsp/juce_dsp.h> // Needed for ADSR and SmoothedValue
#include <map>                  // For std::map
#include <atomic>               // For std::atomic
#include <memory>               // For std::unique_ptr
#include <JuceHeader.h>         // Includes modules selected in Projucer

#include "OscilloscopeComponent.h"
#include "ControlsComponent.h" // Include full definition needed for unique_ptr member

//==============================================================================
/*
    This component lives inside our window, handles audio and keyboard input,
    and owns the child components (oscilloscope, controls).
*/
class MainComponent : public juce::AudioAppComponent,
    public juce::KeyListener
{
public:
    // Enum to identify waveform types (IDs match ComboBox)
    enum Waveform {
        sine = 1,
        square,
        saw,
        triangle
    };

    //==============================================================================
    MainComponent();
    ~MainComponent() override; // Standard override for virtual destructor

    // Public method for ControlsComponent to update ADSR parameters
    void updateADSR(float attack, float decay, float sustain, float release);

    //==============================================================================
    // AudioAppComponent overrides (Keep override in declaration)
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    // Component overrides (Keep override in declaration)
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // KeyListener overrides (NO override in declaration, matching definition for now)
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent);
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent);


private:
    //==============================================================================
    // Audio Engine State
    double currentSampleRate = 0.0; // Set in prepareToPlay
    double currentAngle = 0.0; // Current phase for oscillator
    double angleDelta = 0.0; // Calculated based on final frequency in audio thread
    double targetFrequency = 0.0; // Base frequency (after transpose) set by key events

    // Synth Parameters (Thread-Safe Access where needed)
    std::atomic<int>   currentWaveform{ Waveform::sine }; // Current waveform ID
    juce::SmoothedValue<float> smoothedLevel{ 0.75f };    // Smoothed master volume (0.0 to 1.0)
    std::atomic<float> fineTuneSemitones{ 0.0f };       // Fine tuning (-1 to +1 semitones)
    std::atomic<int>   transposeSemitones{ 0 };         // Transpose (-24 to +24 semitones)

    // ADSR Envelope state and parameters
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    // Keyboard State Tracking
    std::map<int, bool> keysDown; // Tracks which keys *we care about* are physically down

    // Child Components
    OscilloscopeComponent oscilloscope; // Direct member
    // Use unique_ptr for controlsPanel for safer memory management
    std::unique_ptr<ControlsComponent> controlsPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};