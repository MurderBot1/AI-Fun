#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Scripted camera sweep for --demo, treated as if it had been rendered in real time.
//
// The path tracer is far too slow to render a smooth orbit live, so the demo works in two phases:
//   1. Baking:  render every frame of the sweep, one camera position at a time, each fully
//               converged (nothing is recorded yet).
//   2. Playing: play the finished frames back at a fixed frame rate, so the orbit around the
//               centre is smooth and looks like a real-time render.
// The camera follows an eased (smooth-step) path so the swing starts and stops gently, which also
// makes the optional ping-pong playback turn around without a jerk.
class DemoDirector {
  public:
    enum class Phase { Baking, Playing, Done };

    struct Config {
        uint32_t targetSamples = 192; // samples a frame needs before it counts as converged
        uint32_t frames = 120;        // frames in the sweep (fps * duration)
        double fps = 24.0;            // playback frame rate
        float totalYaw = 0.9f;        // radians swept around the centre
        bool pingPong = false;        // play forward, then back
        double endHoldSeconds = 1.0;  // keep the last frame up this long before finishing
    };

    DemoDirector() = default;
    explicit DemoDirector(const Config& config) : m_config(sanitised(config)) {}

    Phase phase() const { return m_phase; }
    uint32_t frames() const { return m_config.frames; }
    uint32_t captured() const { return m_captured; }

    // Camera yaw offset (radians) for frame `frame`: eased from 0 to totalYaw.
    float yawFor(uint32_t frame) const {
        const double u = m_config.frames > 1
                             ? std::min(1.0, static_cast<double>(frame) / (m_config.frames - 1))
                             : 0.0;
        const double eased = 0.5 * (1.0 - std::cos(3.14159265358979323846 * u));
        return static_cast<float>(m_config.totalYaw * eased);
    }

    // Baking phase: call every loop with the renderer's published state. Returns true when the
    // current frame has converged and the caller must capture it now (as frame captured()-1) and
    // then, if phase() is still Baking, point the camera at yawFor(captured()).
    bool bakeUpdate(uint32_t samples, uint32_t generation) {
        if (m_phase != Phase::Baking)
            return false;
        // Right after a camera move the renderer's sample count still belongs to the previous
        // frame; only a frame from a newer generation counts.
        if (generation > m_lastCapturedGeneration && samples >= m_config.targetSamples) {
            ++m_captured;
            m_lastCapturedGeneration = generation;
            if (m_captured >= m_config.frames)
                m_phase = Phase::Playing;
            return true;
        }
        return false;
    }

    // Number of frames in one pass of the playback sequence.
    uint32_t playbackLength() const {
        return m_config.pingPong && m_config.frames > 2 ? 2 * m_config.frames - 2 : m_config.frames;
    }

    // Playing phase: advance the playback clock by dt seconds and return the frame to show.
    // Switches to Done once the sequence has played and the last frame has been held.
    uint32_t playbackFrame(double dt) {
        if (m_phase == Phase::Playing) {
            m_clock += dt;
            const double sequenceSeconds = playbackLength() / m_config.fps;
            if (m_clock >= sequenceSeconds + m_config.endHoldSeconds)
                m_phase = Phase::Done;
        }
        const uint32_t step =
            std::min<uint32_t>(static_cast<uint32_t>(m_clock * m_config.fps), playbackLength() - 1);
        return step < m_config.frames ? step : 2 * m_config.frames - 2 - step;
    }

  private:
    static Config sanitised(Config c) {
        c.frames = std::max<uint32_t>(c.frames, 2);
        c.fps = std::max(c.fps, 1.0);
        c.targetSamples = std::max<uint32_t>(c.targetSamples, 1);
        c.endHoldSeconds = std::max(c.endHoldSeconds, 0.0);
        return c;
    }

    Config m_config;
    Phase m_phase = Phase::Baking;
    uint32_t m_captured = 0;
    uint32_t m_lastCapturedGeneration = 0;
    double m_clock = 0.0;
};
