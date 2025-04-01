#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <juce_dsp/juce_dsp.h> // For SmoothedValue type

// Forward declare MainComponent
class MainComponent;

//==============================================================================
/*
    This component holds all the UI controls for the synthesizer.
*/
class ControlsComponent : public juce::Component,
    public juce::ComboBox::Listener,
    public juce::Slider::Listener
{
public:
    // Constructor takes pointer to MainComponent and references to ALL states it controls
    ControlsComponent(MainComponent* mainComp,
        std::atomic<int>& waveformSelection,
        juce::SmoothedValue<float>& levelSmoother,
        std::atomic<float>& tuneSelection,
        std::atomic<int>& transposeSelection,
        std::atomic<float>& filterCutoff,
        std::atomic<float>& filterResonance,
        std::atomic<int>& rootNoteSelection,      // <-- NEW Ref
        std::atomic<int>& scaleTypeSelection);    // <-- NEW Ref

    ~ControlsComponent() override; // Keep standard override

    // Component overrides
    void paint(juce::Graphics&) override; // Keep override in declaration
    void resized() override;               // Keep override in declaration

    // Listener Callbacks
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override; // Keep override here
    void sliderValueChanged(juce::Slider* sliderThatWasMoved) override;   // Keep override here

private:
    // UI Elements
    juce::Label waveformLabel;
    juce::ComboBox waveformSelector;

    juce::Label levelLabel;
    juce::Slider levelSlider;

    juce::Label tuneLabel;
    juce::Slider tuneSlider;

    juce::Label transposeLabel;
    juce::Slider transposeSlider;

    // ADSR Controls
    juce::Label attackLabel;
    juce::Slider attackSlider;
    juce::Label decayLabel;
    juce::Slider decaySlider;
    juce::Label sustainLabel;
    juce::Slider sustainSlider;
    juce::Label releaseLabel;
    juce::Slider releaseSlider;

    // Filter Controls
    juce::Label filterCutoffLabel;
    juce::Slider filterCutoffSlider;
    juce::Label filterResonanceLabel;
    juce::Slider filterResonanceSlider;

    // --- Scale Controls ---
    juce::Label rootNoteLabel;          // <-- NEW Declaration
    juce::ComboBox rootNoteSelector;    // <-- NEW Declaration
    juce::Label scaleTypeLabel;         // <-- NEW Declaration
    juce::ComboBox scaleTypeSelector;   // <-- NEW Declaration


    // Pointer back to MainComponent (used for updateADSR)
    MainComponent* mainComponentPtr;

    // References to state in MainComponent that this component directly updates/reads
    std::atomic<int>& waveformSelectionRef;
    juce::SmoothedValue<float>& levelSmootherRef;
    std::atomic<float>& tuneSelectionRef;
    std::atomic<int>& transposeSelectionRef;
    std::atomic<float>& filterCutoffRef;
    std::atomic<float>& filterResonanceRef;
    std::atomic<int>& rootNoteRef;          // <-- NEW Ref Member
    std::atomic<int>& scaleTypeRef;         // <-- NEW Ref Member

    // Helper function to trigger update in MainComponent for ADSR
    void updateADSRParameters();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsComponent)
};