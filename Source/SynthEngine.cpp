#include "SynthEngine.h"
#include "MainComponent.h" // For Waveform enum access
#include <cmath>   
#include <JuceHeader.h> // For std::sin, std::fmod, std::abs, std::pow

//==============================================================================
SynthEngine::SynthEngine()
{
    // Set default ADSR parameters when the engine is created
    adsrParams.attack = 0.05f;
    adsrParams.decay = 0.1f;
    adsrParams.sustain = 0.8f;
    adsrParams.release = 0.5f;
    // Initial filter params will be set in prepareToPlay
}
double SynthEngine::getCurrentFrequency() const
{
    // Returns the frequency currently being used by the oscillator
    return frequency; // 'frequency' is a member variable of SynthEngine
}
// --- UPDATE prepareToPlay ---
// Now accepts block size and num channels for ProcessSpec
// --- REPLACE prepareToPlay function ---
// --- REPLACE prepareToPlay function ---
void SynthEngine::prepareToPlay(double sampleRate, int maximumBlockSize, int /*numChannels*/) // numChannels passed in is now ignored
{
    currentSampleRate = sampleRate;
    currentAngle = 0.0;
    angleDelta = 0.0;
    frequency = 440.0;

    // --- Prepare Filter ---
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)juce::jmax(1, maximumBlockSize);
    spec.numChannels = 1; // <<< FORCE to 1 Channel for mono processing >>>

    filter.prepare(spec); // Prepare the filter with the MONO spec
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // Set initial values (read from atomics passed via MainComponent::updateFilter maybe?)
    // For now, re-apply reasonable defaults after prepare
    float initialCutoff = 10000.0f; // Start wide open
    float initialResonance = 1.0f / std::sqrt(2.0f);
    filter.setCutoffFrequency(initialCutoff);
    filter.setResonance(initialResonance);

    // Log initial state
    DBG("SynthEngine::prepareToPlay - Spec: Rate=" + juce::String(spec.sampleRate)
        + ", BlockSize=" + juce::String(spec.maximumBlockSize)
        + ", Channels=" + juce::String(spec.numChannels)); // Will now log 1 channel
    DBG("SynthEngine::prepareToPlay - Initial Filter Cutoff set to: " + juce::String(filter.getCutoffFrequency()));
    DBG("SynthEngine::prepareToPlay - Initial Filter Resonance set to: " + juce::String(filter.getResonance()));

    // --- Prepare ADSR ---
    adsr.setSampleRate(sampleRate);
    adsr.setParameters(adsrParams);
    adsr.reset();
    filter.reset(); // <<< ADD THIS LINE to clear internal filter state
}

void SynthEngine::setParameters(const juce::ADSR::Parameters& params)
{
    // Update local copy and apply to ADSR object
    adsrParams = params;
    adsr.setParameters(adsrParams);
}

void SynthEngine::setWaveform(int waveformTypeId)
{
    currentWaveformType.store(waveformTypeId);
}

void SynthEngine::setFrequency(double frequencyHz)
{
    frequency = frequencyHz; // Store the frequency
    if (currentSampleRate > 0.0) {
        double cyclesPerSample = frequency / currentSampleRate;
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }
    else {
        angleDelta = 0.0;
    }
}

// --- ADD setFilterParameters ---
// --- REPLACE setFilterParameters function ---
void SynthEngine::setFilterParameters(float cutoffHz, float resonance)
{
    // Clamp values to ensure they are safe for the filter
    // Cutoff: Limit between ~20Hz and slightly below Nyquist frequency
    float clampedCutoff = juce::jlimit(20.0f, (float)(currentSampleRate / 2.0 * 0.98), cutoffHz);
    // Resonance: Limit between reasonable minimum (e.g., 0.1 or sqrt(0.5)) and a max (e.g., 18)
    float clampedRes = juce::jlimit(0.707f, 18.0f, resonance); // Using slider range min/max basically

    DBG("SynthEngine::setFilterParameters called. Input C=" + juce::String(cutoffHz) + ", R=" + juce::String(resonance)
        + " | Clamped C=" + juce::String(clampedCutoff) + ", R=" + juce::String(clampedRes));

    filter.setCutoffFrequency(clampedCutoff);
    filter.setResonance(clampedRes);
}


void SynthEngine::noteOn()
{
    adsr.noteOn();
}

void SynthEngine::noteOff()
{
    adsr.noteOff();
}

bool SynthEngine::isActive() const
{
    return adsr.isActive();
}


// --- UPDATE renderNextBlock ---
// --- REPLACE renderNextBlock function ---
// --- REPLACE renderNextBlock function (Back to Sample-by-Sample) ---
void SynthEngine::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{   
 
    // If ADSR is completely finished, ensure output is silent and exit
    if (!adsr.isActive())
    {
        outputBuffer.clear(startSample, numSamples);
        return; // Exit early
    }

    // --- If ADSR is active ---

    // Calculate the angle delta based on the current stored frequency
    double currentBlockAngleDelta = 0.0;
    if (currentSampleRate > 0.0 && frequency > 0.0) // Use the 'frequency' member variable
    {
        currentBlockAngleDelta = (frequency / currentSampleRate) * 2.0 * juce::MathConstants<double>::pi;
    }

    // Get waveform type
    auto waveTypeInt = currentWaveformType.load(); // Read atomic waveform type as int
    auto* leftBuffer = outputBuffer.getWritePointer(0, startSample);
    auto* rightBuffer = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1, startSample) : nullptr;

    // Process sample by sample
    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Get the ADSR gain value for this sample (advances ADSR state)
        float envelopeGain = adsr.getNextSample();

        // 2. Calculate raw oscillator value
        double currentSampleValue = 0.0;
        // Only calculate if gain > 0 and delta is valid (slight optimization, but run filter anyway)
        // Let's calculate oscillator regardless of gain first, simpler.
        if (currentBlockAngleDelta != 0.0)
        {
            double phase = std::fmod(currentAngle, 2.0 * juce::MathConstants<double>::pi) / (2.0 * juce::MathConstants<double>::pi);
            // Use integer cases for the switch
            switch (waveTypeInt)
            {
            case 1: currentSampleValue = std::sin(currentAngle); break;
            case 2: currentSampleValue = (phase < 0.5) ? 1.0 : -1.0; break;
            case 3: currentSampleValue = (2.0 * phase) - 1.0; break;
            case 4: currentSampleValue = 2.0 * (1.0 - 2.0 * std::abs(phase - 0.5)) - 1.0; break;
            default: currentSampleValue = std::sin(currentAngle); break;
            }
            // Increment oscillator phase angle using the calculated delta
            currentAngle += currentBlockAngleDelta;
        }
        // If delta was 0, currentSampleValue remains 0.0

        // 3. Apply Filter (process sample - treating input as mono for now)
        float filteredSample = filter.processSample(0, (float)currentSampleValue);

        // Log Filter I/O (for first sample only)
        if (i == 0) {
            DBG("Filter I/O [Sample 0]: In=" + juce::String(currentSampleValue, 4)
                + ", Out=" + juce::String(filteredSample, 4)
                + ", Cutoff=" + juce::String(filter.getCutoffFrequency())
                + ", Res=" + juce::String(filter.getResonance())
                + ", Type=" + juce::String((int)filter.getType()));
        }

        // 4. Calculate final sample value: FilteredOsc * Envelope Gain
        // Master Level is applied later in MainComponent::getNextAudioBlock
        float outputSample = filteredSample * envelopeGain;

        // 5. Write to output buffers
        leftBuffer[i] = outputSample;
        if (rightBuffer != nullptr)
            rightBuffer[i] = outputSample; // Write same mono signal to right channel
    }

    // Optional: Wrap main phase angle
    if (currentAngle >= 2.0 * juce::MathConstants<double>::pi)
        currentAngle -= 2.0 * juce::MathConstants<double>::pi;

    // Note: Oscilloscope copy and Master Level are handled in MainComponent::getNextAudioBlock
}