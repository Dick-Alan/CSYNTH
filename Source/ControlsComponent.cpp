#include "ControlsComponent.h"
#include "MainComponent.h" // Needs to be included for calling MainComponent methods & enum
#include <juce_core/system/juce_TargetPlatform.h> // For DBG

//==============================================================================
// Constructor takes pointer to MainComponent and references to the states it controls
// --- REPLACE ControlsComponent Constructor ---
ControlsComponent::ControlsComponent(MainComponent* mainComp,
    std::atomic<int>& waveformSelection,
    juce::SmoothedValue<float>& levelSmoother,
    std::atomic<float>& tuneSelection,
    std::atomic<int>& transposeSelection,
    std::atomic<float>& filterCutoff,
    std::atomic<float>& filterResonance) :
    mainComponentPtr(mainComp),
    waveformSelectionRef(waveformSelection),
    levelSmootherRef(levelSmoother),
    tuneSelectionRef(tuneSelection),
    transposeSelectionRef(transposeSelection),
    filterCutoffRef(filterCutoff),
    filterResonanceRef(filterResonance)
{
    // Ensure the pointer is valid before proceeding
    jassert(mainComponentPtr != nullptr);

    // --- Waveform Selector ---
    waveformLabel.setText("Waveform:", juce::dontSendNotification);
    waveformLabel.attachToComponent(&waveformSelector, true);
    waveformLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(waveformLabel); // Make Label visible
    addAndMakeVisible(waveformSelector); // Make ComboBox visible
    waveformSelector.setEditableText(false);
    waveformSelector.setJustificationType(juce::Justification::centred);
    waveformSelector.addItem("Sine", MainComponent::Waveform::sine);
    waveformSelector.addItem("Square", MainComponent::Waveform::square);
    waveformSelector.addItem("Sawtooth", MainComponent::Waveform::saw);
    waveformSelector.addItem("Triangle", MainComponent::Waveform::triangle);
    waveformSelector.setSelectedId(waveformSelectionRef.load(), juce::dontSendNotification);
    waveformSelector.addListener(this);

    // --- Level Slider ---
    levelLabel.setText("Level:", juce::dontSendNotification);
    levelLabel.attachToComponent(&levelSlider, true);
    levelLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(levelLabel); // Make Label visible
    addAndMakeVisible(levelSlider); // Make Slider visible
    levelSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    levelSlider.setRange(0.0, 1.0, 0.01);
    levelSlider.setValue(levelSmootherRef.getCurrentValue(), juce::dontSendNotification);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    levelSlider.addListener(this);

    // --- Fine Tune Slider ---
    tuneLabel.setText("Fine Tune:", juce::dontSendNotification);
    tuneLabel.attachToComponent(&tuneSlider, true);
    tuneLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(tuneLabel); // Make Label visible
    addAndMakeVisible(tuneSlider); // Make Slider visible
    tuneSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    tuneSlider.setRange(-1.0, 1.0, 0.01);
    tuneSlider.setValue(tuneSelectionRef.load(), juce::dontSendNotification);
    tuneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    tuneSlider.setNumDecimalPlacesToDisplay(2);
    tuneSlider.setTextValueSuffix(" st");
    tuneSlider.addListener(this);

    // --- Transpose Slider ---
    transposeLabel.setText("Transpose:", juce::dontSendNotification);
    transposeLabel.attachToComponent(&transposeSlider, true);
    transposeLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(transposeLabel); // Make Label visible
    addAndMakeVisible(transposeSlider); // Make Slider visible
    transposeSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    transposeSlider.setRange(-24, 24, 1);
    transposeSlider.setValue(transposeSelectionRef.load(), juce::dontSendNotification);
    transposeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    transposeSlider.setTextValueSuffix(" semi");
    transposeSlider.addListener(this);

    // --- ADSR Sliders ---
    // Attack
    attackLabel.setText("Attack:", juce::dontSendNotification);
    attackLabel.attachToComponent(&attackSlider, true);
    attackLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(attackLabel); // Make Label visible
    addAndMakeVisible(attackSlider); // Make Slider visible
    attackSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    attackSlider.setRange(0.001, 1.0, 0.001);
    attackSlider.setValue(0.05, juce::dontSendNotification); // Default Attack
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    attackSlider.setSkewFactorFromMidPoint(0.2);
    attackSlider.addListener(this);

    // Decay
    decayLabel.setText("Decay:", juce::dontSendNotification);
    decayLabel.attachToComponent(&decaySlider, true);
    decayLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(decayLabel); // Make Label visible
    addAndMakeVisible(decaySlider); // Make Slider visible
    decaySlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    decaySlider.setRange(0.001, 1.0, 0.001);
    decaySlider.setValue(0.1, juce::dontSendNotification); // Default Decay
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    decaySlider.setSkewFactorFromMidPoint(0.2);
    decaySlider.addListener(this);

    // Sustain
    sustainLabel.setText("Sustain:", juce::dontSendNotification);
    sustainLabel.attachToComponent(&sustainSlider, true);
    sustainLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(sustainLabel); // Make Label visible
    addAndMakeVisible(sustainSlider); // Make Slider visible
    sustainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sustainSlider.setRange(0.0, 1.0, 0.01);
    sustainSlider.setValue(0.8, juce::dontSendNotification); // Default Sustain
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    sustainSlider.addListener(this);

    // Release
    releaseLabel.setText("Release:", juce::dontSendNotification);
    releaseLabel.attachToComponent(&releaseSlider, true);
    releaseLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(releaseLabel); // Make Label visible
    addAndMakeVisible(releaseSlider); // Make Slider visible
    releaseSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    releaseSlider.setRange(0.001, 2.0, 0.001);
    releaseSlider.setValue(0.5, juce::dontSendNotification); // Default Release
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    releaseSlider.setSkewFactorFromMidPoint(0.3);
    releaseSlider.addListener(this);

    // --- Filter Controls ---
    // Cutoff
    filterCutoffLabel.setText("Cutoff:", juce::dontSendNotification);
    filterCutoffLabel.attachToComponent(&filterCutoffSlider, true);
    filterCutoffLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(filterCutoffLabel); // Make Label visible
    addAndMakeVisible(filterCutoffSlider); // Make Slider visible
    filterCutoffSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    filterCutoffSlider.setRange(20.0, 20000.0, 0.1);
    filterCutoffSlider.setSkewFactorFromMidPoint(1000.0); // Logarithmic skew
    filterCutoffSlider.setValue(filterCutoffRef.load(), juce::dontSendNotification);
    filterCutoffSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    filterCutoffSlider.setTextValueSuffix(" Hz");
    filterCutoffSlider.addListener(this);

    // Resonance
    filterResonanceLabel.setText("Resonance:", juce::dontSendNotification);
    filterResonanceLabel.attachToComponent(&filterResonanceSlider, true);
    filterResonanceLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(filterResonanceLabel); // Make Label visible
    addAndMakeVisible(filterResonanceSlider); // Make Slider visible
    filterResonanceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    filterResonanceSlider.setRange(1.0 / std::sqrt(2.0), 18.0, 0.01); // Approx 0.707 to 18
    filterResonanceSlider.setValue(filterResonanceRef.load(), juce::dontSendNotification);
    filterResonanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    filterResonanceSlider.setNumDecimalPlacesToDisplay(2);
    filterResonanceSlider.addListener(this);

    // Call once initially to set default ADSR params in MainComponent from slider values
    updateADSRParameters();
}

ControlsComponent::~ControlsComponent() // No override
{
    // Remove all listeners
    waveformSelector.removeListener(this);
    levelSlider.removeListener(this);
    tuneSlider.removeListener(this);
    transposeSlider.removeListener(this);
    attackSlider.removeListener(this);
    decaySlider.removeListener(this);
    sustainSlider.removeListener(this);
    releaseSlider.removeListener(this);
    filterCutoffSlider.removeListener(this);
    filterResonanceSlider.removeListener(this);
}

void ControlsComponent::paint(juce::Graphics& g) // No override
{
    // Draw an outline rectangle
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
}

void ControlsComponent::resized() // No override
{
    // Layout all controls vertically
    auto bounds = getLocalBounds().reduced(10); // Internal margin
    auto labelWidth = 80;    // Width for labels
    auto controlHeight = 25; // Height for controls
    auto spacing = 5;        // Vertical spacing

    // Helper lambda for laying out a row
    auto layoutRow = [&](juce::Component& control) { // Pass Component ref
        auto rowBounds = bounds.removeFromTop(controlHeight);
        // The label is attached, so we only need to set bounds for the control itself
        control.setBounds(labelWidth, rowBounds.getY(), rowBounds.getWidth() - labelWidth, controlHeight);
        bounds.removeFromTop(spacing); // Add space below this row
        };

    // Layout each control
    layoutRow(waveformSelector);
    layoutRow(levelSlider);
    layoutRow(tuneSlider);
    layoutRow(transposeSlider);
    layoutRow(attackSlider);
    layoutRow(decaySlider);
    layoutRow(sustainSlider);
    layoutRow(releaseSlider);
    layoutRow(filterCutoffSlider);    // Layout filter cutoff
    layoutRow(filterResonanceSlider); // Layout filter resonance
}

// ComboBox Listener Callback
void ControlsComponent::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) // No override
{
    if (comboBoxThatHasChanged == &waveformSelector)
    {
        // Update the atomic variable directly
        waveformSelectionRef.store(waveformSelector.getSelectedId());
        DBG("ControlsComponent: Waveform changed to ID: " + juce::String(waveformSelectionRef.load()));
        // Also tell MainComponent so it can tell the engine immediately
        if (mainComponentPtr != nullptr)
            mainComponentPtr->setWaveform(waveformSelectionRef.load());
    }
}

// Slider Listener Callback - Handles ALL sliders
void ControlsComponent::sliderValueChanged(juce::Slider* sliderThatWasMoved) // No override
{
    // Ensure pointer is valid before using
    if (mainComponentPtr == nullptr) return;

    if (sliderThatWasMoved == &levelSlider)
    {
        float newLevel = (float)levelSlider.getValue();
        levelSmootherRef.setTargetValue(newLevel); // Set target for smoothed value
        DBG("ControlsComponent: Level Slider moved -> Target set to: " + juce::String(newLevel));
    }
    else if (sliderThatWasMoved == &tuneSlider)
    {
        // Call method on MainComponent to handle tune changes
        mainComponentPtr->setFineTune((float)tuneSlider.getValue());
        // DBG log moved to MainComponent::setFineTune
    }
    else if (sliderThatWasMoved == &transposeSlider)
    {
        // Call method on MainComponent to handle transpose changes
        mainComponentPtr->setTranspose((int)transposeSlider.getValue());
        // DBG log moved to MainComponent::setTranspose
    }
    // Check if any ADSR slider moved
    else if (sliderThatWasMoved == &attackSlider ||
        sliderThatWasMoved == &decaySlider ||
        sliderThatWasMoved == &sustainSlider ||
        sliderThatWasMoved == &releaseSlider)
    {
        updateADSRParameters(); // Call helper function to update all ADSR params via MainComponent
    }
    // Check if any Filter slider moved
    else if (sliderThatWasMoved == &filterCutoffSlider ||
        sliderThatWasMoved == &filterResonanceSlider)
    {
        // Read both values and call the update method on MainComponent
        float cutoff = (float)filterCutoffSlider.getValue();
        float res = (float)filterResonanceSlider.getValue();
        mainComponentPtr->updateFilter(cutoff, res);
        // DBG log moved to MainComponent::updateFilter
    }
    else if (sliderThatWasMoved == &filterCutoffSlider ||
        sliderThatWasMoved == &filterResonanceSlider)
    {
        // Read both values
        float cutoff = (float)filterCutoffSlider.getValue();
        float res = (float)filterResonanceSlider.getValue();

        // --- ADD THIS LOG ---
        DBG("ControlsComponent: Filter slider moved. Read Cutoff=" + juce::String(cutoff) + ", Res=" + juce::String(res) + ". Calling updateFilter...");

        // Call the update method on MainComponent (check pointer first)
        if (mainComponentPtr != nullptr)
            mainComponentPtr->updateFilter(cutoff, res);
    }
}

// Helper function to read ADSR sliders and update MainComponent
void ControlsComponent::updateADSRParameters()
{
    // Ensure mainComponentPtr is valid before using it
    if (mainComponentPtr != nullptr)
    {
        // Read current values from all ADSR sliders
        float a = (float)attackSlider.getValue();
        float d = (float)decaySlider.getValue();
        float s = (float)sustainSlider.getValue();
        float r = (float)releaseSlider.getValue();

        // Call the public method on MainComponent to update the actual ADSR object
        mainComponentPtr->updateADSR(a, d, s, r);
    }
}