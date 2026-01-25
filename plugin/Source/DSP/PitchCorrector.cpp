#include "PitchCorrector.h"
#include <algorithm>
#include <cmath>

namespace blink {

PitchCorrector::PitchCorrector() 
    : rootKey(0), scaleType(ScaleType::Major), targetFreq(0.0f), smoothedTarget(0.0f) {
    // Initialize with chromatic scale (all notes active)
    for (int i = 0; i < 12; i++) {
        activeNotes[i] = true;
    }
}

float PitchCorrector::correctPitch(float detectedFreq, float correctionAmount, float speed) {
    if (detectedFreq <= 0.0f) {
        return detectedFreq;
    }
    
    // Convert to MIDI note number
    float midiNote = hzToMidi(detectedFreq);
    
    // Find nearest note in the active scale
    int targetNote = findNearestScaleNote(midiNote);
    
    // Calculate target frequency
    targetFreq = midiToHz((float)targetNote);
    
    // Apply correction amount (0.0 = no correction, 1.0 = full correction)
    float correctedMidi = midiNote + (targetNote - midiNote) * correctionAmount;
    float correctedFreq = midiToHz(correctedMidi);
    
    // Apply smoothing based on speed parameter
    // speed: 0.0 = instant, 1.0 = very slow
    float smoothing = 1.0f - speed;
    smoothedTarget = smoothedTarget * (1.0f - smoothing) + correctedFreq * smoothing;
    
    return smoothedTarget;
}

void PitchCorrector::setKey(int rootKey) {
    this->rootKey = rootKey % 12;
    
    // Update active notes based on current scale
    std::vector<int> intervals = getScaleIntervals(scaleType);
    for (int i = 0; i < 12; i++) {
        activeNotes[i] = false;
    }
    for (int interval : intervals) {
        activeNotes[(rootKey + interval) % 12] = true;
    }
}

void PitchCorrector::setScale(ScaleType scale) {
    this->scaleType = scale;
    setKey(rootKey); // Rebuild active notes
}

void PitchCorrector::setActiveNotes(const bool notes[12]) {
    for (int i = 0; i < 12; i++) {
        activeNotes[i] = notes[i];
    }
}

std::vector<int> PitchCorrector::getScaleIntervals(ScaleType scale) const {
    switch (scale) {
        case ScaleType::Major:
            return {0, 2, 4, 5, 7, 9, 11};
        case ScaleType::Minor:
            return {0, 2, 3, 5, 7, 8, 10};
        case ScaleType::HarmonicMinor:
            return {0, 2, 3, 5, 7, 8, 11};
        case ScaleType::MelodicMinor:
            return {0, 2, 3, 5, 7, 9, 11};
        case ScaleType::Dorian:
            return {0, 2, 3, 5, 7, 9, 10};
        case ScaleType::Phrygian:
            return {0, 1, 3, 5, 7, 8, 10};
        case ScaleType::Lydian:
            return {0, 2, 4, 6, 7, 9, 11};
        case ScaleType::Mixolydian:
            return {0, 2, 4, 5, 7, 9, 10};
        case ScaleType::Chromatic:
            return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        default:
            return {0, 2, 4, 5, 7, 9, 11}; // Default to Major
    }
}

int PitchCorrector::findNearestScaleNote(float midiNote) const {
    int roundedNote = (int)std::round(midiNote);
    int noteClass = roundedNote % 12;
    
    // If the detected note is already in the scale, return it
    if (activeNotes[noteClass]) {
        return roundedNote;
    }
    
    // Otherwise, find the nearest active note
    int minDistance = 12;
    int closestNote = roundedNote;
    
    for (int distance = 1; distance < 12; distance++) {
        // Check above
        int noteAbove = (noteClass + distance) % 12;
        if (activeNotes[noteAbove]) {
            closestNote = roundedNote + distance;
            minDistance = distance;
            break;
        }
        
        // Check below
        int noteBelow = (noteClass - distance + 12) % 12;
        if (activeNotes[noteBelow]) {
            closestNote = roundedNote - distance;
            minDistance = distance;
            break;
        }
    }
    
    return closestNote;
}

float PitchCorrector::hzToMidi(float hz) const {
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

float PitchCorrector::midiToHz(float midi) const {
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

} // namespace blink
