#pragma once

// Smadder DSP engine. Plain C++ (no JUCE) so it can be tested on its own.
// Chain: Drive -> Crush -> Filter (+LFO) -> Tape wow/flutter -> Noise -> Delay -> Dry/Wet -> Output

#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace smadder
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

struct Settings
{
    float drive = 0.0f;        // 0..1
    float bits = 16.0f;        // 1..16
    float downsample = 1.0f;   // 1..50
    int   filterType = 0;      // 0 off, 1 LP, 2 HP, 3 BP
    float cutoff = 20000.0f;   // Hz
    float reso = 0.1f;         // 0..1
    float lfoRate = 1.0f;      // Hz
    float lfoDepth = 0.0f;     // 0..1 (0..4 octaves)
    float wow = 0.0f;          // 0..1
    float noise = 0.0f;        // 0..1
    float delaySeconds = 0.25f;
    float feedback = 0.3f;     // 0..0.95
    float delayTone = 6000.0f; // Hz, lowpass in feedback
    float delayMix = 0.0f;     // 0..1
    bool  pingPong = false;
    float mix = 1.0f;          // 0..1
    float outputDb = 0.0f;
};

// One-pole smoother
struct Smooth
{
    float current = 0.0f, target = 0.0f, coeff = 0.0f;
    void prepare (double sr, float timeSec, float initial)
    {
        coeff = std::exp (-1.0f / (float) (timeSec * sr));
        current = target = initial;
    }
    inline float next() { current = target + coeff * (current - target); return current; }
};

// Zero-delay-feedback state variable filter (Cytomic / Simper)
struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
    void reset() { ic1 = ic2 = 0.0f; }
    void set (float cutoff, float q, float sr)
    {
        const float g = std::tan (kPi * std::min (cutoff, 0.49f * sr) / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // returns lp, bp, hp through refs
    inline void tick (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

struct DelayLine
{
    std::vector<float> buf;
    int writePos = 0, mask = 0;
    void prepare (int minSize)
    {
        int size = 1;
        while (size < minSize) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }
    inline void push (float x) { buf[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
    // delay in samples, 0 = most recently pushed sample
    inline float read (float delay) const
    {
        const float pos = (float) (writePos - 1) - delay;
        const int i0 = (int) std::floor (pos);
        const float frac = pos - (float) i0;
        const float s0 = buf[(size_t) (i0 & mask)];
        const float s1 = buf[(size_t) ((i0 + 1) & mask)];
        return s0 + frac * (s1 - s0);
    }
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        driveS.prepare (sampleRate, 0.02f, 0.0f);
        cutoffS.prepare (sampleRate, 0.03f, std::log2 (20000.0f));
        resoS.prepare (sampleRate, 0.03f, 0.1f);
        wowS.prepare (sampleRate, 0.05f, 0.0f);
        noiseS.prepare (sampleRate, 0.02f, 0.0f);
        delayTimeS.prepare (sampleRate, 0.15f, 0.25f * sr);
        fbS.prepare (sampleRate, 0.02f, 0.3f);
        delayMixS.prepare (sampleRate, 0.02f, 0.0f);
        mixS.prepare (sampleRate, 0.02f, 1.0f);
        outS.prepare (sampleRate, 0.02f, 1.0f);
        filterMixS.prepare (sampleRate, 0.02f, 0.0f);

        for (int c = 0; c < 2; ++c)
        {
            svf[c].reset();
            tape[c].prepare ((int) (0.05f * sr) + 4);
            echo[c].prepare ((int) (4.2f * sr) + 4);
            damp[c] = 0.0f;
            holdSample[c] = 0.0f;
            noiseLp[c] = 0.0f;
            dcX[c] = dcY[c] = 0.0f;
        }
        holdPhase = 1.0f;
        lfoPhase = 0.0f;
        wowPhase = 0.0f;
        flutterPhase = 0.0f;
    }

    void process (float* left, float* right, int numSamples, const Settings& s)
    {
        driveS.target = s.drive;
        cutoffS.target = std::log2 (std::clamp (s.cutoff, 20.0f, 20000.0f));
        resoS.target = s.reso;
        wowS.target = s.wow;
        noiseS.target = s.noise;
        delayTimeS.target = std::clamp (s.delaySeconds, 0.001f, 4.0f) * sr;
        fbS.target = std::clamp (s.feedback, 0.0f, 0.95f);
        delayMixS.target = s.delayMix;
        mixS.target = s.mix;
        outS.target = dbToGain (s.outputDb);

        // Filter on/off crossfades so switching type does not click
        if (s.filterType != 0) activeFilterType = s.filterType;
        filterMixS.target = s.filterType == 0 ? 0.0f : 1.0f;

        const float bits = std::clamp (s.bits, 1.0f, 16.0f);
        const float levels = std::pow (2.0f, bits - 1.0f);
        const bool crushBits = bits < 15.99f;
        const float holdInc = 1.0f / std::max (1.0f, s.downsample);
        const float lfoInc = s.lfoRate / sr;
        const float dampCoeff = std::exp (-kTwoPi * std::clamp (s.delayTone, 200.0f, 18000.0f) / sr);

        float* ch[2] = { left, right != nullptr ? right : left };
        const int numCh = right != nullptr ? 2 : 1;

        for (int n = 0; n < numSamples; ++n)
        {
            const float drive = driveS.next();
            const float logCut = cutoffS.next();
            const float reso = resoS.next();
            const float wow = wowS.next();
            const float noise = noiseS.next();
            const float dTime = delayTimeS.next();
            const float fb = fbS.next();
            const float dMix = delayMixS.next();
            const float mix = mixS.next();
            const float outGain = outS.next();
            const float fMix = filterMixS.next();

            // Drive parameters
            const float k = dbToGain (drive * 36.0f);
            const float bias = 0.25f * drive;
            const float tb = std::tanh (bias);
            const float makeup = 1.0f / (1.0f + drive);
            const float driveBlend = std::min (1.0f, drive * 8.0f);

            // Sample-rate reduction clock (shared by both channels)
            holdPhase += holdInc;
            const bool takeNew = holdPhase >= 1.0f;
            if (takeNew) holdPhase -= 1.0f;

            // LFO
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

            // Tape modulation: slow wow + fast flutter + a little drift
            wowPhase += 0.55f / sr;       if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutterPhase += 6.3f / sr;    if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
            const float drift = randomBipolar() * 0.02f;
            driftLp += 0.0005f * (drift * 50.0f - driftLp);
            const float wowMod = 0.7f * std::sin (kTwoPi * wowPhase) + 0.2f * std::sin (kTwoPi * flutterPhase) + 0.1f * driftLp;
            const float wowDepth = wow * wow * 0.004f * sr; // up to 4 ms
            const float tapeDelay = wowDepth * (1.0f + wowMod);

            // Filter coefficients every 8 samples
            if ((n & 7) == 0 && fMix > 0.0001f)
            {
                for (int c = 0; c < numCh; ++c)
                {
                    const float phase = lfoPhase + (c == 1 ? 0.25f : 0.0f);
                    const float lfo = std::sin (kTwoPi * phase);
                    const float cut = std::exp2 (logCut + lfo * s.lfoDepth * 4.0f);
                    svf[c].set (std::clamp (cut, 20.0f, 20000.0f), 0.5f + reso * reso * 14.0f, sr);
                }
            }

            float wetOut[2] = { 0.0f, 0.0f };

            for (int c = 0; c < numCh; ++c)
            {
                float x = ch[c][n];

                // 1. Drive (asymmetric tanh, gives some even harmonics)
                if (driveBlend > 0.0f)
                {
                    float sat = (std::tanh (k * x + bias) - tb) * makeup;
                    // DC blocker for the asymmetry
                    const float y = sat - dcX[c] + 0.9995f * dcY[c];
                    dcX[c] = sat; dcY[c] = y;
                    x += driveBlend * (y - x);
                }

                // 2. Crush
                if (takeNew) holdSample[c] = x;
                x = holdSample[c];
                if (crushBits)
                    x = std::round (x * levels) / levels;

                // 3. Filter
                if (fMix > 0.0001f)
                {
                    float lp, bp, hp;
                    svf[c].tick (x, lp, bp, hp);
                    const float filtered = activeFilterType == 1 ? lp : activeFilterType == 2 ? hp : bp;
                    x += fMix * (filtered - x);
                }

                // 4. Tape wow/flutter
                tape[c].push (x);
                if (wowDepth > 0.01f)
                    x = tape[c].read (tapeDelay);

                // 5. Noise (hiss + occasional crackle)
                if (noise > 0.0001f)
                {
                    const float white = randomBipolar();
                    noiseLp[c] += 0.35f * (white - noiseLp[c]);
                    float nse = noiseLp[c] * 0.06f * noise * noise;
                    if (randomUnipolar() < 0.00015f * noise)
                        nse += randomBipolar() * 0.35f * noise;
                    x += nse;
                }

                wetOut[c] = x;
            }

            // 6. Delay
            if (numCh == 2)
            {
                const float rL = echo[0].read (dTime);
                const float rR = echo[1].read (dTime);
                damp[0] = rL + dampCoeff * (damp[0] - rL);
                damp[1] = rR + dampCoeff * (damp[1] - rR);
                const float fbL = softLimit (damp[0] * fb);
                const float fbR = softLimit (damp[1] * fb);

                if (s.pingPong)
                {
                    echo[0].push (0.5f * (wetOut[0] + wetOut[1]) + fbR);
                    echo[1].push (fbL);
                }
                else
                {
                    echo[0].push (wetOut[0] + fbL);
                    echo[1].push (wetOut[1] + fbR);
                }
                wetOut[0] += dMix * rL;
                wetOut[1] += dMix * rR;
            }
            else
            {
                const float r = echo[0].read (dTime);
                damp[0] = r + dampCoeff * (damp[0] - r);
                echo[0].push (wetOut[0] + softLimit (damp[0] * fb));
                wetOut[0] += dMix * r;
            }

            // 7. Mix + output
            for (int c = 0; c < numCh; ++c)
            {
                const float dry = ch[c][n];
                float y = (dry + mix * (wetOut[c] - dry)) * outGain;
                if (! std::isfinite (y)) y = 0.0f;
                y = safetyClip (y);
                ch[c][n] = y;
            }
        }
    }

private:
    static inline float softLimit (float x) { return std::tanh (x); }

    // Transparent below -1 dBFS, then a soft knee that never exceeds 0 dBFS
    static inline float safetyClip (float x)
    {
        constexpr float t = 0.891f, room = 1.0f - t;
        const float a = std::fabs (x);
        if (a <= t) return x;
        return std::copysign (t + room * std::tanh ((a - t) / room), x);
    }

    inline float randomUnipolar()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xFFFFFF) / 16777216.0f;
    }
    inline float randomBipolar() { return randomUnipolar() * 2.0f - 1.0f; }

    float sr = 44100.0f;
    Smooth driveS, cutoffS, resoS, wowS, noiseS, delayTimeS, fbS, delayMixS, mixS, outS, filterMixS;
    SVF svf[2];
    DelayLine tape[2], echo[2];
    float damp[2] {}, holdSample[2] {}, noiseLp[2] {}, dcX[2] {}, dcY[2] {};
    float holdPhase = 1.0f, lfoPhase = 0.0f, wowPhase = 0.0f, flutterPhase = 0.0f, driftLp = 0.0f;
    int activeFilterType = 1;
    uint32_t rng = 0x12345678u;
};
} // namespace smadder
