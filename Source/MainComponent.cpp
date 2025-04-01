#include "MainComponent.h" // Includes ControlsComponent.h, SynthEngine.h implicitly now
#include <cmath>            // For std::pow, std::fmod, std::abs, std::sin
#include <juce_audio_utils/juce_audio_utils.h> // For MidiMessage
#include <juce_core/system/juce_TargetPlatform.h> // For DBG


// --- REPLACE MainComponent Constructor ---
MainComponent::MainComponent() :
    // REMOVE controlsPanel from initializer list
    smoothedLevel(0.75f)
{
    // --- Define Scale Patterns FIRST ---
    scaleData.push_back({ "Major",        { 0, 2, 4, 5, 7, 9, 11 } });
    scaleData.push_back({ "Natural Minor",{ 0, 2, 3, 5, 7, 8, 10 } });
    scaleData.push_back({ "Dorian",       { 0, 2, 3, 5, 7, 9, 10 } });
    // Add more scales here...

    scaleNames.clear();
    for (const auto& scaleInfo : scaleData)
        scaleNames.add(scaleInfo.name);
    // --- End Define Scale Patterns ---

    // --- NOW Create ControlsComponent using make_unique ---
    controlsPanel = std::make_unique<ControlsComponent>(this,
        currentWaveform,
        smoothedLevel,
        fineTuneSemitones,
        transposeSemitones,
        filterCutoffHz,
        filterResonance,
        rootNote,
        currentScaleType); // Pass all required refs

    // Add and make child components visible
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(*controlsPanel); // <-- Use * to dereference unique_ptr

    // Keyboard setup
    setWantsKeyboardFocus(true);
    addKeyListener(this); // Workaround

    // Window size
    setSize(800, 550);

    // Set Default ADSR Parameters
    updateADSR(0.05f, 0.1f, 0.8f, 0.5f);

    // Set initial synth waveform
    synthEngine.setWaveform(currentWaveform.load());

    // Initialize audio device
    setAudioChannels(0, 2);
}

MainComponent::~MainComponent() // No override needed on definition
{
    removeKeyListener(this);
    shutdownAudio();
    // Child components (oscilloscope, controlsPanel, synthEngine) are direct members,
    // their destructors are called automatically.
}
// --- ADD THESE TWO NEW SETTERS ---
void MainComponent::setRootNote(int rootNoteIndex) // rootNoteIndex is 0-11
{
    jassert(rootNoteIndex >= 0 && rootNoteIndex < 12); // Basic validation
    if (rootNote.load() != rootNoteIndex)
    {
        rootNote.store(rootNoteIndex); // Store the 0-11 value
        DBG("MainComponent: Root Note set to index: " + juce::String(rootNoteIndex)
            + " (" + juce::MidiMessage::getMidiNoteName(rootNoteIndex, true, false, 3) + ")");
        // Pitch of held note will update automatically via keyStateChanged check or next key press
    }
}

void MainComponent::setScaleType(int scaleId) // scaleId is 1, 2, 3...
{
    // Ensure ID is valid before storing
    if (scaleId > 0 && scaleId <= scaleData.size())
    {
        if (currentScaleType.load() != scaleId)
        {
            currentScaleType.store(scaleId); // Store the ScaleType enum value
            DBG("MainComponent: Scale Type set to ID: " + juce::String(scaleId)
                + " (" + scaleData[scaleId - 1].name + ")");
            // Pitch of held note will update automatically via keyStateChanged check or next key press
        }
    }
    else {
        DBG("MainComponent: Invalid Scale Type ID received: " + juce::String(scaleId));
    }
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

// --- REPLACE setFineTune ---
void MainComponent::setFineTune(float semitones)
{
    if (fineTuneSemitones.exchange(semitones) != semitones) // Update atomic state, check if changed
    {
        updateEnginePitch(); // Recalculate and update engine frequency immediately
        DBG("MainComponent: Fine Tune set to: " + juce::String(semitones, 2));
    }
}

// --- REPLACE setTranspose ---
void MainComponent::setTranspose(int semitones)
{
    if (transposeSemitones.exchange(semitones) != semitones) // Update atomic state, check if changed
    {
        updateEnginePitch(); // Recalculate and update engine frequency immediately
        DBG("MainComponent: Transpose set to: " + juce::String(semitones));
    }
}


//==============================================================================
// --- Private helper method ---
//==============================================================================
// --- ADD This Private Helper Method ---
// --- ADD This Private Helper Method ---
void MainComponent::updateEnginePitch()
{
    // Recalculates final frequency based on the *currently held base MIDI note*
    // and the latest tuning/transpose values, then tells the synth engine.

    if (currentlyPlayingNote != -1 && currentSampleRate > 0.0)
    {
        int baseMidiNote = currentlyPlayingNote; // Note from scale mapping
        int currentTranspose = transposeSemitones.load();
        float currentFineTune = fineTuneSemitones.load();

        // Apply transpose first
        int transposedMidiNote = juce::jlimit(0, 127, baseMidiNote + currentTranspose);

        // Calculate frequency after transpose
        double baseFrequency = juce::MidiMessage::getMidiNoteInHertz(transposedMidiNote);

        // Apply fine tuning
        double adjustedFrequency = baseFrequency * std::pow(2.0, currentFineTune / 12.0);

        // Tell the synth engine the new final frequency
        synthEngine.setFrequency(adjustedFrequency);

        DBG("MainComponent::updateEnginePitch - Final Freq set: " + juce::String(adjustedFrequency, 2)
            + " (BaseMIDI=" + juce::String(baseMidiNote) + ", Trans=" + juce::String(currentTranspose) + ", Fine=" + juce::String(currentFineTune, 2) + ")");
    }
    else
    {
        // If no note is playing, set engine frequency to 0
        synthEngine.setFrequency(0.0);
        // DBG("MainComponent::updateEnginePitch - No note playing, engine frequency set to 0.");
    }
}


//==============================================================================
// Component overrides (Definitions without override)
//==============================================================================
// --- REPLACE paint function in MainComponent.cpp ---
void MainComponent::paint(juce::Graphics& g) // No override definition
{
    // Fill the background ONLY
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // --- All text drawing code removed ---
}

// --- REPLACE resized function in MainComponent.cpp ---
void MainComponent::resized() // No override definition
{
    auto bounds = getLocalBounds(); // Get the total area of MainComponent
    auto margin = 10;               // Margin size in pixels

    // --- REMOVED textHeight and initial removeFromTop ---

    // Layout: Scope starts at the top (with margin)
    auto scopeHeight = 120; // Height for the oscilloscope
    // Use reduced bounds directly for placing first element
    auto scopeBounds = bounds.reduced(margin).removeFromTop(scopeHeight); // Reduce by margin first
    oscilloscope.setBounds(scopeBounds);

    // Adjust remaining bounds - remove scope height AND margin below it
    bounds.removeFromTop(scopeBounds.getBottom() + margin); // Use scope's bottom edge + margin

    // Controls panel takes remaining space at the bottom
    // Check if controlsPanel unique_ptr is valid before accessing
    if (controlsPanel != nullptr)
        controlsPanel->setBounds(bounds.reduced(margin)); // Reduce remaining bounds by margin

    // Update DBG logs
    DBG("MainComponent::resized() - Oscilloscope Bounds: " + oscilloscope.getBounds().toString());
    if (controlsPanel != nullptr)
        DBG("MainComponent::resized() - Controls Bounds: " + controlsPanel->getBounds().toString());
}

// --- REPLACE keyPressed function ---
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) // No override definition
{
    int keyCode = key.getKeyCode();
    // Check if key is *already* logically down according to our map
    if (keysDown.count(keyCode) && keysDown[keyCode]) {
        // DBG("keyPressed: Key code " + juce::String(keyCode) + " ignored (already down).");
        return false; // Prevent auto-repeat trigger
    }

    // --- Map KeyCode to an index based on defined layout ---
    const juce::String keyOrder = "QWERTYUIOPASDFGHJKLZXCVBNM"; // 26 keys
    int keyIndex = keyOrder.indexOfChar((juce::juce_wchar)keyCode);

    if (keyIndex == -1) // Key not in our defined layout
    {
        // DBG("keyPressed: Key code " + juce::String(keyCode) + " (" + key.getTextDescription() + ") - Not in keyOrder map.");
        return false;
    }

    DBG("keyPressed: Key code " + juce::String(keyCode) + " (" + key.getTextDescription() + ")");

    // --- Calculate MIDI Note based on Root, Scale, and Key Index ---
    int rootNoteIndex = rootNote.load(); // 0-11 (C=0)
    int scaleTypeId = currentScaleType.load(); // 1=Major, 2=Minor, ...
    int scalePatternIndex = scaleTypeId - 1; // Adjust for 0-based vector index

    // Ensure scale pattern exists and is valid (size 7)
    if (scalePatternIndex < 0 || scalePatternIndex >= scaleData.size() || scaleData[scalePatternIndex].intervals.size() != 7) {
        DBG("  Invalid scale type selected or scaleData incorrect! ScaleID=" + juce::String(scaleTypeId));
        // Maybe flash UI or log more prominently? For now, just don't play.
        return false; // Cannot proceed
    }
    const auto& intervals = scaleData[scalePatternIndex].intervals; // Get intervals {0, 2, 4...}

    // Calculate reference MIDI note for 'A' key (index 10) - Root Note's pitch in octave closest to Middle C (60)
    int refMidiNote = 12 * (int)std::round((60.0 - rootNoteIndex) / 12.0) + rootNoteIndex;
    int refKeyIndex = keyOrder.indexOfChar('A'); // Should be 10

    int offset = keyIndex - refKeyIndex; // Offset in scale steps from 'A' key

    // Calculate octave shift and degree index within the scale pattern
    int octaveShift = floor((double)offset / 7.0); // How many full octaves up/down relative to 'A's octave
    int degreeIndex = ((offset % 7) + 7) % 7; // Index within the 7 scale intervals (0-6)

    // Get the interval (in semitones) from the root for this scale degree
    int interval = intervals[degreeIndex];

    // Calculate the final base MIDI note number (Root's Octave + Octave Shift + Interval)
    int finalMidiNote = refMidiNote + (octaveShift * 12) + interval; // intervals[0] is 0

    // Clamp to valid MIDI range
    finalMidiNote = juce::jlimit(0, 127, finalMidiNote);

    // --- Store state and trigger sound ---
    keysDown[keyCode] = true; // Mark key down in our map NOW
    currentlyPlayingNote = finalMidiNote; // Store the base MIDI note being played

    updateEnginePitch(); // Calculate final freq (incl tune/transpose) and tell engine
    synthEngine.setWaveform(currentWaveform.load()); // Ensure waveform is set
    synthEngine.noteOn(); // Tell engine to start ADSR

    DBG("  Key Mapped: Key='" + key.getTextDescription() + "', Offset=" + juce::String(offset)
        + ", OctShift=" + juce::String(octaveShift) + ", DegIdx=" + juce::String(degreeIndex)
        + ", Interval=" + juce::String(interval) + ", FinalMIDI=" + juce::String(finalMidiNote)
        + ", ADSR Note ON");

    return true; // Handled
}


// --- REPLACE keyStateChanged function ---
// --- REPLACE keyStateChanged function ---
bool MainComponent::keyStateChanged(bool /*isKeyDown*/, juce::Component* /*originatingComponent*/) // No override definition
{
    // DBG("keyStateChanged called."); // Keep commented unless needed for detailed debug
    bool shouldBePlaying = false;
    int lastKeyDownCode = -1;
    int highestKeyIndex = -1; // Track index of highest priority key still down

    const juce::String keyOrder = "QWERTYUIOPASDFGHJKLZXCVBNM";

    // Check which keys *we care about* are still physically down
    for (auto it = keysDown.begin(); it != keysDown.end(); ) {
        int currentKeyCode = it->first;
        if (juce::KeyPress::isKeyCurrentlyDown(currentKeyCode)) {
            shouldBePlaying = true;
            it->second = true; // Update map state
            int currentKeyIndex = keyOrder.indexOfChar((juce::juce_wchar)currentKeyCode);
            if (currentKeyIndex > highestKeyIndex) { // Track the highest index found
                highestKeyIndex = currentKeyIndex;
                lastKeyDownCode = currentKeyCode; // Store the code of the highest key
            }
            ++it;
        }
        else {
            // If key is UP, remove it from our map of tracked keys
             // DBG("  Key Up detected in keyStateChanged: " + juce::String(currentKeyCode));
            it = keysDown.erase(it); // Erase returns iterator to the next element
        }
    }

    if (shouldBePlaying) {
        // --- Handle potential note change if multiple keys were held ---
        int rootNoteIndex = rootNote.load();
        int scaleTypeId = currentScaleType.load();
        int scalePatternIndex = juce::jlimit(0, (int)scaleData.size() - 1, scaleTypeId - 1);

        // Safety check scale data
        if (scaleData.empty() || scalePatternIndex >= scaleData.size() || scaleData[scalePatternIndex].intervals.size() != 7) {
            DBG("  keyStateChanged: Invalid scale data! Forcing note off.");
            if (currentlyPlayingNote != -1) {
                synthEngine.noteOff();
                currentlyPlayingNote = -1;
                updateEnginePitch(); // Tell engine freq is 0
            }
            return true;
        }
        const auto& intervals = scaleData[scalePatternIndex].intervals;
        int refMidiNote = 12 * (int)std::round((60.0 - rootNoteIndex) / 12.0) + rootNoteIndex;
        int refKeyIndex = keyOrder.indexOfChar('A');
        int offset = highestKeyIndex - refKeyIndex;
        int octaveShift = floor((double)offset / 7.0);
        int degreeIndex = ((offset % 7) + 7) % 7;
        int interval = intervals[degreeIndex];
        int newMidiNote = juce::jlimit(0, 127, refMidiNote + (octaveShift * 12) + interval);

        // Check if the highest sounding note has changed
        if (newMidiNote != currentlyPlayingNote) {
            currentlyPlayingNote = newMidiNote; // Store the new note
            updateEnginePitch(); // Recalculate final freq and tell engine
            synthEngine.setWaveform(currentWaveform.load()); // Ensure waveform correct
            // Optional: Retrigger ADSR? Let's keep legato feel for now.
            // synthEngine.noteOn();
            DBG("  Note Changed/Retriggered: New MIDI=" + juce::String(currentlyPlayingNote));
        }
        // If the same note is still held, do nothing - ADSR continues
    }
    else // --- All relevant keys are now released ---
    {
        if (currentlyPlayingNote != -1) // Only trigger noteOff if a note was actually playing
        {
            DBG("  All relevant keys released. Triggering ADSR Note OFF.");
            synthEngine.noteOff(); // <<< Trigger ADSR Release >>>
            currentlyPlayingNote = -1; // Mark no note as playing
            // --- REMOVED resetting targetFrequency and angleDelta here ---
            // updateEnginePitch(); // Call this to tell engine frequency is now 0
        }
    }
    return true; // Handled state change
}