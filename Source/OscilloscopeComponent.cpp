#include "OscilloscopeComponent.h"
#include <juce_core/system/juce_TargetPlatform.h> // Ensure DBG is available

//==============================================================================
OscilloscopeComponent::OscilloscopeComponent()
{
    // Initialize the display buffer: 1 channel, bufferSize samples
    displayBuffer.setSize(1, bufferSize);
    displayBuffer.clear(); // Start with silence

    // Start the timer: Aim for roughly 30 repaints per second (adjust as needed)
    startTimerHz(30);
}

OscilloscopeComponent::~OscilloscopeComponent() /*override*/ // Override on destructor definition is optional/often omitted
{
    // Stop the timer when the component is deleted to prevent callbacks
    stopTimer();
}

// This is where the drawing happens (on the message thread)
void OscilloscopeComponent::paint(juce::Graphics& g) /*override*/ // Ideally has override if header does
{
    // 1. Fill background
    g.fillAll(juce::Colours::black);

    // 2. Set drawing color for the waveform
    g.setColour(juce::Colours::limegreen);

    // 3. Create a Path object to store the waveform shape
    juce::Path waveformPath;

    // 4. Safely read data from the displayBuffer
    // Use ScopedLockType for automatic locking/unlocking
    const juce::SpinLock::ScopedLockType lock(bufferLock);

    // --- ADDED DBG LOGS ---
    DBG("OscilloscopeComponent::paint - Width=" + juce::String(getWidth()) + ", Height=" + juce::String(getHeight()));
    if (displayBuffer.getNumSamples() > 0) {
        DBG("  First sample in displayBuffer: " + juce::String(displayBuffer.getReadPointer(0)[0]));
    }
    // --- END DBG LOGS ---

    const float* bufferData = displayBuffer.getReadPointer(0); // Pointer to samples
    int numSamples = displayBuffer.getNumSamples();            // Should be bufferSize (512)
    float width = (float)getWidth();                           // Component width
    float height = (float)getHeight();                         // Component height
    float midY = height / 2.0f;                                // Vertical center

    if (numSamples > 0 && width > 0 && height > 0) // Check if we have data and space
    {
        // Move path to the starting point (first sample)
        float x = 0.0f;
        // Map sample value (-1 to 1) to vertical pixel coordinate
        float y = midY - (bufferData[0] * midY); // Assumes bufferData values are roughly -1 to 1
        waveformPath.startNewSubPath(x, y);

        // Add lines connecting subsequent samples
        for (int i = 1; i < numSamples; ++i)
        {
            // Map sample index to horizontal pixel coordinate
            x = (width * i) / (float)(numSamples - 1);
            // Map sample value to vertical pixel coordinate
            y = midY - (bufferData[i] * midY);
            waveformPath.lineTo(x, y);
        }
    }
    // --- bufferLock is automatically released here when 'lock' goes out of scope ---

    // 5. Draw the completed path
    // Use a stroke thickness of 1 pixel for a simple line
    g.strokePath(waveformPath, juce::PathStrokeType(1.0f));
}

void OscilloscopeComponent::resized() /*override*/ // Ideally has override if header does
{
    // Called when the component's size changes.
}

// This function is called from the audio thread (`MainComponent::getNextAudioBlock`)
void OscilloscopeComponent::copySamples(const float* sourceSamples, int numSourceSamples)
{
    if (sourceSamples == nullptr || numSourceSamples <= 0)
        return;

    // Safely write data to the displayBuffer
    const juce::SpinLock::ScopedLockType lock(bufferLock);

    int samplesToCopy = juce::jmin(numSourceSamples, bufferSize);

    // --- ADDED DBG LOG ---
    if (samplesToCopy > 0) {
        DBG("OscilloscopeComponent::copySamples - First sample received: " + juce::String(sourceSamples[0]));
    }
    // --- END DBG LOG ---

    // Get a write pointer to our internal buffer
    float* dest = displayBuffer.getWritePointer(0);

    // Copy the samples
    juce::FloatVectorOperations::copy(dest,           // Destination
        sourceSamples,  // Source
        samplesToCopy); // Number of samples

    // If we received fewer samples than our buffer size, clear the remainder
    if (samplesToCopy < bufferSize)
    {
        displayBuffer.clear(0, samplesToCopy, bufferSize - samplesToCopy);
    }
    // --- bufferLock automatically released ---
}

// This function is called periodically by the Timer (on the message thread)
void OscilloscopeComponent::timerCallback() /*override*/ // Ideally has override if header does
{
    // Trigger an asynchronous repaint of the component.
    repaint();
}