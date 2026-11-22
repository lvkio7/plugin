// Definição dos parâmetros do Nosfera (mesmos nomes usados pela interface).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthEngine.h"

namespace nosfera {

enum class Kind { Float, LogFloat, Int, Choice, Bool };

struct ParamDef {
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
};

inline const juce::StringArray& waveNames() { static juce::StringArray s { "sawtooth", "square", "triangle", "sine" }; return s; }
inline const juce::StringArray& filterNames() { static juce::StringArray s { "lowpass", "highpass", "bandpass" }; return s; }

inline const std::vector<ParamDef>& paramDefs() {
    static const std::vector<ParamDef> defs = {
        { "w1", "Wave 1", Kind::Choice, 0, 3, 0 },          { "w2", "Wave 2", Kind::Choice, 0, 3, 0 },
        { "o2", "Pitch 2", Kind::Int, -24, 24, 0 },          { "det", "Detune", Kind::Float, 0, 50, 10 },
        { "mix", "Osc 2", Kind::Float, 0, 1, .5f },          { "uni", "Unison", Kind::Int, 1, 7, 1 },
        { "spread", "Spread", Kind::Float, 0, 60, 0 },       { "sub", "Sub", Kind::Float, 0, 1, 0 },
        { "noise", "Noise", Kind::Float, 0, 1, 0 },          { "ftype", "Filter type", Kind::Choice, 0, 2, 0 },
        { "cut", "Cutoff", Kind::LogFloat, 40, 18000, 3000 },{ "res", "Resonance", Kind::Float, 0, 20, 1 },
        { "fenv", "Env amount", Kind::Float, 0, 1, .3f },    { "fa", "Filter attack", Kind::LogFloat, .001f, 3, .005f },
        { "fd", "Filter decay", Kind::LogFloat, .01f, 3, .3f }, { "fs", "Filter sustain", Kind::Float, 0, 1, .3f },
        { "fr", "Filter release", Kind::LogFloat, .01f, 5, .4f }, { "a", "Attack", Kind::LogFloat, .001f, 4, .005f },
        { "d", "Decay", Kind::LogFloat, .01f, 4, .3f },      { "s", "Sustain", Kind::Float, 0, 1, .7f },
        { "r", "Release", Kind::LogFloat, .01f, 6, .4f },    { "lfoR", "LFO rate", Kind::LogFloat, .05f, 12, 4 },
        { "lfoD", "LFO filter", Kind::Float, 0, 1, 0 },      { "vib", "Vibrato", Kind::Float, 0, 1, 0 },
        { "trem", "Tremolo", Kind::Float, 0, 1, 0 },         { "glide", "Glide", Kind::Float, 0, .5f, .06f },
        { "mono", "Mono", Kind::Bool, 0, 1, 0 },             { "arp", "Arp", Kind::Bool, 0, 1, 0 },
        { "bpm", "Arp BPM", Kind::Int, 60, 180, 128 },       { "drive", "Drive", Kind::Float, 0, 1, .1f },
        { "cho", "Chorus", Kind::Float, 0, 1, 0 },           { "dly", "Delay", Kind::Float, 0, 1, .15f },
        { "fb", "Feedback", Kind::Float, 0, .85f, .3f },     { "rev", "Reverb", Kind::Float, 0, 1, .2f },
        { "vol", "Volume", Kind::Float, 0, 1, .7f },
    };
    return defs;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : paramDefs()) {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind) {
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::String (d.id) == "ftype" ? filterNames() : waveNames(), (int) d.def));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > .5f));
                break;
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.min, (int) d.max, (int) d.def));
                break;
            case Kind::LogFloat: {
                const float lo = d.min, hi = d.max;
                juce::NormalisableRange<float> range (lo, hi,
                    [] (float a, float b, float x) { return a * std::pow (b / a, x); },
                    [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, range, d.def));
                break;
            }
            case Kind::Float:
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, juce::NormalisableRange<float> (d.min, d.max), d.def));
                break;
        }
    }
    return layout;
}

// Lê os valores atuais dos parâmetros para a struct do motor.
inline void readParams (juce::AudioProcessorValueTreeState& s, Params& p) {
    auto v = [&] (const char* id) { return s.getRawParameterValue (id)->load(); };
    p.w1 = (int) v ("w1"); p.w2 = (int) v ("w2"); p.o2 = v ("o2"); p.det = v ("det"); p.mix = v ("mix");
    p.uni = (int) v ("uni"); p.spread = v ("spread"); p.sub = v ("sub"); p.noise = v ("noise");
    p.ftype = (int) v ("ftype"); p.cut = v ("cut"); p.res = v ("res"); p.fenv = v ("fenv");
    p.fa = v ("fa"); p.fd = v ("fd"); p.fs = v ("fs"); p.fr = v ("fr");
    p.a = v ("a"); p.d = v ("d"); p.s = v ("s"); p.r = v ("r");
    p.lfoR = v ("lfoR"); p.lfoD = v ("lfoD"); p.vib = v ("vib"); p.trem = v ("trem"); p.glide = v ("glide");
    p.mono = v ("mono") > .5f; p.arp = v ("arp") > .5f; p.bpm = v ("bpm");
    p.drive = v ("drive"); p.cho = v ("cho"); p.dly = v ("dly"); p.fb = v ("fb"); p.rev = v ("rev"); p.vol = v ("vol");
}

// Valor "real" de um preset do motor para um parâmetro (usado nos programas do host).
inline float presetValue (const Params& p, const juce::String& id) {
    const std::pair<const char*, float> values[] = {
        { "w1", (float) p.w1 }, { "w2", (float) p.w2 }, { "o2", p.o2 }, { "det", p.det }, { "mix", p.mix },
        { "uni", (float) p.uni }, { "spread", p.spread }, { "sub", p.sub }, { "noise", p.noise },
        { "ftype", (float) p.ftype }, { "cut", p.cut }, { "res", p.res }, { "fenv", p.fenv },
        { "fa", p.fa }, { "fd", p.fd }, { "fs", p.fs }, { "fr", p.fr },
        { "a", p.a }, { "d", p.d }, { "s", p.s }, { "r", p.r },
        { "lfoR", p.lfoR }, { "lfoD", p.lfoD }, { "vib", p.vib }, { "trem", p.trem }, { "glide", p.glide },
        { "mono", p.mono ? 1.0f : 0.0f }, { "arp", p.arp ? 1.0f : 0.0f }, { "bpm", p.bpm },
        { "drive", p.drive }, { "cho", p.cho }, { "dly", p.dly }, { "fb", p.fb }, { "rev", p.rev }, { "vol", p.vol } };
    for (const auto& [key, value] : values)
        if (id == key) return value;
    return 0.0f;
}

} // namespace nosfera
