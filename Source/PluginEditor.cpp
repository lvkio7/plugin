#include "PluginEditor.h"
#include "BinaryData.h"

NosferaEditor::NosferaEditor (NosferaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), web (makeOptions())
{
    addAndMakeVisible (web);
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setResizable (true, true);
    setResizeLimits (640, 480, 2600, 2000);
    setSize (980, 740);
    lastValues.assign (nosfera::paramDefs().size(), -1.0e9f);
    startTimerHz (30);
}

juce::WebBrowserComponent::Options NosferaEditor::makeOptions()
{
    using Options = juce::WebBrowserComponent::Options;
    return Options{}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2{}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                                              .getChildFile ("Nosfera").getChildFile ("WebView"))
                                     .withBackgroundColour (juce::Colour (0xffeeedea)))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        // A página avisa que carregou: mandamos o estado atual do plugin.
        .withEventListener ("ready", [this] (const juce::var&) { ready = true; sendState(); })
        // Notas tocadas no teclado da interface.
        .withEventListener ("note", [this] (const juce::var& e)
        {
            const int m = (int) e.getProperty ("m", 60);
            if ((bool) e.getProperty ("on", false))
                proc.keyboard.noteOn (1, m, (float) (int) e.getProperty ("v", 100) / 127.0f);
            else
                proc.keyboard.noteOff (1, m, 0.0f);
        })
        // Um knob mudou.
        .withEventListener ("param", [this] (const juce::var& e)
        {
            proc.setParameterFromUi (e.getProperty ("id", {}).toString(), e.getProperty ("v", {}));
        })
        // Um preset foi escolhido na interface.
        .withEventListener ("preset", [this] (const juce::var& e)
        {
            if (auto* obj = e.getProperty ("p", {}).getDynamicObject())
                for (const auto& prop : obj->getProperties())
                    proc.setParameterFromUi (prop.name.toString(), prop.value);
            proc.presetName = e.getProperty ("name", {}).toString();
        })
        .withEventListener ("panic", [this] (const juce::var&) { proc.uiPanic = true; proc.keyboard.allNotesOff (1); })
        .withEventListener ("bend", [this] (const juce::var& e) { proc.uiBend = (float) (double) e.getProperty ("c", 0.0); })
        .withEventListener ("mod", [this] (const juce::var& e) { proc.uiMod = (float) (double) e.getProperty ("v", 0.0); });
}

std::optional<juce::WebBrowserComponent::Resource> NosferaEditor::getResource (const juce::String& url)
{
    const auto path = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    if (path.isEmpty() || path == "index.html" || path == "/")
    {
        const auto* bytes = reinterpret_cast<const std::byte*> (BinaryData::index_html);
        return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + BinaryData::index_htmlSize), "text/html; charset=utf-8" };
    }
    return std::nullopt;
}

void NosferaEditor::sendState()
{
    auto* params = new juce::DynamicObject();
    const auto& defs = nosfera::paramDefs();
    for (size_t i = 0; i < defs.size(); ++i)
    {
        params->setProperty (defs[i].id, proc.getParameterForUi (defs[i].id));
        lastValues[i] = proc.apvts.getRawParameterValue (defs[i].id)->load();
    }
    auto* msg = new juce::DynamicObject();
    msg->setProperty ("p", juce::var (params));
    msg->setProperty ("name", proc.presetName);
    web.emitEventIfBrowserIsVisible ("state", juce::var (msg));
    seenStateVersion = proc.stateVersion.load();
}

void NosferaEditor::timerCallback()
{
    if (! ready) return;

    // Preset/estado trocado pelo FL, ou automação mexendo em knobs.
    bool changed = proc.stateVersion.load() != seenStateVersion;
    if (! changed && ++tick % 6 == 0)
    {
        const auto& defs = nosfera::paramDefs();
        for (size_t i = 0; i < defs.size() && ! changed; ++i)
            changed = std::abs (proc.apvts.getRawParameterValue (defs[i].id)->load() - lastValues[i]) > 1.0e-5f;
    }
    if (changed) sendState();

    // Notas vindas do FL (piano roll ou teclado MIDI) acendem as teclas.
    for (int n = 0; n < 128; ++n)
    {
        const uint8_t on = proc.noteState[(size_t) n].load();
        if (on != lastNotes[(size_t) n])
        {
            lastNotes[(size_t) n] = on;
            auto* e = new juce::DynamicObject();
            e->setProperty ("m", n);
            e->setProperty ("on", on != 0);
            web.emitEventIfBrowserIsVisible ("hostnote", juce::var (e));
        }
    }

    // Osciloscópio e luz piloto.
    std::array<float, 768> wave {};
    proc.copyScope (wave.data(), (int) wave.size());
    juce::Array<juce::var> arr;
    arr.ensureStorageAllocated ((int) wave.size());
    for (const float v : wave) arr.add (std::round (v * 1000.0f) / 1000.0f);
    auto* meter = new juce::DynamicObject();
    meter->setProperty ("lvl", proc.getLevel());
    meter->setProperty ("w", arr);
    web.emitEventIfBrowserIsVisible ("meter", juce::var (meter));
}
