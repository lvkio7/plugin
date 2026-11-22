// Nosfera — motor de som em C++ puro (sem dependência do JUCE).
// Espelha o motor do protótipo web: 2 osciladores com unison, sub, ruído,
// filtro LP/HP/BP com envelope, ADSR de volume, LFO (filtro, vibrato, trêmolo),
// glide/mono, arpejador, saturação, chorus, delay, reverb e limitador.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nosfera {

enum Wave { Saw = 0, Square = 1, Triangle = 2, Sine = 3 };
enum FilterType { LowPass = 0, HighPass = 1, BandPass = 2 };

struct Params {
    int w1 = Saw, w2 = Saw;
    float o2 = 0, det = 10, mix = .5f;
    int uni = 1;
    float spread = 0, sub = 0, noise = 0;
    int ftype = LowPass;
    float cut = 3000, res = 1, fenv = .3f, fa = .005f, fd = .3f, fs = .3f, fr = .4f;
    float a = .005f, d = .3f, s = .7f, r = .4f;
    float lfoR = 4, lfoD = 0, vib = 0, trem = 0, glide = .06f;
    bool mono = false, arp = false;
    float bpm = 128, drive = .1f, cho = 0, dly = .15f, fb = .3f, rev = .2f, vol = .7f;
};

namespace detail {
constexpr double kTwoPi = 6.283185307179586;

inline float polyBlep (double t, double dt) {
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

inline float onePoleCoef (double seconds, double sr) {
    return (float) (1.0 - std::exp (-1.0 / std::max (1.0e-5, seconds * sr)));
}

struct Rng {
    uint32_t s = 22222;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s * (1.0 / 4294967296.0) * 2.0 - 1.0); }
};

// Filtro de estado variável (TPT) — estável com modulação rápida.
struct Svf {
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (double fc, double q, double sr) {
        const double g = std::tan (3.141592653589793 * std::clamp (fc, 20.0, sr * 0.49) / sr);
        k = (float) (1.0 / std::max (0.3, q));
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
    }
    float process (float x, int type) {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        if (type == LowPass) return v2;
        if (type == HighPass) return x - k * v1 - v2;
        return k * v1; // passa-banda com pico de 0 dB
    }
    void reset() { ic1 = ic2 = 0; }
};

struct Osc {
    double phase = 0, ratio = 1;
    float detune = 0, level = 0, gainL = 1, gainR = 1;
    int wave = Saw;
    bool isOsc2 = false;
};

// Reverb estilo Freeverb (8 combs + 4 allpasses por canal).
class Reverb {
public:
    void prepare (double sr) {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4] = { 556, 441, 341, 225 };
        const double sc = sr / 44100.0;
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < 8; ++i) { combs[c][i].assign ((size_t) ((combT[i] + c * 23) * sc), 0.0f); cIdx[c][i] = 0; filt[c][i] = 0; }
            for (int i = 0; i < 4; ++i) { aps[c][i].assign ((size_t) ((apT[i] + c * 23) * sc), 0.0f); aIdx[c][i] = 0; }
        }
    }
    void process (float inL, float inR, float& outL, float& outR) {
        const float in = (inL + inR) * 0.015f, feedback = 0.86f, damp = 0.22f;
        float o[2] = { 0, 0 };
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < 8; ++i) {
                auto& b = combs[c][i]; auto& idx = cIdx[c][i];
                const float y = b[idx];
                filt[c][i] = y * (1 - damp) + filt[c][i] * damp;
                b[idx] = in + filt[c][i] * feedback;
                if (++idx >= b.size()) idx = 0;
                o[c] += y;
            }
            for (int i = 0; i < 4; ++i) {
                auto& b = aps[c][i]; auto& idx = aIdx[c][i];
                const float bo = b[idx];
                b[idx] = o[c] + bo * 0.5f;
                o[c] = bo - o[c];
                if (++idx >= b.size()) idx = 0;
            }
        }
        outL = o[0] * 3.0f; outR = o[1] * 3.0f;
    }
private:
    std::vector<float> combs[2][8], aps[2][4];
    size_t cIdx[2][8] {}, aIdx[2][4] {};
    float filt[2][8] {};
};

class DelayLine {
public:
    void prepare (double sr, double maxSeconds) { buf.assign ((size_t) (sr * maxSeconds) + 4, 0.0f); w = 0; }
    void push (float x) { buf[w] = x; if (++w >= buf.size()) w = 0; }
    float read (double delaySamples) const {
        const double rp = (double) w - delaySamples - 1.0 + (double) buf.size() * 2.0;
        const size_t i0 = (size_t) rp % buf.size(), i1 = (i0 + 1) % buf.size();
        const float f = (float) (rp - std::floor (rp));
        return buf[i0] + (buf[i1] - buf[i0]) * f;
    }
private:
    std::vector<float> buf; size_t w = 0;
};
} // namespace detail

class Synth {
public:
    static constexpr int kMaxVoices = 16, kMaxOsc = 16, kBlock = 32;
    Params p;

    void prepare (double sampleRate) {
        sr = sampleRate;
        rev.prepare (sr);
        for (auto& d : delay) d.prepare (sr, 2.1);
        for (auto& c : chorus) c.prepare (sr, 0.05);
        for (auto& v : voices) v.active = false;
        held.clear(); arpTimer = 0; arpIndex = 0;
    }

    // ---------- notas ----------
    void noteOn (int note) {
        if (std::find (held.begin(), held.end(), note) != held.end()) return;
        held.push_back (note);
        if (p.arp) { if (held.size() == 1) { arpTimer = 0; arpIndex = 0; } return; }
        if (p.mono) {
            if (monoVoice >= 0 && voices[(size_t) monoVoice].active && ! voices[(size_t) monoVoice].releasing) { glideTo (voices[(size_t) monoVoice], note); return; }
            monoVoice = startVoice (note, -1);
            return;
        }
        startVoice (note, -1);
    }
    void noteOff (int note) {
        auto it = std::find (held.begin(), held.end(), note);
        if (it == held.end()) return;
        held.erase (it);
        if (p.arp) return;
        if (p.mono) {
            if (monoVoice < 0) return;
            auto& v = voices[(size_t) monoVoice];
            if (! held.empty()) glideTo (v, held.back());
            else { release (v); monoVoice = -1; }
            return;
        }
        for (auto& v : voices) if (v.active && ! v.releasing && v.note == note && v.autoRelease < 0) release (v);
    }
    void allNotesOff() {
        held.clear(); monoVoice = -1;
        for (auto& v : voices) if (v.active) release (v);
    }
    void setBend (float cents) { bendCents = cents; }
    void setModWheel (float v) { modWheel = std::clamp (v, 0.0f, 1.0f); }

    // ---------- áudio ----------
    void render (float* outL, float* outR, int numSamples) {
        int pos = 0;
        while (pos < numSamples) {
            const int n = std::min (kBlock, numSamples - pos);
            renderBlock (outL + pos, outR + pos, n);
            pos += n;
        }
    }

    float getLevel() const { return level; }
    // Últimas amostras (mono) para o osciloscópio da interface.
    void copyScope (float* dest, int n) const {
        size_t r = (scopeW + scope.size() - (size_t) n) % scope.size();
        for (int i = 0; i < n; ++i) { dest[i] = scope[r]; if (++r >= scope.size()) r = 0; }
    }

private:
    struct Voice {
        bool active = false, releasing = false;
        int note = 60, nOsc = 0;
        uint64_t age = 0;
        double freq = 440, target = 440;
        std::array<detail::Osc, kMaxOsc> osc;
        float subLevel = 0, noiseLevel = 0;
        double subPhase = 0;
        int ampStage = 0; float amp = 0;
        int fStage = 0; float fEnv = 0;
        detail::Svf fL, fR;
        int64_t autoRelease = -1;
        Params snap;
    };

    double sr = 48000;
    std::array<Voice, kMaxVoices> voices;
    std::vector<int> held;
    int monoVoice = -1;
    uint64_t ageCounter = 0;
    double lfoPhase = 0, choPhase = 0, arpTimer = 0;
    size_t arpIndex = 0;
    float bendCents = 0, modWheel = 0, level = 0;
    detail::Rng rng;
    detail::Reverb rev;
    std::array<detail::DelayLine, 2> delay, chorus;
    std::array<float, 2048> scope {};
    size_t scopeW = 0;

    static double mtof (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

    int startVoice (int note, int64_t autoReleaseSamples) {
        int idx = -1; uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < kMaxVoices; ++i) if (! voices[(size_t) i].active) { idx = i; break; }
        if (idx < 0) for (int i = 0; i < kMaxVoices; ++i) if (voices[(size_t) i].age < oldest) { oldest = voices[(size_t) i].age; idx = i; }
        auto& v = voices[(size_t) idx];
        v = Voice();
        v.active = true; v.note = note; v.age = ++ageCounter; v.snap = p;
        v.freq = v.target = mtof (note);
        v.autoRelease = autoReleaseSamples;
        const int U = std::clamp (p.uni, 1, 7);
        const float n = 1.0f / std::sqrt ((float) U), wd = std::min (1.0f, p.spread / 20.0f) * 0.8f;
        auto add = [&] (int wave, double ratio, float det, float lvl, float pan, bool is2) {
            if (v.nOsc >= kMaxOsc) return;
            auto& o = v.osc[(size_t) v.nOsc++];
            o.wave = wave; o.ratio = ratio; o.detune = det; o.level = lvl; o.isOsc2 = is2;
            o.phase = rng.next() * 0.5 + 0.5;
            if (std::abs (pan) > 1.0e-6f) { const float x = (pan + 1) * 0.5f; o.gainL = std::cos (x * 1.5707963f); o.gainR = std::sin (x * 1.5707963f); }
        };
        for (int i = 0; i < U; ++i) {
            const float x = U > 1 ? (float) i / (float) (U - 1) - 0.5f : 0.0f;
            add (p.w1, 1.0, x * 2 * p.spread, 0.5f * n, x * 2 * wd, false);
            if (p.mix > 0) add (p.w2, std::pow (2.0, p.o2 / 12.0), p.det - x * 2 * p.spread, p.mix * 0.5f * n, -x * 2 * wd, true);
        }
        v.subLevel = p.sub * 0.6f; v.noiseLevel = p.noise * 0.3f;
        return idx;
    }

    void glideTo (Voice& v, int note) { v.note = note; v.target = mtof (note); }
    static void release (Voice& v) { v.releasing = true; v.ampStage = 2; v.fStage = 2; }

    void renderBlock (float* L, float* R, int n) {
        // ---- arpejador (1/16, sobe 2 oitavas) ----
        if (p.arp && ! held.empty()) {
            const double step = 60.0 / std::max (30.0f, p.bpm) / 4.0 * sr;
            arpTimer -= n;
            if (arpTimer <= 0) {
                std::vector<int> seq (held); std::sort (seq.begin(), seq.end());
                const size_t base = seq.size(); for (size_t i = 0; i < base; ++i) seq.push_back (seq[i] + 12);
                startVoice (seq[arpIndex++ % seq.size()], (int64_t) (step * 0.7));
                arpTimer += step;
            }
        }
        // ---- valores de controle do bloco ----
        lfoPhase += p.lfoR * n / sr; lfoPhase -= std::floor (lfoPhase);
        const float lfo = (float) std::sin (lfoPhase * detail::kTwoPi);
        const float cutMod = std::pow (2.0f, lfo * (p.lfoD + modWheel * (1 - p.lfoD) * 0.6f) * 2400.0f / 1200.0f);
        const float vibC = lfo * p.vib * 200.0f, trem = 1.0f - p.trem * 0.5f + lfo * p.trem * 0.5f;
        const double q = p.ftype == BandPass ? std::max (0.3f, p.res) : std::pow (10.0, p.res / 20.0);
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);

        for (auto& v : voices) {
            if (! v.active) continue;
            const Params& vs = v.snap;
            if (std::abs (v.target - v.freq) > 1.0e-9) v.freq += (v.target - v.freq) * (1.0 - std::exp (-n / (std::max (0.002f, p.glide / 3.0f) * sr)));
            const float peak = std::min (20000.0f, p.cut * std::pow (2.0f, vs.fenv * 7.0f));
            const float fc = (p.cut + (peak - p.cut) * v.fEnv) * cutMod;
            v.fL.set (fc, q, sr); v.fR.set (fc, q, sr);
            double inc[kMaxOsc];
            for (int i = 0; i < v.nOsc; ++i) {
                const auto& o = v.osc[(size_t) i];
                inc[i] = v.freq * o.ratio * std::pow (2.0, (o.detune + bendCents + vibC) / 1200.0) / sr;
            }
            const double subInc = v.freq * 0.5 * std::pow (2.0, bendCents / 1200.0) / sr;
            const float aC = detail::onePoleCoef (vs.d / 3.0, sr), rC = detail::onePoleCoef (vs.r / 4.0, sr);
            const float fdC = detail::onePoleCoef (vs.fd / 3.0, sr), frC = detail::onePoleCoef (vs.fr / 4.0, sr);
            const float aStep = 0.9f / (float) std::max (1.0, vs.a * sr), fStep = 1.0f / (float) std::max (1.0, vs.fa * sr);
            for (int s = 0; s < n; ++s) {
                // envelopes
                if (v.autoRelease >= 0 && --v.autoRelease < 0 && ! v.releasing) release (v);
                if (v.ampStage == 0) { v.amp += aStep; if (v.amp >= 0.9f) { v.amp = 0.9f; v.ampStage = 1; } }
                else if (v.ampStage == 1) v.amp += (vs.s * 0.9f - v.amp) * aC;
                else v.amp += (0 - v.amp) * rC;
                if (v.fStage == 0) { v.fEnv += fStep; if (v.fEnv >= 1) { v.fEnv = 1; v.fStage = 1; } }
                else if (v.fStage == 1) v.fEnv += (vs.fs - v.fEnv) * fdC;
                else v.fEnv += (0 - v.fEnv) * frC;
                // osciladores
                float sl = 0, sr2 = 0;
                for (int i = 0; i < v.nOsc; ++i) {
                    auto& o = v.osc[(size_t) i];
                    const double t = o.phase, dt = inc[i];
                    float x;
                    switch (o.wave) {
                        case Saw: x = (float) (2.0 * t - 1.0) - detail::polyBlep (t, dt); break;
                        case Square: { double t2 = t + 0.5; t2 -= std::floor (t2); x = (t < 0.5 ? 1.0f : -1.0f) + detail::polyBlep (t, dt) - detail::polyBlep (t2, dt); break; }
                        case Triangle: x = (float) (4.0 * std::abs (t - 0.5) - 1.0); break;
                        default: x = (float) std::sin (t * detail::kTwoPi); break;
                    }
                    o.phase += dt; o.phase -= std::floor (o.phase);
                    sl += x * o.level * o.gainL; sr2 += x * o.level * o.gainR;
                }
                if (v.subLevel > 0) { const float x = (float) std::sin (v.subPhase * detail::kTwoPi) * v.subLevel; v.subPhase += subInc; v.subPhase -= std::floor (v.subPhase); sl += x; sr2 += x; }
                if (v.noiseLevel > 0) { const float x = rng.next() * v.noiseLevel; sl += x; sr2 += x; }
                const float g = v.amp * trem;
                L[s] += v.fL.process (sl, p.ftype) * g;
                R[s] += v.fR.process (sr2, p.ftype) * g;
            }
            if (v.releasing && v.amp < 1.0e-4f) v.active = false;
        }

        // ---- efeitos globais ----
        const float k = 1.0f + p.drive * p.drive * 9.0f, tk = std::tanh (k);
        const double dTime = 60.0 / std::max (30.0f, p.bpm) * 0.75 * sr;
        const float fb = std::min (0.85f, p.fb), wet = p.dly * 0.8f, choG = p.cho * 0.6f, revG = p.rev * 1.2f, vol = p.vol;
        float sumSq = 0;
        for (int s = 0; s < n; ++s) {
            auto shape = [&] (float x) { return std::tanh (k * std::clamp (x, -1.0f, 1.0f)) / tk; };
            const float dl = shape (L[s] * 0.45f) * 1.8f, dr = shape (R[s] * 0.45f) * 1.8f; // ganho de compensação do drive
            // delay
            const float yl = delay[0].read (dTime), yr = delay[1].read (dTime);
            delay[0].push (dl + yl * fb); delay[1].push (dr + yr * fb);
            // chorus
            choPhase += 0.55 / sr; choPhase -= std::floor (choPhase);
            const float cm = (float) std::sin (choPhase * detail::kTwoPi) * 0.0028f;
            const float mono = (dl + dr) * 0.5f;
            chorus[0].push (mono); chorus[1].push (mono);
            const float cl = chorus[0].read ((0.012 + cm) * sr) * choG, cr = chorus[1].read ((0.019 - cm) * sr) * choG;
            // reverb
            float rl, rr; rev.process (dl + cl, dr + cr, rl, rr);
            float outL = (dl + yl * wet + cl + rl * revG) * vol, outR = (dr + yr * wet + cr + rr * revG) * vol;
            // limitador suave
            outL = std::tanh (outL * 1.1f) / 1.1f * 1.0f; outR = std::tanh (outR * 1.1f) / 1.1f * 1.0f;
            L[s] = outL; R[s] = outR;
            sumSq += outL * outL + outR * outR;
            scope[scopeW] = (outL + outR) * 0.5f; if (++scopeW >= scope.size()) scopeW = 0;
        }
        level = std::sqrt (sumSq / (2.0f * (float) n));
    }
};

} // namespace nosfera
