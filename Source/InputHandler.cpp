#include "InputHandler.h"
#include "MainComponent.h" // Included for ScaleInfo, enum, DBG usually via JuceHeader.h anyway
#include <cmath>           // For std::round, std::floor, std::fmod, std::abs
#include <juce_core/system/juce_TargetPlatform.h> // For DBG

//==============================================================================
InputHandler::InputHandler(Listener& listener,
                           const std::vector<MainComponent::ScaleInfo>& scaleData,
                           const std::atomic<int>& rootNote,
                           const std::atomic<int>& scaleType,
                           const std::atomic<int>& transpose) :
    listenerRef(listener), // Initialize listener reference
    scaleDataRef(scaleData), // Initialize references to read state from MainComponent
    rootNoteRef(rootNote),
    scaleTypeRef(scaleType),
    transposeRef(transpose)
    // currentlyPlayingNote defaults to -1, keysDown map defaults to empty
{
    // Constructor body can be empty for now
}

// Method called by MainComponent::keyPressed
void InputHandler::handleKeyPress(const juce::KeyPress& key)
{
    int keyCode = key.getKeyCode();
    // Check if key is *already* logically down according to our map
    if (keysDown.count(keyCode) && keysDown[keyCode]) {
         DBG("InputHandler::handleKeyPress: Key code " + juce::String(keyCode) + " ignored (already down).");
         return; // Prevent auto-repeat trigger by OS
    }

    // --- Map KeyCode to an index ---
    int keyIndex = keyOrder.indexOfChar((juce::juce_wchar)keyCode);
    if (keyIndex == -1) { // Key not in our defined layout
        DBG("InputHandler::handleKeyPress: Key code " + juce::String(keyCode) + " (" + key.getTextDescription() + ") - Not in keyOrder map.");
        return;
    }

    DBG("InputHandler::handleKeyPress: Key code " + juce::String(keyCode) + " (" + key.getTextDescription() + ")");

    // --- Calculate MIDI Note based on Root, Scale, and Key Index ---
    int rootNoteIndex = rootNoteRef.load(); // 0-11 (C=0)
    int scaleTypeId = scaleTypeRef.load(); // 1=Major, 2=Minor, ...
    int scalePatternIndex = scaleTypeId - 1; // Adjust for 0-based vector index

    // Validate scale data access
    if (scalePatternIndex < 0 || scalePatternIndex >= scaleDataRef.size() || scaleDataRef[scalePatternIndex].intervals.size() != 7) {
        DBG("InputHandler::handleKeyPress - Invalid scale type selected or scaleData incorrect! ScaleID=" + juce::String(scaleTypeId));
        return; // Cannot proceed
    }
    const auto& intervals = scaleDataRef[scalePatternIndex].intervals; // Get intervals {0, 2, 4...}

    // Calculate reference MIDI note for 'A' key (index 10) - Root Note in octave closest to Middle C (60)
    int refMidiNote = 12 * (int)std::round((60.0 - rootNoteIndex) / 12.0) + rootNoteIndex;

    int offset = keyIndex - refKeyIndex; // Offset in scale steps from 'A'

    // Calculate octave shift and degree index within the scale pattern
    int octaveShift = floor((double)offset / 7.0); // How many full octaves up/down
    int degreeIndex = ((offset % 7) + 7) % 7; // Index within the 7 scale intervals (0-6)

    // Get the interval (in semitones) from the root for this scale degree
    int interval = intervals[degreeIndex];

    // Calculate the final MIDI note number (Root + Octave + Interval relative to root)
    int finalMidiNote = refMidiNote + (octaveShift * 12) + (interval - intervals[0]); // intervals[0] is always 0

    // Clamp to valid MIDI range
    finalMidiNote = juce::jlimit(0, 127, finalMidiNote);

    // --- Store state and trigger listener ---
    keysDown[keyCode] = true; // Mark key down in our map NOW
    currentlyPlayingNote = finalMidiNote; // Store the MIDI note being played

    // Apply transpose *before* sending note to listener (MainComponent applies fine tune)
    int currentTranspose = transposeRef.load();
    int transposedMidiNote = juce::jlimit(0, 127, finalMidiNote + currentTranspose);

    // Tell the listener (MainComponent) to turn the note ON
    listenerRef.inputNoteOn(transposedMidiNote); // Pass the transposed note

    DBG("InputHandler::handleKeyPress - Key Mapped: Key='" + key.getTextDescription() + "', Offset=" + juce::String(offset)
        + ", OctShift=" + juce::String(octaveShift) + ", DegIdx=" + juce::String(degreeIndex)
        + ", Interval=" + juce::String(interval)
        + ", BaseMIDI=" + juce::String(finalMidiNote) // Log the note BEFORE transpose
        + ", Transpose=" + juce::String(currentTranspose)
        + ", FinalMIDI Sent=" + juce::String(transposedMidiNote) + " -> Note ON");
}

// Method called by MainComponent::keyStateChanged
void InputHandler::handleKeyStateChange()
{
    DBG("InputHandler::handleKeyStateChange called.");
    bool shouldBePlaying = false;
    int lastKeyDownCode = -1;
    int highestKeyIndex = -1; // Track index of highest priority key still down

    // Update internal map based on which keys *we care about* are physically down
    for (auto it = keysDown.begin(); it != keysDown.end(); )
    {
        int currentKeyCode = it->first;
        // Check the actual current state using JUCE's function
        if (juce::KeyPress::isKeyCurrentlyDown(currentKeyCode))
        {
            shouldBePlaying = true;
            it->second = true; // Update map state

            // Find the index of this key to determine priority (later keys = higher pitch)
            int currentKeyIndex = keyOrder.indexOfChar((juce::juce_wchar)currentKeyCode);
            if (currentKeyIndex > highestKeyIndex) {
                highestKeyIndex = currentKeyIndex;
                lastKeyDownCode = currentKeyCode; // Store the code of the highest key
            }
            ++it;
        }
        else
        {
            // If key is UP, remove it from our map of tracked keys
             DBG("InputHandler::handleKeyStateChange - Key Up detected: " + juce::String(currentKeyCode));
            it = keysDown.erase(it); // Erase returns iterator to the next element
        }
    }


    if (shouldBePlaying)
    {
        // --- Handle potential note change if multiple keys were held ---
        // Calculate the MIDI note for the highest key still held down
        int rootNoteIndex = rootNoteRef.load();
        int scaleTypeId = scaleTypeRef.load();
        int scalePatternIndex = juce::jlimit(0, (int)scaleDataRef.size() - 1, scaleTypeId - 1);

        if (scaleDataRef.empty() || scaleDataRef[scalePatternIndex].intervals.size() != 7) {
             DBG("InputHandler::handleKeyStateChange - Invalid scale data! Forcing note off.");
             if (currentlyPlayingNote != -1) {
                 listenerRef.inputNoteOff(currentlyPlayingNote); // Use last known note for note off
                 currentlyPlayingNote = -1;
             }
             return;
        }
        const auto& intervals = scaleDataRef[scalePatternIndex].intervals;
        int refMidiNote = 12 * (int)std::round((60.0 - rootNoteIndex) / 12.0) + rootNoteIndex;
        int refKeyIndex = keyOrder.indexOfChar('A');
        int offset = highestKeyIndex - refKeyIndex;
        int octaveShift = floor((double)offset / 7.0);
        int degreeIndex = ((offset % 7) + 7) % 7;
        int interval = intervals[degreeIndex];
        int newMidiNote = juce::jlimit(0, 127, refMidiNote + (octaveShift * 12) + interval);

        // Check if the highest sounding note has changed
        if (newMidiNote != currentlyPlayingNote)
        {
            // Trigger note off for the previous note first? Optional for monophonic.
            // listenerRef.inputNoteOff(currentlyPlayingNote);

            currentlyPlayingNote = newMidiNote; // Store the new note

            // Apply transpose before sending note to listener
            int currentTranspose = transposeRef.load();
            int transposedMidiNote = juce::jlimit(0, 127, currentlyPlayingNote + currentTranspose);

            listenerRef.inputNoteOn(transposedMidiNote); // Trigger new note ON

            DBG("InputHandler::handleKeyStateChange - Note Changed/Retriggered: New BaseMIDI=" + juce::String(currentlyPlayingNote)
                + ", FinalMIDI Sent=" + juce::String(transposedMidiNote) + " -> Note ON");
        }
        // If the same note is still held, do nothing - let ADSR continue
    }
    else // No relevant keys in our map are currently down
    {
        if (currentlyPlayingNote != -1) // Only trigger noteOff if a note was actually playing
        {
             DBG("InputHandler::handleKeyStateChange - All relevant keys released. Triggering Note OFF for MIDI=" + juce::String(currentlyPlayingNote));
             // Apply transpose to the note being turned off as well? Not necessary.
             // Send the base MIDI note that needs to stop.
             listenerRef.inputNoteOff(currentlyPlayingNote); // <<< Trigger Note OFF via Listener >>>
             currentlyPlayingNote = -1; // Mark no note as playing
        }
    }
}