#include "ControlsComponent.h"
#include "MainComponent.h" // Needs to be included for calling updateADSR & accessing enum
#include <juce_core/system/juce_TargetPlatform.h> // For DBG

//==============================================================================
// Constructor: Takes pointer to MainComponent and references to the states it controls
ControlsComponent::ControlsComponent(MainComponent* mainComp,
    std::atomic<int>& waveformSelection,
    juce::SmoothedValue<float>& levelSmoother,
    std::atomic<float>& tuneSelection,
    std::atomic<int>& transposeSelection) :
    mainComponentPtr(mainComp), // Store pointer to parent
    waveformSelectionRef(waveformSelection), // Initialize references
    levelSmootherRef(levelSmoother),
    tuneSelectionRef(tuneSelection),
    transposeSelectionRef(transposeSelection)
{
    // Ensure the pointer is valid before proceeding
    jassert(mainComponentPtr != nullptr);

    // --- Waveform Selector ---
    waveformLabel.setText("Waveform:", juce::dontSendNotification);
    waveformLabel.attachToComponent(&waveformSelector, true);
    waveformLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(waveformLabel);

    addAndMakeVisible(waveformSelector);
    waveformSelector.setEditableText(false);
    waveformSelector.setJustificationType(juce::Justification::centred);
    waveformSelector.addItem("Sine", MainComponent::Waveform::sine);
    waveformSelector.addItem("Square", MainComponent::Waveform::square);
    waveformSelector.addItem("Sawtooth", MainComponent::Waveform::saw);
    waveformSelector.addItem("Triangle", MainComponent::Waveform::triangle);
    waveformSelector.setSelectedId(waveformSelectionRef.load(), juce::dontSendNotification); // Set initial
    waveformSelector.addListener(this);

    // --- Level Slider ---
    levelLabel.setText("Level:", juce::dontSendNotification);
    levelLabel.attachToComponent(&levelSlider, true);
    levelLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(levelLabel);
    addAndMakeVisible(levelSlider);
    levelSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    levelSlider.setRange(0.0, 1.0, 0.01);
    levelSlider.setValue(levelSmootherRef.getCurrentValue(), juce::dontSendNotification); // Use getCurrentValue for initial sync
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    levelSlider.addListener(this);

    // --- Fine Tune Slider ---
    tuneLabel.setText("Fine Tune:", juce::dontSendNotification);
    tuneLabel.attachToComponent(&tuneSlider, true);
    tuneLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(tuneLabel);
    addAndMakeVisible(tuneSlider);
    tuneSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    tuneSlider.setRange(-1.0, 1.0, 0.01); // +/- 1 semitone
    tuneSlider.setValue(tuneSelectionRef.load(), juce::dontSendNotification); // Read atomic initial
    tuneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    tuneSlider.setNumDecimalPlacesToDisplay(2);
    tuneSlider.setTextValueSuffix(" st");
    tuneSlider.addListener(this); // Add listener

    // --- Transpose Slider ---
    transposeLabel.setText("Transpose:", juce::dontSendNotification);
    transposeLabel.attachToComponent(&transposeSlider, true);
    transposeLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(transposeLabel);
    addAndMakeVisible(transposeSlider);
    transposeSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    transposeSlider.setRange(-24, 24, 1); // +/- 24 semitones
    transposeSlider.setValue(transposeSelectionRef.load(), juce::dontSendNotification); // Read atomic initial
    transposeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    transposeSlider.setTextValueSuffix(" semi");
    transposeSlider.addListener(this); // Add listener

    // --- ADSR Sliders ---
    // Attack
    attackLabel.setText("Attack:", juce::dontSendNotification);
    attackLabel.attachToComponent(&attackSlider, true);
    attackLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(attackLabel);
    addAndMakeVisible(attackSlider);
    attackSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    attackSlider.setRange(0.001, 1.0, 0.001); // Min attack > 0
    // Get initial value from MainComponent's adsrParams if possible (requires accessor or making adsrParams public/friend)
    // For now, set to reasonable default consistent with MainComponent constructor
    attackSlider.setValue(0.05, juce::dontSendNotification); // Default Attack
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    attackSlider.setSkewFactorFromMidPoint(0.2);
    attackSlider.addListener(this);

    // Decay
    decayLabel.setText("Decay:", juce::dontSendNotification);
    decayLabel.attachToComponent(&decaySlider, true);
    decayLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(decayLabel);
    addAndMakeVisible(decaySlider);
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
    addAndMakeVisible(sustainLabel);
    addAndMakeVisible(sustainSlider);
    sustainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sustainSlider.setRange(0.0, 1.0, 0.01); // Level 0-1
    sustainSlider.setValue(0.8, juce::dontSendNotification); // Default Sustain
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    sustainSlider.addListener(this);

    // Release
    releaseLabel.setText("Release:", juce::dontSendNotification);
    releaseLabel.attachToComponent(&releaseSlider, true);
    releaseLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(releaseLabel);
    addAndMakeVisible(releaseSlider);
    releaseSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    releaseSlider.setRange(0.001, 2.0, 0.001); // Allow longer release
    releaseSlider.setValue(0.5, juce::dontSendNotification); // Default Release
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    releaseSlider.setSkewFactorFromMidPoint(0.3);
    releaseSlider.addListener(this);

    // Call once initially to set default ADSR params in MainComponent from slider values
    // This assumes MainComponent::adsrParams was already initialized with these defaults too
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
    auto bounds = getLocalBounds().reduced(10); // Add some internal margin
    auto labelWidth = 80;    // Width for labels (e.g., "Waveform:")
    auto controlHeight = 25; // Height for each slider/combobox row
    auto spacing = 5;        // Vertical spacing between rows

    // Helper lambda to simplify laying out a control row
    auto layoutRow = [&](juce::Component& control) {
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
}

// ComboBox Listener Callback
void ControlsComponent::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) // No override
{
    if (comboBoxThatHasChanged == &waveformSelector)
    {
        waveformSelectionRef.store(waveformSelector.getSelectedId()); // Update atomic waveform ID
        DBG("ControlsComponent: Waveform changed to ID: " + juce::String(waveformSelectionRef.load()));
    }
}

// Slider Listener Callback - Handles ALL sliders now
void ControlsComponent::sliderValueChanged(juce::Slider* sliderThatWasMoved) // No override
{
    if (sliderThatWasMoved == &levelSlider)
    {
        float newLevel = (float)levelSlider.getValue();
        levelSmootherRef.setTargetValue(newLevel); // Set target for smoothed value
        DBG("ControlsComponent: Level Slider moved -> Target set to: " + juce::String(newLevel));
    }
    else if (sliderThatWasMoved == &tuneSlider)
    {
        float newTune = (float)tuneSlider.getValue();
        tuneSelectionRef.store(newTune); // Update atomic value directly
        DBG("ControlsComponent: Fine Tune changed to: " + juce::String(newTune));
    }
    else if (sliderThatWasMoved == &transposeSlider)
    {
        int newTranspose = (int)transposeSlider.getValue();
        transposeSelectionRef.store(newTranspose); // Update atomic value directly
        DBG("ControlsComponent: Transpose changed to: " + juce::String(newTranspose) + " semi");
    }
    // Check if any ADSR slider moved
    else if (sliderThatWasMoved == &attackSlider ||
        sliderThatWasMoved == &decaySlider ||
        sliderThatWasMoved == &sustainSlider ||
        sliderThatWasMoved == &releaseSlider)
    {
        updateADSRParameters(); // Call helper function to update all ADSR params
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