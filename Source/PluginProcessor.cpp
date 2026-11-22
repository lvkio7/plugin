#include "PluginProcessor.h"
#include "PluginEditor.h"

NosferaProcessor::NosferaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NOSFERA", nosfera::createLayout())
{
    for (auto& n : noteState) n.store (0);
    setCurrentProgram (0);
}

void NosferaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    tmpL.assign ((size_t) juce::jmax (samplesPerBlock, 32), 0.0f);
    tmpR.assign ((size_t) juce::jmax (samplesPerBlock, 32), 0.0f);
    keyboard.reset();
}

bool NosferaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NosferaProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                         { synth.noteOn (m.getNoteNumber()); noteState[(size_t) m.getNoteNumber()].store (1); }
    else if (m.isNoteOff())                   { synth.noteOff (m.getNoteNumber()); noteState[(size_t) m.getNoteNumber()].store (0); }
    else if (m.isPitchWheel())                hostBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * 200.0f;
    else if (m.isController() && m.getControllerNumber() == 1) hostMod = (float) m.getControllerValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        synth.allNotesOff();
        for (auto& n : noteState) n.store (0);
    }
}

void NosferaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if ((int) tmpL.size() < numSamples) { tmpL.resize ((size_t) numSamples); tmpR.resize ((size_t) numSamples); }

    nosfera::readParams (apvts, synth.p);
    keyboard.processNextMidiBuffer (midi, 0, numSamples, true);

    if (uiPanic.exchange (false))
    {
        synth.allNotesOff();
        for (auto& n : noteState) n.store (0);
    }
    synth.setBend (hostBend + uiBend.load());
    synth.setModWheel (juce::jmax (hostMod, uiMod.load()));

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        if (t > pos) { synth.render (tmpL.data() + pos, tmpR.data() + pos, t - pos); pos = t; }
        handleMidi (meta.getMessage());
    }
    if (pos < numSamples) synth.render (tmpL.data() + pos, tmpR.data() + pos, numSamples - pos);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, tmpL.data(), numSamples);
        buffer.copyFrom (1, 0, tmpR.data(), numSamples);
        for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, tmpL.data(), numSamples, 0.5f);
        buffer.addFrom (0, 0, tmpR.data(), numSamples, 0.5f);
    }

    level.store (synth.getLevel());
    if (const juce::SpinLock::ScopedTryLockType lock (scopeLock); lock.isLocked())
        synth.copyScope (scope.data(), (int) scope.size());
}

void NosferaProcessor::copyScope (float* dest, int n)
{
    const juce::SpinLock::ScopedLockType lock (scopeLock);
    const int count = juce::jmin (n, (int) scope.size());
    std::copy (scope.end() - count, scope.end(), dest);
}

// ---------- programas (presets) ----------
int NosferaProcessor::getNumPrograms() { return (int) nosfera::getPresets().size(); }

const juce::String NosferaProcessor::getProgramName (int index)
{
    const auto& list = nosfera::getPresets();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void NosferaProcessor::setCurrentProgram (int index)
{
    const auto& list = nosfera::getPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentProgram = index;
    const auto& pr = list[(size_t) index];
    for (const auto& d : nosfera::paramDefs())
        if (auto* prm = apvts.getParameter (d.id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (nosfera::presetValue (pr.params, d.id)));
    presetName = pr.name;
    ++stateVersion;
}

// ---------- ponte com a interface ----------
void NosferaProcessor::setParameterFromUi (const juce::String& id, const juce::var& value)
{
    auto* prm = apvts.getParameter (id);
    if (prm == nullptr) return;
    float real = 0;
    if (id == "w1" || id == "w2") real = (float) juce::jmax (0, nosfera::waveNames().indexOf (value.toString()));
    else if (id == "ftype")       real = (float) juce::jmax (0, nosfera::filterNames().indexOf (value.toString()));
    else if (value.isBool())      real = (bool) value ? 1.0f : 0.0f;
    else                          real = (float) (double) value;
    const float norm = prm->convertTo0to1 (real);
    if (std::abs (prm->getValue() - norm) < 1.0e-6f) return;
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (norm);
    prm->endChangeGesture();
}

juce::var NosferaProcessor::getParameterForUi (const juce::String& id) const
{
    const float v = apvts.getRawParameterValue (id)->load();
    if (id == "w1" || id == "w2") return nosfera::waveNames()[(int) v];
    if (id == "ftype")            return nosfera::filterNames()[(int) v];
    if (id == "mono" || id == "arp") return v > .5f ? 1 : 0;
    return v;
}

// ---------- estado salvo no projeto do FL ----------
void NosferaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void NosferaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            presetName = state.getProperty ("preset").toString();
            apvts.replaceState (state);
            // Garante que todos os parâmetros (inclusive os liga/desliga) fiquem exatamente no valor salvo.
            for (const auto& d : nosfera::paramDefs())
                if (auto* prm = apvts.getParameter (d.id))
                    prm->setValueNotifyingHost (prm->convertTo0to1 (apvts.getRawParameterValue (d.id)->load()));
            ++stateVersion;
        }
}

juce::AudioProcessorEditor* NosferaProcessor::createEditor() { return new NosferaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NosferaProcessor(); }
