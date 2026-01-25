#include "LPCAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace blink {

LPCAnalyzer::LPCAnalyzer(int order)
    : order(order), predictionError(0.0f) {
    lpcCoeffs.resize(order + 1, 0.0f);
    autocorr.resize(order + 1, 0.0f);
}

void LPCAnalyzer::calculateAutocorrelation(const float* buffer, int numSamples) {
    // Calculate autocorrelation for lags 0 to order
    for (int lag = 0; lag <= order; lag++) {
        float sum = 0.0f;
        for (int i = 0; i < numSamples - lag; i++) {
            sum += buffer[i] * buffer[i + lag];
        }
        autocorr[lag] = sum;
    }
}

void LPCAnalyzer::levinsonDurbin(const std::vector<float>& r) {
    // Levinson-Durbin recursion algorithm
    // Solves the Toeplitz system to find LPC coefficients
    
    if (r[0] == 0.0f) {
        // Avoid division by zero
        std::fill(lpcCoeffs.begin(), lpcCoeffs.end(), 0.0f);
        predictionError = 0.0f;
        return;
    }
    
    std::vector<float> a(order + 1, 0.0f);
    std::vector<float> aPrev(order + 1, 0.0f);
    
    // Initialize
    float error = r[0];
    
    // Recursion
    for (int i = 1; i <= order; i++) {
        // Calculate reflection coefficient
        float lambda = r[i];
        for (int j = 1; j < i; j++) {
            lambda -= a[j] * r[i - j];
        }
        float k = lambda / error;
        
        // Update coefficients
        a[i] = k;
        for (int j = 1; j < i; j++) {
            aPrev[j] = a[j];
        }
        for (int j = 1; j < i; j++) {
            a[j] = aPrev[j] - k * aPrev[i - j];
        }
        
        // Update error
        error *= (1.0f - k * k);
    }
    
    // Copy to output
    lpcCoeffs[0] = 1.0f;
    for (int i = 1; i <= order; i++) {
        lpcCoeffs[i] = -a[i];  // Note: negative for filter form
    }
    
    predictionError = error;
}

bool LPCAnalyzer::analyze(const float* buffer, int numSamples) {
    if (numSamples < order + 1) {
        return false;
    }
    
    // Step 1: Calculate autocorrelation
    calculateAutocorrelation(buffer, numSamples);
    
    // Step 2: Apply Levinson-Durbin recursion
    levinsonDurbin(autocorr);
    
    return true;
}

void LPCAnalyzer::getSpectralEnvelope(const float* frequencies, int numFreqs,
                                     float* envelope, float sampleRate) {
    // Evaluate LPC filter frequency response
    // H(z) = 1 / (1 + a1*z^-1 + a2*z^-2 + ... + ap*z^-p)
    
    for (int i = 0; i < numFreqs; i++) {
        float freq = frequencies[i];
        float omega = 2.0f * M_PI * freq / sampleRate;
        
        // Calculate denominator: sum of a[k] * e^(-j*omega*k)
        std::complex<float> denom(1.0f, 0.0f);
        for (int k = 1; k <= order; k++) {
            float phase = -omega * k;
            std::complex<float> expTerm(cosf(phase), sinf(phase));
            denom += lpcCoeffs[k] * expTerm;
        }
        
        // Magnitude of frequency response
        envelope[i] = 1.0f / std::abs(denom);
    }
}

} // namespace blink
