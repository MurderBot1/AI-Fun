#pragma once

#include <memory>
#include <string>

namespace audio {

// Audio output built on miniaudio. If no audio device is available (CI, headless servers)
// the engine stays in a disabled state: every call is a safe no-op and ok() returns false.
class Engine {
  public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool ok() const;

    void setMasterVolume(float volume); // clamped to [0, 1]
    float masterVolume() const;

    // Fire-and-forget playback of a sound file. Returns false on failure.
    bool playFile(const std::string& path);

    // A continuously playing sine tone (handy for testing the audio path without assets).
    void setTone(bool enabled, float frequencyHz = 440.0f, float amplitude = 0.2f);
    bool toneEnabled() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace audio
