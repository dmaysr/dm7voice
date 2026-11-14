#pragma once
// SM7B-style voicing: a chain of biquads modelled on the published response of a
// large-diaphragm broadcast dynamic mic (flat setting, plus its two rear switches),
// with a continuous proximity-effect control.

#include <array>
#include <cmath>
#include <complex>
#include <algorithm>

namespace dm7
{
constexpr double kPi = 3.14159265358979323846;

struct Coeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    double magnitudeDb (double freq, double fs) const
    {
        return 20.0 * std::log10 (std::abs (response (freq, fs)) + 1e-12);
    }

    std::complex<double> response (double freq, double fs) const
    {
        const double w = 2.0 * kPi * freq / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

namespace design
{
    inline double clampF (double f, double fs) { return std::clamp (f, 10.0, fs * 0.45); }

    inline Coeffs norm (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    inline Coeffs peak (double fs, double f, double q, double gainDb)
    {
        f = clampF (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
        const double c = std::cos (w), alpha = std::sin (w) / (2 * q);
        return norm (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    inline Coeffs lowShelf (double fs, double f, double slope, double gainDb)
    {
        f = clampF (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
        const double c = std::cos (w), s = std::sin (w);
        const double alpha = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double k = 2 * std::sqrt (A) * alpha;
        return norm (A * ((A + 1) - (A - 1) * c + k), 2 * A * ((A - 1) - (A + 1) * c),
                     A * ((A + 1) - (A - 1) * c - k), (A + 1) + (A - 1) * c + k,
                     -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - k);
    }

    inline Coeffs highShelf (double fs, double f, double slope, double gainDb)
    {
        f = clampF (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
        const double c = std::cos (w), s = std::sin (w);
        const double alpha = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double k = 2 * std::sqrt (A) * alpha;
        return norm (A * ((A + 1) + (A - 1) * c + k), -2 * A * ((A - 1) + (A + 1) * c),
                     A * ((A + 1) + (A - 1) * c - k), (A + 1) - (A - 1) * c + k,
                     2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - k);
    }

    inline Coeffs lowPass (double fs, double f, double q)
    {
        f = clampF (f, fs);
        const double w = 2 * kPi * f / fs, c = std::cos (w), alpha = std::sin (w) / (2 * q);
        return norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    inline Coeffs highPass (double fs, double f, double q)
    {
        f = clampF (f, fs);
        const double w = 2 * kPi * f / fs, c = std::cos (w), alpha = std::sin (w) / (2 * q);
        return norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }
} // namespace design

struct VoiceSettings
{
    double proximity = 0.4;  // 0..1  (0 = ~12 in away, 1 = lips on the grille)
    double presence  = 0.0;  // 0..1  (presence-boost switch, smoothed)
    double bassCut   = 0.0;  // 0..1  (bass roll-off switch, smoothed)
};

constexpr int numStages = 9;
using Chain = std::array<Coeffs, numStages>;

// The voicing itself. Values were set against the SM7B's published frequency-response
// chart: a smooth, flat midrange, a gentle broad lift around 4-6 kHz, a soft top that
// falls away above ~9 kHz, low end down by 50 Hz, and the two switch curves.
inline Chain designChain (const VoiceSettings& s, double fs)
{
    using namespace design;
    Chain c;
    // capsule low-frequency limit
    c[0] = highPass (fs, 48.0, 0.70);
    // proximity effect: up to +9 dB of low shelf as you close in
    c[1] = lowShelf (fs, 160.0, 0.75, 9.0 * s.proximity);
    // the "thick" lower-mid body dynamics are known for
    c[2] = peak (fs, 220.0, 0.75, 1.5);
    // slight boxiness scoop that keeps the low-mids from getting muddy
    c[3] = peak (fs, 650.0, 1.1, -1.0);
    // inherent broad presence lift of the flat setting
    c[4] = peak (fs, 4200.0, 0.9, 2.0);
    // smooth, dark top: shelf down from ~8.5 kHz ...
    c[5] = highShelf (fs, 8500.0, 0.7, -5.0);
    // ... and a gentle extreme-top roll-off (diaphragm mass)
    c[6] = lowPass (fs, 15500.0, 0.6);
    // presence-boost switch: broad +5.5 dB rise centred near 5 kHz
    c[7] = peak (fs, 5000.0, 0.75, 5.5 * s.presence);
    // bass roll-off switch: low shelf pulling 100 Hz down ~8 dB
    c[8] = lowShelf (fs, 320.0, 0.6, -9.0 * s.bassCut);
    return c;
}

// Total response of the wet chain, blended with the dry signal by mix (0..1)
inline double chainMagnitudeDb (const Chain& chain, double freq, double fs, double mix)
{
    std::complex<double> h (1.0, 0.0);
    for (auto& c : chain)
        h *= c.response (freq, fs);
    const auto total = mix * h + (1.0 - mix);
    return 20.0 * std::log10 (std::abs (total) + 1e-12);
}

struct Biquad
{
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x, const Coeffs& c)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};
} // namespace dm7
