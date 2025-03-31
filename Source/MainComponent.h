#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_dsp/juce_dsp.h> // Needed for ADSR and SmoothedValue
#include <map>
#include <atomic>
#include <memory> // Keep for std::unique_ptr if used, or remove if not
#include <JuceHeader.h>

#include "OscilloscopeComponent.h"
#include "ControlsComponent.h" // Need full definition
#include "SynthEngine.h"       // Need full definition

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
    public juce::KeyListener
{
public:
    enum Waveform { sine = 1, square, saw, triangle };
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    // --- Public methods for ControlsComponent callbacks ---
    void updateADSR(float attack, float decay, float sustain, float release);
    void setWaveform(int typeId);
    void setFineTune(float semitones);
    void setTranspose(int semitones);
    void updateFilter(float cutoff, float resonance); // <-- NEW

    //==============================================================================
    // AudioAppComponent overrides (Keep override in declaration)
    // Ensure signature matches base class EXACTLY
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override; // <-- Ensure samplesPerBlockExpected is here
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    // Component overrides (Keep override in declaration)
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // KeyListener overrides (NO override in declaration for now)
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent);
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent);


private:
    //==============================================================================
    // State Variables MainComponent manages:
    double currentSampleRate = 0.0;

    // Synth Parameters controlled by UI
    std::atomic<int>   currentWaveform{ Waveform::sine };
    juce::SmoothedValue<float> smoothedLevel{ 0.75f };
    std::atomic<float> fineTuneSemitones{ 0.0f };
    std::atomic<int>   transposeSemitones{ 0 };
    // Filter Parameters controlled by UI
    std::atomic<float> filterCutoffHz{ 1000.0f };   // <-- NEW (Default cutoff Hz)
    std::atomic<float> filterResonance{ 0.707f }; // <-- NEW (Default resonance ~1/sqrt(2))

    // Keyboard State Tracking
    std::map<int, bool> keysDown;
    int                 currentlyPlayingNote = -1;

    // Core Synthesis handled by SynthEngine
    SynthEngine synthEngine; // Direct member

    // Child Components
    OscilloscopeComponent oscilloscope;
    ControlsComponent controlsPanel; // Direct member

    // Helper method to update engine pitch (already exists)
    void updateEnginePitch();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};