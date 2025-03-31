#include "OscilloscopeComponent.h"
#include <juce_core/system/juce_TargetPlatform.h> // For DBG

//==============================================================================
OscilloscopeComponent::OscilloscopeComponent()
{
    displayBuffer.setSize(1, bufferSize);
    displayBuffer.clear();
    startTimerHz(30);
}

OscilloscopeComponent::~OscilloscopeComponent() // No override needed on definition
{
    stopTimer();
}

// --- UPDATE paint function ---
void OscilloscopeComponent::paint(juce::Graphics& g) // No override needed on definition
{
    // 1. Fill background
    g.fillAll(juce::Colours::black);

    // 2. Set drawing color for the waveform
    g.setColour(juce::Colours::limegreen);

    // 3. Draw Waveform Path
    juce::Path waveformPath;
    const juce::SpinLock::ScopedLockType lock(bufferLock); // Lock buffer access

    // DBG logs can remain or be commented out
    // DBG("OscilloscopeComponent::paint - Width=" + juce::String(getWidth()) + ", Height=" + juce::String(getHeight()));
    const float* bufferData = displayBuffer.getReadPointer(0);
    int numSamples = displayBuffer.getNumSamples();
    // if (numSamples > 0) { DBG("  First sample in displayBuffer: " + juce::String(bufferData[0])); }

    float width = (float)getWidth();
    float height = (float)getHeight();
    float midY = height / 2.0f;

    if (numSamples > 0 && width > 0 && height > 0)
    {
        waveformPath.startNewSubPath(0.0f, midY - (bufferData[0] * midY));
        for (int i = 1; i < numSamples; ++i)
        {
            float x = (width * i) / (float)(numSamples - 1);
            float y = midY - (bufferData[i] * midY);
            waveformPath.lineTo(x, y);
        }
    }
    // --- lock automatically released here ---
    g.strokePath(waveformPath, juce::PathStrokeType(1.0f));


    // --- 4. Draw Frequency Text (NEW / UPDATED) ---
    g.setColour(juce::Colours::limegreen); // Ensure color is green
    // Set to a common monospace font
   // Use these lines instead:
// Try this if the explicit String didn't work:
    g.setFont(juce::Font("Consolas", 14.0f, juce::Font::plain)); // Add explicit style flag
    // Or specific: g.setFont(juce::Font ("Consolas", 14.0f));

    // Format frequency string (display 2 decimal places) only if > 0 Hz
    juce::String freqText = (frequencyHz > 0.0f) ? (juce::String(frequencyHz, 2) + " Hz") : "--- Hz";

    int textMargin = 5;
    int textWidth = 150; // Approx width needed
    int textHeight = 20; // Approx height needed
    juce::Rectangle<int> textArea(textMargin, textMargin, textWidth, textHeight);

    // Draw the text in the top-left corner
    g.drawText(freqText, textArea, juce::Justification::topLeft, false); // Use false for single line
}

void OscilloscopeComponent::resized() // No override needed on definition
{
    // Nothing needed here for now
}

// --- UPDATE copySamples function ---
// Definition now matches header, accepting and storing freqHz
void OscilloscopeComponent::copySamples(const float* sourceSamples, int numSourceSamples, float freqHz) // <-- ADD freqHz parameter
{
    // Store the frequency passed in
    // Do this even if sourceSamples is null, so it displays 0 Hz or last known freq correctly
    // Let's update only if sourceSamples is valid, otherwise keep last non-zero value?
    // Or simpler: always update, show 0.0 Hz when silent/inactive.
    frequencyHz = freqHz; // <-- STORE frequency passed from MainComponent

    // If no valid samples, just clear internal buffer (optional, copy logic handles it)
    if (sourceSamples == nullptr || numSourceSamples <= 0)
    {
        // Clear display buffer if no real data came in? Or just let old data persist?
        // Let's clear it for now if no samples are valid.
        const juce::SpinLock::ScopedLockType lock(bufferLock);
        displayBuffer.clear();
        return;
    }

    // Safely write sample data to the displayBuffer
    const juce::SpinLock::ScopedLockType lock(bufferLock);

    int samplesToCopy = juce::jmin(numSourceSamples, bufferSize);

    // DBG log now includes frequency
    if (samplesToCopy > 0) {
        DBG("OscilloscopeComponent::copySamples - First sample received: " + juce::String(sourceSamples[0]) + ", Freq: " + juce::String(freqHz));
    }

    float* dest = displayBuffer.getWritePointer(0);
    // Copy the samples
    juce::FloatVectorOperations::copy(dest, sourceSamples, samplesToCopy);

    // If we received fewer samples than our buffer size, clear the remainder
    if (samplesToCopy < bufferSize)
    {
        displayBuffer.clear(0, samplesToCopy, bufferSize - samplesToCopy);
    }
}

void OscilloscopeComponent::timerCallback() // No override needed on definition
{
    repaint(); // Trigger repaint periodically
}