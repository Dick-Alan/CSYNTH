#include "MainComponent.h" // Includes ControlsComponent.h, SynthEngine.h implicitly now
#include <cmath>            // For std::pow, std::fmod, std::abs, std::sin
#include <juce_audio_utils/juce_audio_utils.h> // For MidiMessage
#include <juce_core/system/juce_TargetPlatform.h> // For DBG


//==============================================================================
MainComponent::MainComponent() :
    // Initialize controlsPanel FIRST, passing this pointer and required references
    controlsPanel(this, currentWaveform, smoothedLevel, fineTuneSemitones, transposeSemitones, filterCutoffHz, filterResonance),
    smoothedLevel(0.75f) // Initialize SmoothedValue default level
    // Other members (synthEngine, oscilloscope, atomics, map) are default constructed
{
    // Make child components visible AFTER they are constructed
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(controlsPanel); // Direct member access

    // Keyboard setup
    setWantsKeyboardFocus(true);
    addKeyListener(this); // Workaround for key event handling

    // Window size - ensure enough height for controls
    setSize(800, 550); // Adjust if needed

    // Set Default ADSR Parameters directly in the engine via public method
    // (Ensure default slider values in ControlsComponent match these)
    updateADSR(0.05f, 0.1f, 0.8f, 0.5f);

    // Set initial synth waveform
    synthEngine.setWaveform(currentWaveform.load());

    // Initialize audio device requesting 0 inputs and 2 outputs
    setAudioChannels(0, 2);
}

MainComponent::~MainComponent() // No override needed on definition
{
    removeKeyListener(this);
    shutdownAudio();
    // Child components (oscilloscope, controlsPanel, synthEngine) are direct members,
    // their destructors are called automatically.
}

//==============================================================================
// --- REPLACE prepareToPlay function ---
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) // No override definition
{
    currentSampleRate = sampleRate; // Store sample rate

    // Prepare the level smoother
    smoothedLevel.reset(sampleRate, 0.02);
    smoothedLevel.setCurrentAndTargetValue(0.75f); // Ensure value is set after reset

    // Prepare the synth engine - Use constant '2' for numOutputChannels
    int numOutputChannels = 2; // <<< FIXED: Use 2 directly since we called setAudioChannels(0, 2)
    synthEngine.prepareToPlay(sampleRate, samplesPerBlockExpected, numOutputChannels);

    // Reset note state
    currentlyPlayingNote = -1;

    // Call update methods once initially AFTER prepareToPlay
    updateEnginePitch();
    updateFilter(filterCutoffHz.load(), filterResonance.load());

    DBG("MainComponent::prepareToPlay called. Sample Rate: " + juce::String(currentSampleRate));
}



void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) // No override definition
{
    // Get buffer pointer and number of samples
    auto* buffer = bufferToFill.buffer;
    auto numSamples = buffer->getNumSamples();
    auto startSample = bufferToFill.startSample;

    // --- 1. Let the SynthEngine render its output (Osc -> Filter -> ADSR) ---
    // The engine handles its own internal state checks (e.g., adsr.isActive)
    synthEngine.renderNextBlock(*buffer, startSample, numSamples);

    // --- 2. Apply the smoothed Master Level gain ---
    // Apply gain sample-by-sample using the SmoothedValue
    auto* leftChan = buffer->getWritePointer(0, startSample);
    auto* rightChan = buffer->getNumChannels() > 1 ? buffer->getWritePointer(1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float gain = smoothedLevel.getNextValue(); // Get next smoothed gain value
        leftChan[i] *= gain; // Apply gain
        if (rightChan != nullptr)
            rightChan[i] *= gain;
    }

    // --- 3. Copy final result to Oscilloscope ---
    // Get the frequency the engine is currently using
    float currentFreq = (float)synthEngine.getCurrentFrequency();
    oscilloscope.copySamples(leftChan, // Use the final processed left channel data
        numSamples,
        currentFreq); // Pass frequency to scope
}
void MainComponent::updateFilter(float cutoff, float resonance)
{   
    DBG("MainComponent::updateFilter called. Cutoff=" + juce::String(cutoff) + ", Res=" + juce::String(resonance) + ". Calling synthEngine.setFilterParameters...");
    // Optional: Store atomic values if needed elsewhere, though engine now holds the state
    // filterCutoffHz.store(cutoff);
    // filterResonance.store(resonance);

    // Tell the synth engine to update its internal filter parameters
    synthEngine.setFilterParameters(cutoff, resonance);

    DBG("MainComponent: Filter updated - Cutoff: " + juce::String(cutoff, 1)
        + " Hz, Resonance: " + juce::String(resonance, 2));
}
void MainComponent::releaseResources() // No override definition
{
    // Called when playback stops or audio device changes.
    DBG("MainComponent::releaseResources called.");
}

//==============================================================================
// --- Public methods called by ControlsComponent ---
//==============================================================================

void MainComponent::updateADSR(float attack, float decay, float sustain, float release)
{
    // Create temporary params struct to pass to engine
    juce::ADSR::Parameters newParams;
    newParams.attack = juce::jmax(0.001f, attack);
    newParams.decay = juce::jmax(0.001f, decay);
    newParams.sustain = juce::jlimit(0.0f, 1.0f, sustain); // Clamp sustain 0-1
    newParams.release = juce::jmax(0.001f, release);

    // Tell the synth engine to update its parameters
    synthEngine.setParameters(newParams);

    DBG("MainComponent: ADSR Params Updated: A=" + juce::String(newParams.attack, 3)
        + " D=" + juce::String(newParams.decay, 3)
        + " S=" + juce::String(newParams.sustain, 2)
        + " R=" + juce::String(newParams.release, 3));
}

void MainComponent::setWaveform(int typeId)
{
    currentWaveform.store(typeId); // Update our atomic state
    synthEngine.setWaveform(typeId); // Tell the engine
    DBG("MainComponent: Waveform set to ID: " + juce::String(typeId));
}

void MainComponent::setFineTune(float semitones)
{
    fineTuneSemitones.store(semitones); // Update atomic state
    updateEnginePitch(); // Recalculate and update engine frequency
    DBG("MainComponent: Fine Tune set to: " + juce::String(semitones));
}

void MainComponent::setTranspose(int semitones)
{
    transposeSemitones.store(semitones); // Update atomic state
    updateEnginePitch(); // Recalculate and update engine frequency
    DBG("MainComponent: Transpose set to: " + juce::String(semitones));
}


//==============================================================================
// --- Private helper method ---
//==============================================================================
void MainComponent::updateEnginePitch()
{
    // This method recalculates the final frequency based on the
    // currently held note (if any) and the latest tune/transpose values,
    // then tells the synth engine.

    if (currentlyPlayingNote != -1) // Only update if a note is actually supposed to be playing
    {
        int baseMidiNote = currentlyPlayingNote;
        int currentTranspose = transposeSemitones.load();
        float currentFineTune = fineTuneSemitones.load();

        int transposedMidiNote = juce::jlimit(0, 127, baseMidiNote + currentTranspose);
        double baseFrequency = juce::MidiMessage::getMidiNoteInHertz(transposedMidiNote);
        double adjustedFrequency = baseFrequency * std::pow(2.0, currentFineTune / 12.0);

        synthEngine.setFrequency(adjustedFrequency); // Tell engine the new frequency

        DBG("MainComponent::updateEnginePitch - MIDI: " + juce::String(baseMidiNote)
            + " -> " + juce::String(transposedMidiNote)
            + ", Freq: " + juce::String(adjustedFrequency));
    }
    // If no note is playing, the engine's frequency doesn't need immediate update.
    // It will be set correctly when the next noteOn occurs.
}


//==============================================================================
// Component overrides (Definitions without override)
//==============================================================================
// --- REPLACE paint function ---
void MainComponent::paint(juce::Graphics& g) // No override definition
{
    // Just fill the background
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // Text drawing code removed
}

// --- REPLACE resized function ---
void MainComponent::resized() // No override definition
{
    auto bounds = getLocalBounds(); // Get the total area of MainComponent
    auto margin = 10;               // Margin size in pixels

    // Remove the lines that reserved space for textHeight at the top
    // bounds.removeFromTop(textHeight + margin); // REMOVED

    // Layout: Scope starts nearer the top now
    auto scopeHeight = 120; // Height for the oscilloscope
    // Use reduced bounds directly for placing first element
    auto scopeBounds = bounds.reduced(margin, margin).removeFromTop(scopeHeight);
    oscilloscope.setBounds(scopeBounds);

    // Adjust remaining bounds - remove scope height AND margin below it
    bounds.removeFromTop(scopeHeight + margin + margin); // Remove scope + top margin + bottom margin

    // Controls panel takes remaining space at the bottom
    controlsPanel.setBounds(bounds.reduced(margin, margin)); // Reduce remaining bounds by margin

    // Update DBG logs (using direct member access)
    DBG("MainComponent::resized() - Oscilloscope Bounds: " + oscilloscope.getBounds().toString());
    DBG("MainComponent::resized() - Controls Bounds: " + controlsPanel.getBounds().toString());
}


// --- REPLACE keyPressed function ---
// --- REPLACE keyPressed function ---
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) // No override definition
{
    int keyCode = key.getKeyCode();
    // Check if key is *already* logically down according to our map
    if (keysDown.count(keyCode) && keysDown[keyCode]) {
        DBG("keyPressed: Key code " + juce::String(keyCode) + " ignored (already down).");
        return false; // Prevent auto-repeat trigger by OS
    }

    DBG("keyPressed: Key code " + juce::String(keyCode) + " (" + key.getTextDescription() + ")");

    int baseMidiNote = -1;
    // --- Key Mapping Switch ---
    switch (keyCode)
    {
    case 'A': baseMidiNote = 60; break; // C4
    case 'W': baseMidiNote = 61; break; // C#4
    case 'S': baseMidiNote = 62; break; // D4
    case 'E': baseMidiNote = 63; break; // D#4
    case 'D': baseMidiNote = 64; break; // E4
    case 'F': baseMidiNote = 65; break; // F4
    case 'T': baseMidiNote = 66; break; // F#4
    case 'G': baseMidiNote = 67; break; // G4
    case 'Y': baseMidiNote = 68; break; // G#4
    case 'H': baseMidiNote = 69; break; // A4 (440 Hz)
    case 'U': baseMidiNote = 70; break; // A#4
    case 'J': baseMidiNote = 71; break; // B4
    case 'K': baseMidiNote = 72; break; // C5
    default:
        DBG("  Key not mapped.");
        return false; // Indicate we didn't handle this key
    }
    // --- End Key Mapping Switch ---

    // If we get here, key was mapped
    keysDown[keyCode] = true; // Mark key as logically down NOW

    if (baseMidiNote > 0)
    {
        currentlyPlayingNote = baseMidiNote; // Store the new base note

        // Calculate final pitch (including tune/transpose) and tell the engine
        updateEnginePitch();

        // Tell the engine which waveform to use
        synthEngine.setWaveform(currentWaveform.load());

        // Trigger the ADSR Note ON in the engine
        synthEngine.noteOn();

        // Log the event (using currentlyPlayingNote which holds the base MIDI)
        DBG("  Key Mapped: BaseMIDI=" + juce::String(currentlyPlayingNote) + ", ADSR Note ON");

        return true; // Handled
    }

    // Should not be reached if default case handles unmapped keys properly
    return false;
}


// --- REPLACE keyStateChanged function ---
bool MainComponent::keyStateChanged(bool /*isKeyDown*/, juce::Component* /*originatingComponent*/) // No override definition
{
    DBG("keyStateChanged called.");
    bool shouldBePlaying = false;
    int lastKeyDownCode = -1;

    // Update internal map based on which keys *we care about* are physically down
    for (auto it = keysDown.begin(); it != keysDown.end(); ) {
        int currentKeyCode = it->first;
        if (juce::KeyPress::isKeyCurrentlyDown(currentKeyCode)) {
            shouldBePlaying = true;
            it->second = true; // Update map state
            lastKeyDownCode = currentKeyCode; // Remember the last valid key found down
            ++it;
        }
        else {
            // If key is UP, remove it from our map of tracked keys
            DBG("  Key Up detected in keyStateChanged: " + juce::String(currentKeyCode));
            it = keysDown.erase(it); // Erase returns iterator to the next element
        }
    }

    if (shouldBePlaying) {
        // --- Handle potential note change if multiple keys were held ---
        int newBaseMidiNote = -1; // Find the MIDI note for the key still held down
        // --- FULL Key Mapping Switch ---
        switch (lastKeyDownCode)
        {
        case 'A': newBaseMidiNote = 60; break; // C4
        case 'W': newBaseMidiNote = 61; break; // C#4
        case 'S': newBaseMidiNote = 62; break; // D4
        case 'E': newBaseMidiNote = 63; break; // D#4
        case 'D': newBaseMidiNote = 64; break; // E4
        case 'F': newBaseMidiNote = 65; break; // F4
        case 'T': newBaseMidiNote = 66; break; // F#4
        case 'G': newBaseMidiNote = 67; break; // G4
        case 'Y': newBaseMidiNote = 68; break; // G#4
        case 'H': newBaseMidiNote = 69; break; // A4 (440 Hz)
        case 'U': newBaseMidiNote = 70; break; // A#4
        case 'J': newBaseMidiNote = 71; break; // B4
        case 'K': newBaseMidiNote = 72; break; // C5
            // No default needed, if lastKeyDownCode doesn't match, newBaseMidiNote remains -1
        }
        // --- End Key Mapping Switch ---

        if (newBaseMidiNote > 0 && newBaseMidiNote != currentlyPlayingNote)
        {
            // A different note is now the highest priority (or only) one held down
            currentlyPlayingNote = newBaseMidiNote; // Store the new note
            updateEnginePitch(); // Recalculate final freq and tell engine
            synthEngine.setWaveform(currentWaveform.load()); // Ensure waveform correct
            // Optional: Retrigger ADSR for legato re-articulation?
            // synthEngine.noteOn();
            DBG("  Note Changed/Retriggered: New MIDI=" + juce::String(currentlyPlayingNote));
        }
        else if (newBaseMidiNote <= 0)
        {
            // This case means the last key down wasn't mapped, but shouldBePlaying was true? Error state.
            DBG("Key state logic error: Should be playing but last key down wasn't mapped? Forcing note off.");
            synthEngine.noteOff(); // Force note off
            currentlyPlayingNote = -1; // Mark no note playing
           
        }
        // If the same note is still held (newBaseMidiNote == currentlyPlayingNote), do nothing - ADSR continues
    }
    else
    {
        // --- All relevant keys are now released ---
        if (currentlyPlayingNote != -1) // Only trigger noteOff if a note was actually playing
        {
            DBG("  All relevant keys released. Triggering ADSR Note OFF.");
            synthEngine.noteOff(); // <<< Trigger ADSR Release >>>
            currentlyPlayingNote = -1; // Mark no note as playing
            // Keep targetFrequency as is, engine uses it until ADSR inactive
        }
    }
    return true; // Handled state change
}