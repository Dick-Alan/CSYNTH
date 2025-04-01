#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <vector>           // <-- Added for std::vector
#include <atomic>
#include <JuceHeader.h>     // Keep this, includes many things
#include <memory>
#include "OscilloscopeComponent.h"
#include "ControlsComponent.h"      // Need full definition because ControlsComponent is a direct member
#include "SynthEngine.h"          // Need full definition because SynthEngine is a direct member

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
    public juce::KeyListener
{
public:
    // Enum to identify waveform types (shared with ControlsComponent)
    enum Waveform { sine = 1, square, saw, triangle };

    // --- ADDED Scale Information ---
    // Scale Type Enum / Identifiers (Start from 1 for ComboBox)
    enum ScaleType {
        Major = 1,
        NaturalMinor,
        Dorian,
        // Add more scales here later if desired
        NumScaleTypes // Keep this last for count if needed
    };

    // Structure to hold scale data
    struct ScaleInfo {
        juce::String name;
        std::vector<int> intervals; // Semitones relative to root (root = 0)
    };
    // --- End Scale Information ---

    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    // --- Public methods for ControlsComponent callbacks ---
    void updateADSR(float attack, float decay, float sustain, float release);
    void setWaveform(int typeId);
    void setFineTune(float semitones);
    void setTranspose(int semitones);
    void updateFilter(float cutoff, float resonance);
    void setRootNote(int rootNoteIndex); // 0-11 for C to B <-- NEW
    void setScaleType(int scaleId);      // Use ScaleType enum values <-- NEW

    // --- Getters for ControlsComponent initialization ---
    int getRootNote() const { return rootNote.load(); }         // <-- NEW Getter
    int getScaleType() const { return currentScaleType.load(); } // <-- NEW Getter
    const juce::StringArray& getScaleNames() const { return scaleNames; } // <-- NEW Getter
    // Getters needed for existing controls if ControlsComponent constructor reads them
    float getFilterCutoff() const { return filterCutoffHz.load(); }
    float getFilterResonance() const { return filterResonance.load(); }
    float getFineTune() const { return fineTuneSemitones.load(); }
    int getTranspose() const { return transposeSemitones.load(); }


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
    // KeyListener overrides (NO override in declaration for now)
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent);
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent);


private:
    //==============================================================================
    // --- Scale Data Storage ---
    std::vector<ScaleInfo> scaleData; // Holds name and interval patterns <-- NEW
    juce::StringArray scaleNames;     // Just the names for the UI <-- NEW

    // --- State Variables ---
    double currentSampleRate = 0.0;

    // Synth Parameters controlled by UI
    std::atomic<int>   currentWaveform{ Waveform::sine };
    juce::SmoothedValue<float> smoothedLevel{ 0.75f };
    std::atomic<float> fineTuneSemitones{ 0.0f };
    std::atomic<int>   transposeSemitones{ 0 };
    std::atomic<float> filterCutoffHz{ 10000.0f }; // Default from your working version
    std::atomic<float> filterResonance{ 0.707f }; // Default from your working version
    std::atomic<int>   rootNote{ 0 }; // 0-11 (Default C=0) <-- NEW State
    std::atomic<int>   currentScaleType{ ScaleType::Major }; // Default Major=1 <-- NEW State

    // Keyboard State Tracking
    std::map<int, bool> keysDown;
    int                 currentlyPlayingNote = -1; // Base MIDI note (0-127) determined by key+scale+root

    // Core Synthesis
    SynthEngine synthEngine; // Direct member based on your uploaded code

    // Child Components
    OscilloscopeComponent oscilloscope; // Direct member
    std::unique_ptr<ControlsComponent> controlsPanel; // Use unique_ptr

    // Private methods (updateEnginePitch is needed by setters/key handlers)
    void updateEnginePitch();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};