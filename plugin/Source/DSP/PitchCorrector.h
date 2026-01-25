#pragma once

#include <vector>
#include <string>
#include <cmath>

namespace blink {

/**
 * Pitch correction module that quantizes detected pitch to musical scales.
 * Supports all 12 keys and common scale types (Major, Minor, etc.)
 */
class PitchCorrector {
public:
    enum class ScaleType {
        Major,
        Minor,
        HarmonicMinor,
        MelodicMinor,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Chromatic
    };

    PitchCorrector();
    
    /**
     * Quantize a detected frequency to the nearest note in the active scale.
     * @param detectedFreq The input frequency in Hz
     * @param correctionAmount How much to correct (0.0 = none, 1.0 = full)
     * @param speed Smoothing factor for correction (0.0 = instant, 1.0 = very slow)
     * @return The corrected frequency in Hz
     */
    float correctPitch(float detectedFreq, float correctionAmount, float speed);
    
    /**
     * Set the root key (0 = C, 1 = C#, ..., 11 = B)
     */
    void setKey(int rootKey);
    
    /**
     * Set the scale type
     */
    void setScale(ScaleType scale);
    
    /**
     * Set which notes are active (for custom scales)
     * @param activeNotes Array of 12 bools, one for each chromatic note
     */
    void setActiveNotes(const bool activeNotes[12]);
    
    /**
     * Get the target pitch (for visualization)
     */
    float getTargetPitch() const { return targetFreq; }

private:
    int rootKey;
    ScaleType scaleType;
    bool activeNotes[12];
    
    float targetFreq;
    float smoothedTarget;
    
    // Scale definitions (intervals from root)
    std::vector<int> getScaleIntervals(ScaleType scale) const;
    
    // Find nearest note in active scale
    int findNearestScaleNote(float midiNote) const;
    
    // Convert between Hz and MIDI note number
    float hzToMidi(float hz) const;
    float midiToHz(float midi) const;
};

} // namespace blink
