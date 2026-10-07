#pragma once

#include <cstdint>

// Drives the scripted camera for --demo. It lets the renderer finish a frame at the current
// camera position before moving on: first a longer "settle" on the opening view, then a short
// dwell at each following stop. A time cap keeps the demo moving on slow machines.
class DemoDirector {
  public:
    struct Config {
        uint32_t settleSamples = 8;  // samples to accumulate on the opening view
        double settleMaxSeconds = 6; // ...but never wait longer than this
        uint32_t stepSamples = 4;    // samples to accumulate at each later stop
        double stepMaxSeconds = 3;
        float yawStep = 0.15f; // radians per move
    };

    DemoDirector() = default;
    explicit DemoDirector(const Config& config) : m_config(config) {}

    // Call every frame. `samples`/`generation` are the renderer's published sample count and
    // generation. Returns true when the camera should move by yawStep() now.
    bool update(double dt, uint32_t samples, uint32_t generation) {
        m_dwell += dt;
        // A frame is only "fresh" once one from the current camera has been published; right after
        // a move the sample count still belongs to the previous camera.
        const bool fresh = generation > m_lastMoveGeneration;
        const uint32_t needed = m_first ? m_config.settleSamples : m_config.stepSamples;
        const double cap = m_first ? m_config.settleMaxSeconds : m_config.stepMaxSeconds;
        if ((fresh && samples >= needed) || m_dwell >= cap) {
            m_first = false;
            m_dwell = 0.0;
            m_lastMoveGeneration = generation;
            return true;
        }
        return false;
    }

    float yawStep() const { return m_config.yawStep; }

  private:
    Config m_config;
    bool m_first = true;
    double m_dwell = 0.0;
    uint32_t m_lastMoveGeneration = 0;
};
