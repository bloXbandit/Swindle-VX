#include "PitchDetector.h"
#include <cmath>
#include <algorithm>

namespace blink {

PitchDetector::PitchDetector(double sr, int bs) : sampleRate(sr), bufferSize(bs) {
    yinBuffer.resize(bufferSize / 2);
}

float PitchDetector::getPitch(const float* buffer, int numSamples) {
    if (buffer == nullptr || numSamples <= 0)
        return 0.0f;

    const int analysisSize = std::min(bufferSize, numSamples);
    const int halfSize = analysisSize / 2;
    if (halfSize < 2)
        return 0.0f;

    if ((int) yinBuffer.size() != halfSize)
        yinBuffer.resize(halfSize);

    int tauEstimate = -1;

    difference(buffer, analysisSize);
    cumulativeMeanNormalizedDifference();
    tauEstimate = absoluteThreshold();

    if (tauEstimate != -1) {
        float betterTau = parabolicInterpolation(tauEstimate);
        return (float)sampleRate / betterTau;
    }

    return 0.0f;
}

void PitchDetector::difference(const float* buffer, int analysisSize) {
    const int halfSize = analysisSize / 2;
    for (int tau = 0; tau < halfSize; tau++) {
        yinBuffer[tau] = 0;
        for (int i = 0; i < halfSize; i++) {
            float delta = buffer[i] - buffer[i + tau];
            yinBuffer[tau] += delta * delta;
        }
    }
}

void PitchDetector::cumulativeMeanNormalizedDifference() {
    yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < yinBuffer.size(); tau++) {
        runningSum += yinBuffer[tau];
        if (runningSum <= 0.0f) {
            yinBuffer[tau] = 1.0f;
            continue;
        }
        yinBuffer[tau] *= (float)tau / runningSum;
    }
}

int PitchDetector::absoluteThreshold() {
    int tau;
    // Find the first value under the threshold
    for (tau = 2; tau < yinBuffer.size(); tau++) {
        if (yinBuffer[tau] < threshold) {
            // Find the local minimum
            while (tau + 1 < yinBuffer.size() && yinBuffer[tau + 1] < yinBuffer[tau]) {
                tau++;
            }
            return tau;
        }
    }
    // If nothing under threshold, return global minimum
    return -1;
}

float PitchDetector::parabolicInterpolation(int tauEstimate) {
    if (tauEstimate < 1 || tauEstimate >= yinBuffer.size() - 1) 
        return (float)tauEstimate;

    float s0 = yinBuffer[tauEstimate - 1];
    float s1 = yinBuffer[tauEstimate];
    float s2 = yinBuffer[tauEstimate + 1];

    return (float)tauEstimate + (s2 - s0) / (2.0f * (2.0f * s1 - s2 - s0));
}

} // namespace blink
