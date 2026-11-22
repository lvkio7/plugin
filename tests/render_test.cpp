// Teste do motor: toca cada preset, mede o som e grava alguns WAVs.
#include "../Source/Presets.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
using namespace nosfera;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); }; auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = (int16_t) (std::clamp (L[i], -1.0f, 1.0f) * 32767), b = (int16_t) (std::clamp (R[i], -1.0f, 1.0f) * 32767); w16 ((uint16_t) a); w16 ((uint16_t) b); }
}

int main (int argc, char** argv) {
    const int sr = 48000, block = 256;
    const bool wav = argc > 1;
    int fails = 0; double worst = 0;
    std::FILE* csv = std::fopen ("tests/results.csv", "w");
    std::fprintf (csv, "name,category,rms,peak,nan,realtime_x,bright\n");
    for (const auto& pr : getPresets()) {
        Synth s; s.p = pr.params; s.prepare (sr);
        const bool single = pr.params.mono || pr.params.arp;
        const int notes[3] = { 57, 60, 64 };
        for (int i = 0; i < (single ? 1 : 3); ++i) s.noteOn (single ? 60 : notes[i]);
        const int total = (int) (sr * 2.0), offAt = (int) (sr * 1.4);
        std::vector<float> L ((size_t) total), R ((size_t) total);
        double sumSq = 0; float peak = 0; bool nan = false; bool released = false;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int pos = 0; pos < total; pos += block) {
            const int n = std::min (block, total - pos);
            if (! released && pos >= offAt) { for (int i = 0; i < 3; ++i) s.noteOff (single ? 60 : notes[i]); released = true; }
            s.render (L.data() + pos, R.data() + pos, n);
        }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        double maxWin = 0;
        for (int w = 0; w + 4800 <= offAt; w += 4800) {
            double ws = 0; for (int i = w; i < w + 4800; ++i) { ws += L[(size_t) i] * L[(size_t) i] + R[(size_t) i] * R[(size_t) i]; }
            maxWin = std::max (maxWin, std::sqrt (ws / 9600.0));
        }
        for (int i = 0; i < total; ++i) { const float a = std::abs (L[(size_t) i]), b = std::abs (R[(size_t) i]); if (! std::isfinite (a) || ! std::isfinite (b)) nan = true; peak = std::max (peak, std::max (a, b)); sumSq += a * a; }
        double dsq = 0, xsq = 0; for (int i = 4800; i < offAt; ++i) { const double dx = L[(size_t) i] - L[(size_t) i - 1]; dsq += dx * dx; xsq += (double) L[(size_t) i] * L[(size_t) i]; }
        const double bright = xsq > 0 ? std::sqrt (dsq / xsq) : 0;
        const double rt = 2.0 / secs; worst = std::min (worst == 0 ? rt : worst, rt);
        const bool bad = nan || maxWin < 0.01 || peak > 1.0f;
        if (bad) { ++fails; std::printf ("PROBLEMA: %s rms=%.4f peak=%.3f nan=%d\n", pr.name, maxWin, peak, nan); }
        std::fprintf (csv, "%s,%s,%.4f,%.3f,%d,%.1f,%.4f\n", pr.name, pr.category, maxWin, peak, nan, rt, bright);
        if (wav) writeWav (std::string ("tests/wav/") + pr.name + ".wav", L, R, sr);
    }
    std::fclose (csv);
    std::printf ("presets: %zu | com problema: %d | pior desempenho: %.0fx tempo real\n", getPresets().size(), fails, worst);
    return fails ? 1 : 0;
}
