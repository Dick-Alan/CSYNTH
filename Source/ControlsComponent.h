#pragma once

#include <JuceHeader.h> // Includes essential JUCE headers
#include <atomic>       // For std::atomic types
#include <juce_dsp/juce_dsp.h> // For juce::SmoothedValue

// Forward declare MainComponent to avoid circular include issues
class MainComponent;

//==============================================================================
/*
    This component holds all the UI controls for the synthesizer.
*/
class ControlsComponent : public juce::Component,
    public juce::ComboBox::Listener, // Listens to ComboBox changes
    public juce::Slider::Listener   // Listens to Slider changes
{
public:
    // Constructor takes pointer to MainComponent and references to the states it controls
    ControlsComponent(MainComponent* mainComp,
        std::atomic<int>& waveformSelection,
        juce::SmoothedValue<float>& levelSmoother,
        std::atomic<float>& tuneSelection,
        std::atomic<int>& transposeSelection);

    ~ControlsComponent() override; // Standard override for virtual destructor

    // Component overrides for drawing and layout
    void paint(juce::Graphics&) override;
    void resized() override;

    // Listener Callbacks (declared with override as they override base class virtuals)
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

    // Pointer back to MainComponent (to call updateADSR method)
    MainComponent* mainComponentPtr;

    // References to state variables in MainComponent that this component directly updates
    std::atomic<int>& waveformSelectionRef;
    juce::SmoothedValue<float>& levelSmootherRef; // Note: We set the *target* of this
    std::atomic<float>& tuneSelectionRef;
    std::atomic<int>& transposeSelectionRef;

    // Helper function to read ADSR sliders and call MainComponent's update method
    void updateADSRParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsComponent)
};