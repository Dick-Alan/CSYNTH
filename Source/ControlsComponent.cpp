#include "ControlsComponent.h"
#include "MainComponent.h" // Needs to be included for calling MainComponent methods & enums
#include <juce_core/system/juce_TargetPlatform.h> // For DBG

//==============================================================================
// Update constructor signature and initializer list to match ControlsComponent.h
ControlsComponent::ControlsComponent(MainComponent* mainComp,
    std::atomic<int>& waveformSelection,
    juce::SmoothedValue<float>& levelSmoother,
    std::atomic<float>& tuneSelection,
    std::atomic<int>& transposeSelection,
    std::atomic<float>& filterCutoff,
    std::atomic<float>& filterResonance,
    std::atomic<int>& rootNoteSelection,      // <-- NEW Ref Added
    std::atomic<int>& scaleTypeSelection) :   // <-- NEW Ref Added
    mainComponentPtr(mainComp),
    waveformSelectionRef(waveformSelection),
    levelSmootherRef(levelSmoother),
    tuneSelectionRef(tuneSelection),
    transposeSelectionRef(transposeSelection),
    filterCutoffRef(filterCutoff),
    filterResonanceRef(filterResonance),
    rootNoteRef(rootNoteSelection),           // <-- Initialize NEW Ref
    scaleTypeRef(scaleTypeSelection)          // <-- Initialize NEW Ref
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
    waveformSelector.setSelectedId(waveformSelectionRef.load(), juce::dontSendNotification);
    waveformSelector.addListener(this);

    // --- Level Slider ---
    levelLabel.setText("Level:", juce::dontSendNotification);
    levelLabel.attachToComponent(&levelSlider, true);
    levelLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(levelLabel);
    addAndMakeVisible(levelSlider);
    levelSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    levelSlider.setRange(0.0, 1.0, 0.01);
    levelSlider.setValue(levelSmootherRef.getCurrentValue(), juce::dontSendNotification);
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
    tuneSlider.setValue(tuneSelectionRef.load(), juce::dontSendNotification); // Use ref for initial value
    tuneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    tuneSlider.setNumDecimalPlacesToDisplay(2);
    tuneSlider.setTextValueSuffix(" st");
    tuneSlider.addListener(this);

    // --- Transpose Slider ---
    transposeLabel.setText("Transpose:", juce::dontSendNotification);
    transposeLabel.attachToComponent(&transposeSlider, true);
    transposeLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(transposeLabel);
    addAndMakeVisible(transposeSlider);
    transposeSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    transposeSlider.setRange(-24, 24, 1); // +/- 24 semitones
    transposeSlider.setValue(transposeSelectionRef.load(), juce::dontSendNotification); // Use ref for initial value
    transposeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    transposeSlider.setTextValueSuffix(" semi");
    transposeSlider.addListener(this);

    // --- ADSR Sliders ---
    // Attack
    attackLabel.setText("Attack:", juce::dontSendNotification);
    attackLabel.attachToComponent(&attackSlider, true);
    attackLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(attackLabel); addAndMakeVisible(attackSlider);
    attackSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    attackSlider.setRange(0.001, 1.0, 0.001); attackSlider.setValue(0.05);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20); attackSlider.setSkewFactorFromMidPoint(0.2);
    attackSlider.addListener(this);

    // Decay
    decayLabel.setText("Decay:", juce::dontSendNotification);
    decayLabel.attachToComponent(&decaySlider, true);
    decayLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(decayLabel); addAndMakeVisible(decaySlider);
    decaySlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    decaySlider.setRange(0.001, 1.0, 0.001); decaySlider.setValue(0.1);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20); decaySlider.setSkewFactorFromMidPoint(0.2);
    decaySlider.addListener(this);

    // Sustain
    sustainLabel.setText("Sustain:", juce::dontSendNotification);
    sustainLabel.attachToComponent(&sustainSlider, true);
    sustainLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(sustainLabel); addAndMakeVisible(sustainSlider);
    sustainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    sustainSlider.setRange(0.0, 1.0, 0.01); sustainSlider.setValue(0.8);
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    sustainSlider.addListener(this);

    // Release
    releaseLabel.setText("Release:", juce::dontSendNotification);
    releaseLabel.attachToComponent(&releaseSlider, true);
    releaseLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(releaseLabel); addAndMakeVisible(releaseSlider);
    releaseSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    releaseSlider.setRange(0.001, 2.0, 0.001); releaseSlider.setValue(0.5);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20); releaseSlider.setSkewFactorFromMidPoint(0.3);
    releaseSlider.addListener(this);

    // --- Filter Controls ---
    // Cutoff
    filterCutoffLabel.setText("Cutoff:", juce::dontSendNotification);
    filterCutoffLabel.attachToComponent(&filterCutoffSlider, true);
    filterCutoffLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(filterCutoffLabel); addAndMakeVisible(filterCutoffSlider);
    filterCutoffSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    filterCutoffSlider.setRange(20.0, 20000.0, 0.1); filterCutoffSlider.setSkewFactorFromMidPoint(1000.0);
    filterCutoffSlider.setValue(filterCutoffRef.load(), juce::dontSendNotification); // Use ref for initial value
    filterCutoffSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20); filterCutoffSlider.setTextValueSuffix(" Hz");
    filterCutoffSlider.addListener(this);

    // Resonance
    filterResonanceLabel.setText("Resonance:", juce::dontSendNotification);
    filterResonanceLabel.attachToComponent(&filterResonanceSlider, true);
    filterResonanceLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(filterResonanceLabel); addAndMakeVisible(filterResonanceSlider);
    filterResonanceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    filterResonanceSlider.setRange(1.0 / std::sqrt(2.0), 18.0, 0.01);
    filterResonanceSlider.setValue(filterResonanceRef.load(), juce::dontSendNotification); // Use ref for initial value
    filterResonanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20); filterResonanceSlider.setNumDecimalPlacesToDisplay(2);
    filterResonanceSlider.addListener(this);

    // --- Scale Controls (NEW Setup) ---
    // Root Note Selector
    rootNoteLabel.setText("Root Note:", juce::dontSendNotification);
    rootNoteLabel.attachToComponent(&rootNoteSelector, true);
    rootNoteLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(rootNoteLabel);
    addAndMakeVisible(rootNoteSelector);
    rootNoteSelector.setEditableText(false);
    rootNoteSelector.setJustificationType(juce::Justification::centred);
    rootNoteSelector.clear();
    for (int i = 0; i < 12; ++i) { // Populate C=0, C#=1 ... B=11
        rootNoteSelector.addItem(juce::MidiMessage::getMidiNoteName(i, true, false, 3), i + 1); // Use ID 1-12
    }
    rootNoteSelector.setSelectedId(rootNoteRef.load() + 1, juce::dontSendNotification); // Set initial based on atomic state (0-11) + 1
    rootNoteSelector.addListener(this);


    // Scale Type Selector
    scaleTypeLabel.setText("Scale Type:", juce::dontSendNotification);
    scaleTypeLabel.attachToComponent(&scaleTypeSelector, true);
    scaleTypeLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(scaleTypeLabel);
    addAndMakeVisible(scaleTypeSelector);
    scaleTypeSelector.setEditableText(false);
    scaleTypeSelector.setJustificationType(juce::Justification::centred);
    scaleTypeSelector.clear();
    // Populate using the names provided by MainComponent's getter
    const auto& scaleNames = mainComponentPtr->getScaleNames();
    for (int i = 0; i < scaleNames.size(); ++i) {
        // Assuming scale IDs defined in MainComponent::ScaleType start from 1 and match vector index+1
        scaleTypeSelector.addItem(scaleNames[i], i + 1); // ID = index + 1
    }
    scaleTypeSelector.setSelectedId(scaleTypeRef.load(), juce::dontSendNotification); // Set initial based on atomic state (1, 2, 3...)
    scaleTypeSelector.addListener(this);

    // Call once initially to set default ADSR params in MainComponent from slider values
    updateADSRParameters();
    // Initial filter update happens in MainComponent::prepareToPlay
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
    rootNoteSelector.removeListener(this);    // <-- Remove new listeners
    scaleTypeSelector.removeListener(this);   // <-- Remove new listeners
}

void ControlsComponent::paint(juce::Graphics& g) // No override
{
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);   // Draw outline
}

// UPDATE resized to include scale controls
// --- REPLACE resized function in ControlsComponent.cpp ---
void ControlsComponent::resized() // No override
{
    auto bounds = getLocalBounds().reduced(10); // Internal margin
    auto labelWidth = 80;    // Width for labels
    auto controlHeight = 25; // Height for controls
    auto spacing = 5;        // Vertical spacing

    // Helper lambda (no changes needed)
    auto layoutRow = [&](juce::Component& control) {
        auto rowBounds = bounds.removeFromTop(controlHeight);
        if (rowBounds.getHeight() < controlHeight) return; // Stop if not enough vertical space
        control.setBounds(labelWidth, rowBounds.getY(), rowBounds.getWidth() - labelWidth, controlHeight);
        bounds.removeFromTop(spacing);
        };

    // --- REORDERED Layout ---
    layoutRow(rootNoteSelector);    // <-- Moved Up
    layoutRow(scaleTypeSelector);   // <-- Moved Up
    layoutRow(waveformSelector);    // <-- Now 3rd
    layoutRow(levelSlider);
    layoutRow(tuneSlider);
    layoutRow(transposeSlider);
    layoutRow(attackSlider);
    layoutRow(decaySlider);
    layoutRow(sustainSlider);
    layoutRow(releaseSlider);
    layoutRow(filterCutoffSlider);
    layoutRow(filterResonanceSlider);
}

// UPDATE comboBoxChanged to handle new selectors
void ControlsComponent::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) // No override
{
    if (mainComponentPtr == nullptr) return;

    if (comboBoxThatHasChanged == &waveformSelector)
    {
        int newWaveId = waveformSelector.getSelectedId();
        waveformSelectionRef.store(newWaveId); // Update atomic directly
        mainComponentPtr->setWaveform(newWaveId); // Also notify MainComponent
        DBG("ControlsComponent: Waveform changed to ID: " + juce::String(newWaveId));
    }
    else if (comboBoxThatHasChanged == &rootNoteSelector) // <-- ADDED handling
    {
        // ComboBox ID is note index + 1 (1-12), convert back to 0-11 for MainComponent
        int rootNoteIndex = rootNoteSelector.getSelectedId() - 1;
        if (rootNoteIndex >= 0 && rootNoteIndex < 12) {
            mainComponentPtr->setRootNote(rootNoteIndex); // Call setter on MainComponent
        }
    }
    else if (comboBoxThatHasChanged == &scaleTypeSelector) // <-- ADDED handling
    {
        int scaleId = scaleTypeSelector.getSelectedId();
        // Assuming scaleId corresponds directly to MainComponent::ScaleType enum values (1, 2, 3...)
        mainComponentPtr->setScaleType(scaleId); // Call setter on MainComponent
    }
}

// UPDATE sliderValueChanged to use setters for Tune/Transpose/Filter
void ControlsComponent::sliderValueChanged(juce::Slider* sliderThatWasMoved) // No override
{
    if (mainComponentPtr == nullptr) return;

    if (sliderThatWasMoved == &levelSlider)
    {
        levelSmootherRef.setTargetValue((float)levelSlider.getValue());
        // DBG("ControlsComponent: Level Target -> " + juce::String(levelSlider.getValue())); // Optional Log
    }
    else if (sliderThatWasMoved == &tuneSlider)
    {
        mainComponentPtr->setFineTune((float)tuneSlider.getValue()); // Call setter
    }
    else if (sliderThatWasMoved == &transposeSlider)
    {
        mainComponentPtr->setTranspose((int)transposeSlider.getValue()); // Call setter
    }
    else if (sliderThatWasMoved == &filterCutoffSlider ||
        sliderThatWasMoved == &filterResonanceSlider)
    {
        float cutoff = (float)filterCutoffSlider.getValue();
        float res = (float)filterResonanceSlider.getValue();
        mainComponentPtr->updateFilter(cutoff, res); // Update via MainComponent::updateFilter
    }
    // If any ADSR slider moved, update all ADSR params via helper
    else if (sliderThatWasMoved == &attackSlider ||
        sliderThatWasMoved == &decaySlider ||
        sliderThatWasMoved == &sustainSlider ||
        sliderThatWasMoved == &releaseSlider)
    {
        updateADSRParameters(); // Calls MainComponent::updateADSR
    }
}

// Helper function updateADSRParameters remains the same
void ControlsComponent::updateADSRParameters()
{
    if (mainComponentPtr != nullptr)
    {
        float a = (float)attackSlider.getValue();
        float d = (float)decaySlider.getValue();
        float s = (float)sustainSlider.getValue();
        float r = (float)releaseSlider.getValue();
        mainComponentPtr->updateADSR(a, d, s, r);
    }
}