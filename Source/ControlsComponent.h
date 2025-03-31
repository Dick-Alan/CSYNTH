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
    // Constructor takes pointer to MainComponent and references to states it controls
    ControlsComponent(MainComponent* mainComp,
        std::atomic<int>& waveformSelection,
        juce::SmoothedValue<float>& levelSmoother,
        std::atomic<float>& tuneSelection,
        std::atomic<int>& transposeSelection,
        std::atomic<float>& filterCutoff,    // <-- NEW Ref
        std::atomic<float>& filterResonance);// <-- NEW Ref
    ~ControlsComponent() override;

    // Component overrides
    void paint(juce::Graphics&) override;
    void resized() override;

    // Listener Callbacks
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void sliderValueChanged(juce::Slider* sliderThatWasMoved) override;

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

    // --- Filter Controls ---
    juce::Label filterCutoffLabel;      // <-- NEW
    juce::Slider filterCutoffSlider;    // <-- NEW
    juce::Label filterResonanceLabel;   // <-- NEW
    juce::Slider filterResonanceSlider; // <-- NEW

    // Pointer back to MainComponent
    MainComponent* mainComponentPtr;

    // References to state in MainComponent
    std::atomic<int>& waveformSelectionRef;
    juce::SmoothedValue<float>& levelSmootherRef;
    std::atomic<float>& tuneSelectionRef;
    std::atomic<int>& transposeSelectionRef;
    std::atomic<float>& filterCutoffRef;      // <-- NEW Ref
    std::atomic<float>& filterResonanceRef;   // <-- NEW Ref

    // Helper function to trigger update in MainComponent
    void updateADSRParameters();
    // No specific helper needed for filter, sliderValueChanged will call MainComponent directly


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsComponent)
};