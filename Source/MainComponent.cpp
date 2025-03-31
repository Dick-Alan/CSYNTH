#include "MainComponent.h" // Must be first if using precompiled headers
#include "ControlsComponent.h"
#include <cmath>            // For std::sin, std::pow, std::fmod, std::abs
#include <juce_audio_utils/juce_audio_utils.h> // For MidiMessage
#include <juce_dsp/juce_dsp.h>     // For dsp::AudioBlock etc. (Though not strictly needed for ADSR sample-by-sample)
#include <juce_core/system/juce_TargetPlatform.h> // For DBG

//==============================================================================
MainComponent::MainComponent() :
    // Use std::make_unique in initializer list to create ControlsComponent owned by unique_ptr
    controlsPanel(std::make_unique<ControlsComponent>(this, currentWaveform, smoothedLevel, fineTuneSemitones, transposeSemitones)),
    smoothedLevel(0.75f) // Initialize SmoothedValue default level
    // Atomics (currentWaveform, fineTuneSemitones, transposeSemitones) default initialize ok
    // Other members (adsr, adsrParams, doubles, map) default initialize ok
{
    // Add and make child components visible
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(*controlsPanel); // Dereference unique_ptr to add the component

    // Keyboard setup
    setWantsKeyboardFocus(true);
    addKeyListener(this); // Workaround for key event handling

    // Window size - ensure enough height for controls
    setSize(800, 550); // Adjust if needed

    // --- Set Default ADSR Parameters (used by adsr object) ---
    adsrParams.attack = 0.05f; // seconds
    adsrParams.decay = 0.1f;  // seconds
    adsrParams.sustain = 0.8f;  // level (0.0 to 1.0)
    adsrParams.release = 0.5f;  // seconds

    // Initialize audio device requesting 0 inputs and 2 outputs
    setAudioChannels(0, 2);
}

MainComponent::~MainComponent() // No override needed on definition
{
    removeKeyListener(this); // Clean up listener since we added it
    shutdownAudio();
    // controlsPanel unique_ptr automatically deleted here
}

//==============================================================================
void MainComponent::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) // No override definition
{
    currentSampleRate = sampleRate;
    currentAngle = 0.0;
    angleDelta = 0.0;       // Will be calculated in getNextAudioBlock
    targetFrequency = 0.0;  // Reset frequency
    // isNoteOn removed

    // --- Initialize Level Smoother ---
    smoothedLevel.reset(sampleRate, 0.02); // 20ms ramp time
    smoothedLevel.setCurrentAndTargetValue(0.75f); // Set initial/target value

    // --- Set up ADSR ---
    adsr.setSampleRate(sampleRate); // Tell ADSR the sample rate
    adsr.setParameters(adsrParams); // Apply the initial parameters stored in adsrParams

    DBG("prepareToPlay called. Sample Rate: " + juce::String(currentSampleRate)
        + ", SmoothedLevel Target: " + juce::String(smoothedLevel.getTargetValue()));
}

// --- REPLACE getNextAudioBlock function ---
// --- REPLACE getNextAudioBlock function ---
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) // No override definition
{
    // Get buffer pointer and number of samples
    auto* buffer = bufferToFill.buffer;
    auto numSamples = buffer->getNumSamples();
    auto startSample = bufferToFill.startSample;

    // Check if the ADSR is completely inactive ONLY at the start.
    if (!adsr.isActive())
    {
        buffer->clear(startSample, numSamples); // Ensure silence if inactive
        oscilloscope.copySamples(buffer->getReadPointer(0, startSample), numSamples);
        return; // Exit early
    }

    // --- If ADSR is active (Attack, Decay, Sustain, or Release phase) ---

    // Calculate the angle delta based on the current targetFrequency.
    // During release, targetFrequency still holds the frequency of the released note.
    double currentBlockAngleDelta = 0.0;
    if (currentSampleRate > 0.0 && targetFrequency > 0.0) // Check targetFrequency here
    {
        float currentFineTune = fineTuneSemitones.load();
        double finalFrequency = targetFrequency * std::pow(2.0, currentFineTune / 12.0);
        double cyclesPerSample = finalFrequency / currentSampleRate;
        currentBlockAngleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }
    // If targetFrequency is 0 (e.g., initial state), delta remains 0.

    // Get buffer pointers
    auto waveformType = currentWaveform.load();
    auto* leftBuffer = buffer->getWritePointer(0, startSample);
    auto* rightBuffer = buffer->getWritePointer(1, startSample);

    // Process samples one by one
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // 1. Get the smoothed master level value for this sample
        float currentSmoothedLevel = smoothedLevel.getNextValue();

        // 2. Get the ADSR gain value for this sample (this advances the ADSR state)
        float envelopeGain = adsr.getNextSample();

        // 3. Calculate raw oscillator value
        double currentSampleValue = 0.0;
        // We still calculate the oscillator shape even if gain is low during release,
        // because the phase needs to advance correctly based on the note's frequency.
        if (currentBlockAngleDelta != 0.0)
        {
            double phase = std::fmod(currentAngle, 2.0 * juce::MathConstants<double>::pi) / (2.0 * juce::MathConstants<double>::pi);
            // --- FULL Waveform Calculations ---
            switch (waveformType)
            {
            case Waveform::sine:
                currentSampleValue = std::sin(currentAngle);
                break;
            case Waveform::square:
                currentSampleValue = (phase < 0.5) ? 1.0 : -1.0;
                // TODO: Add anti-aliasing later (e.g., PolyBLEP) for better quality
                break;
            case Waveform::saw:
                currentSampleValue = (2.0 * phase) - 1.0; // Ramps from -1 to +1
                // TODO: Add anti-aliasing later
                break;
            case Waveform::triangle:
                // Corrected triangle calculation: maps phase 0->0.5->1 to value -1->1->-1
                // This version goes from 0 -> 1 -> 0 -> -1 -> 0 etc relative to phase.
                // Let's use a simpler linear version for now:
                currentSampleValue = 4.0 * std::abs(phase - 0.5) - 1.0; // Peaks at +/- 1
                // TODO: Add anti-aliasing later
                break;
            default: // Default to sine if something goes wrong
                currentSampleValue = std::sin(currentAngle);
                break;
            }
            // --- End Waveform Calculations ---

            // Increment oscillator phase angle using the calculated delta
            currentAngle += currentBlockAngleDelta;
        }
        // If delta was 0, currentSampleValue remains 0.

        // 4. Calculate final sample value: Osc * SmoothedLevel * ADSRGain
        float finalSampleValue = (float)(currentSampleValue * currentSmoothedLevel * envelopeGain);

        // 5. Write to output buffers
        leftBuffer[sample] = finalSampleValue;
        rightBuffer[sample] = finalSampleValue;
    }

    // Optional: Wrap main phase angle
    if (currentAngle >= 2.0 * juce::MathConstants<double>::pi)
        currentAngle -= 2.0 * juce::MathConstants<double>::pi;

    // Send final output (left channel) to oscilloscope AFTER all processing
    oscilloscope.copySamples(leftBuffer, numSamples);
}


void MainComponent::releaseResources() // No override definition
{
    // Called when playback stops or audio device changes.
    DBG("releaseResources called.");
}

// --- Method called by ControlsComponent to update ADSR ---
void MainComponent::updateADSR(float attack, float decay, float sustain, float release)
{
    // Update the parameters struct
    adsrParams.attack = juce::jmax(0.001f, attack); // Ensure minimum time
    adsrParams.decay = juce::jmax(0.001f, decay);
    adsrParams.sustain = sustain; // Level 0-1
    adsrParams.release = juce::jmax(0.001f, release);

    // Apply the parameters to the ADSR object
    adsr.setParameters(adsrParams);

    DBG("MainComponent: ADSR Params Updated: A=" + juce::String(adsrParams.attack, 3)
        + " D=" + juce::String(adsrParams.decay, 3)
        + " S=" + juce::String(adsrParams.sustain, 2)
        + " R=" + juce::String(adsrParams.release, 3));
}


//==============================================================================
// Component overrides (Definitions without override)
//==============================================================================
void MainComponent::paint(juce::Graphics& g) // No override definition
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);

    juce::String textToShow = "Click window & press keys (A,S,D...)";
    // Display frequency only if ADSR is active (meaning note is on or releasing)
    if (adsr.isActive())
    {
        float currentFineTune = fineTuneSemitones.load();
        double displayFrequency = targetFrequency * std::pow(2.0, currentFineTune / 12.0);
        if (displayFrequency > 0.0) {
            textToShow = "Playing: " + juce::String(displayFrequency, 2) + " Hz";
        }
    }
    g.drawText(textToShow, getLocalBounds().removeFromTop(30), juce::Justification::centred, true);
}

void MainComponent::resized() // No override definition
{
    auto bounds = getLocalBounds();
    auto margin = 10;
    auto textHeight = 30;
    bounds.removeFromTop(textHeight + margin);

    // Layout: Scope above controls
    auto scopeHeight = 120;
    auto scopeBounds = bounds.removeFromTop(scopeHeight);
    oscilloscope.setBounds(scopeBounds.reduced(margin, 0));

    bounds.removeFromTop(margin);

    // Controls panel takes remaining space - use -> for unique_ptr
    controlsPanel->setBounds(bounds.reduced(margin, margin));

    DBG("MainComponent::resized() - Oscilloscope Bounds: " + oscilloscope.getBounds().toString());
    DBG("MainComponent::resized() - Controls Bounds: " + controlsPanel->getBounds().toString());
}


//==============================================================================
// KeyListener overrides (Definitions without override)
//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) // No override definition
{
    int keyCode = key.getKeyCode();
    // Check if key is *already* logically down according to our map
    if (keysDown.count(keyCode) && keysDown[keyCode]) {
        DBG("keyPressed: Key code " + juce::String(keyCode) + " ignored (already down).");
        return false; // Prevent auto-repeat trigger by OS
    }

    DBG("keyPressed: Key code " + juce::String(keyCode) + " (" + key.getTextDescription() + ")");
    keysDown[keyCode] = true; // Mark as logically down NOW

    int baseMidiNote = -1;
    switch (keyCode) { /* ... key mapping cases ... */
    case 'A': baseMidiNote = 60; break; case 'W': baseMidiNote = 61; break;
    case 'S': baseMidiNote = 62; break; case 'E': baseMidiNote = 63; break;
    case 'D': baseMidiNote = 64; break; case 'F': baseMidiNote = 65; break;
    case 'T': baseMidiNote = 66; break; case 'G': baseMidiNote = 67; break;
    case 'Y': baseMidiNote = 68; break; case 'H': baseMidiNote = 69; break;
    case 'U': baseMidiNote = 70; break; case 'J': baseMidiNote = 71; break;
    case 'K': baseMidiNote = 72; break;
    default:
        // If key isn't mapped, don't mark it as down in our map
        keysDown.erase(keyCode);
        DBG("  Key not mapped.");
        return false;
    }

    if (baseMidiNote > 0) {
        // --- Calculate Target Frequency (includes Transpose) ---
        int currentTranspose = transposeSemitones.load();
        int transposedMidiNote = juce::jlimit(0, 127, baseMidiNote + currentTranspose);
        targetFrequency = juce::MidiMessage::getMidiNoteInHertz(transposedMidiNote); // Store base target freq

        // --- Trigger ADSR Note ON ---
        // AngleDelta is calculated per-block in getNextAudioBlock now
        adsr.noteOn(); // <<< Call noteOn() >>>

        DBG("  Key Mapped: BaseMIDI=" + juce::String(baseMidiNote)
            + ", Transpose=" + juce::String(currentTranspose)
            + ", FinalMIDI=" + juce::String(transposedMidiNote)
            + ", TargetFreq=" + juce::String(targetFrequency) + ", ADSR Note ON");
        return true; // Handled
    }
    return false; // Should only be reached if baseMidiNote <= 0 somehow
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
        // Check the actual current state using JUCE's function
        if (juce::KeyPress::isKeyCurrentlyDown(currentKeyCode)) {
            shouldBePlaying = true;
            it->second = true; // Ensure map state is true if key is down
            lastKeyDownCode = currentKeyCode; // Remember the last valid key found down
            ++it; // Move to next element in map
        }
        else {
            // If key is UP, remove it from our map of active notes
            DBG("  Key Up detected in keyStateChanged: " + juce::String(currentKeyCode));
            it = keysDown.erase(it); // Erase returns iterator to the next element
        }
    }

    if (shouldBePlaying) {
        // --- Handle potential note change if multiple keys were held ---
        int baseMidiNote = -1;
        // --- FULL Switch statement ---
        switch (lastKeyDownCode) // Use the code of the last key confirmed down
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
            // No default needed here, if lastKeyDownCode doesn't match, baseMidiNote remains -1
        }
        // --- End Switch statement ---

        if (baseMidiNote > 0) {
            // Calculate the frequency for the *newly remaining* key
            int currentTranspose = transposeSemitones.load();
            int transposedMidiNote = juce::jlimit(0, 127, baseMidiNote + currentTranspose);
            double newTargetFrequency = juce::MidiMessage::getMidiNoteInHertz(transposedMidiNote);

            // Check if the target frequency actually needs updating
            // (using approximatelyEqual for floating point comparison)
            if (!juce::approximatelyEqual(newTargetFrequency, targetFrequency)) {
                targetFrequency = newTargetFrequency; // Update target freq for audio thread
                DBG("  Note Changed/Retriggered: New TargetFreq=" + juce::String(targetFrequency));
                // Optional: Retrigger note? adsr.noteOn();
            }
            // Note remains ON (ADSR state continues)
        }
        else {
            // This case means the last key down wasn't mapped, but shouldBePlaying was true? Error state.
            DBG("Key state logic error: Should be playing but last key down wasn't mapped? Forcing note off.");
            adsr.noteOff(); // Force note off
            targetFrequency = 0.0; // Reset frequency if error
            // angleDelta will become 0 in next getNextAudioBlock calculation
        }
    }
    else {
        // --- All relevant keys are now released ---
        DBG("  All relevant keys released. Triggering ADSR Note OFF.");
        adsr.noteOff(); // <<< Trigger ADSR Release >>>
        // DO NOT reset targetFrequency here. Let it persist for the release calculation in getNextAudioBlock.
    }
    return true; // Handled state change
}