#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>
#include <atomic>
#include "MainComponent.h" // Include MainComponent header for ScaleInfo struct and enums

//==============================================================================
/*
    Handles mapping computer keyboard input to MIDI notes based on selected
    scale, root, and tuning parameters. Communicates note on/off events
    back via a Listener interface.
*/
class InputHandler
{
public:
    // Listener interface for communication back to the owner (MainComponent)
    class Listener
    {
    public:
        virtual ~Listener() = default;
        // Called when a key press maps to a valid note that should start playing
        virtual void inputNoteOn(int midiNoteNumber) = 0;
        // Called when a key release means the currently playing note should stop
        virtual void inputNoteOff(int midiNoteNumber) = 0;
         // Or potentially just inputNoteOff() if monophonic? Let's include note number for future.
    };

    // Constructor needs access to scale data, relevant state atomics, and the listener
    InputHandler(Listener& listener,
                 const std::vector<MainComponent::ScaleInfo>& scaleData, // Read-only access to scale definitions
                 const std::atomic<int>& rootNote,         // Read-only access to atomics
                 const std::atomic<int>& scaleType,
                 const std::atomic<int>& transpose);       // Only need transpose here, fine tune applied later

    // Methods called by MainComponent's KeyListener callbacks
    void handleKeyPress(const juce::KeyPress& key);
    void handleKeyStateChange(); // Called when *any* key state changes


private:
    Listener& listenerRef; // Reference to the listener (MainComponent)

    // References to read state from MainComponent
    const std::vector<MainComponent::ScaleInfo>& scaleDataRef;
    const std::atomic<int>& rootNoteRef;
    const std::atomic<int>& scaleTypeRef;
    const std::atomic<int>& transposeRef;

    // Internal state for tracking pressed keys and the active note
    std::map<int, bool> keysDown; // Map of keyCode -> isLogicallyDown
    int                 currentlyPlayingNote = -1; // Base MIDI note (before fine tune)

    // Key layout information
    const juce::String keyOrder = "QWERTYUIOPASDFGHJKLZXCVBNM";
    const int refKeyIndex = keyOrder.indexOfChar('A'); // Index of 'A'
};