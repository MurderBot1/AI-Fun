#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/engine.h"

#include <algorithm>

namespace audio {

struct Engine::Impl {
    ma_engine engine{};
    ma_waveform waveform{};
    ma_sound toneSound{};
    bool engineReady = false;
    bool toneReady = false;
    bool toneOn = false;
    float volume = 1.0f;
};

Engine::Engine() : m_impl(std::make_unique<Impl>()) {
    if (ma_engine_init(nullptr, &m_impl->engine) != MA_SUCCESS)
        return;
    m_impl->engineReady = true;

    ma_waveform_config cfg = ma_waveform_config_init(
        ma_format_f32, ma_engine_get_channels(&m_impl->engine),
        ma_engine_get_sample_rate(&m_impl->engine), ma_waveform_type_sine, 0.2, 440.0);
    if (ma_waveform_init(&cfg, &m_impl->waveform) != MA_SUCCESS)
        return;
    if (ma_sound_init_from_data_source(&m_impl->engine, &m_impl->waveform, 0, nullptr,
                                       &m_impl->toneSound) == MA_SUCCESS)
        m_impl->toneReady = true;
}

Engine::~Engine() {
    if (m_impl->toneReady)
        ma_sound_uninit(&m_impl->toneSound);
    if (m_impl->engineReady)
        ma_engine_uninit(&m_impl->engine);
}

bool Engine::ok() const {
    return m_impl->engineReady;
}

void Engine::setMasterVolume(float volume) {
    m_impl->volume = std::min(1.0f, std::max(0.0f, volume));
    if (m_impl->engineReady)
        ma_engine_set_volume(&m_impl->engine, m_impl->volume);
}

float Engine::masterVolume() const {
    return m_impl->volume;
}

bool Engine::playFile(const std::string& path) {
    if (!m_impl->engineReady)
        return false;
    return ma_engine_play_sound(&m_impl->engine, path.c_str(), nullptr) == MA_SUCCESS;
}

void Engine::setTone(bool enabled, float frequencyHz, float amplitude) {
    m_impl->toneOn = enabled;
    if (!m_impl->toneReady)
        return;
    ma_waveform_set_frequency(&m_impl->waveform, std::max(1.0f, frequencyHz));
    ma_waveform_set_amplitude(&m_impl->waveform, std::min(1.0f, std::max(0.0f, amplitude)));
    if (enabled)
        ma_sound_start(&m_impl->toneSound);
    else
        ma_sound_stop(&m_impl->toneSound);
}

bool Engine::toneEnabled() const {
    return m_impl->toneOn;
}

} // namespace audio
