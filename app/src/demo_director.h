#pragma once

#include <cstdint>

// Drives the scripted camera for --demo. A view is only *shown* once the renderer has converged on
// it (no progressive refinement on screen); the renderer then works on the next view out of
// sight while the finished one stays up.
//
// Per frame, call update(). When it returns true the caller must (1) copy the renderer's current
// frame to the screen and (2) move the camera by yawStep(). After the last view has been shown and
// held, finished() turns true and the demo should exit.
class DemoDirector {
  public:
    struct Config {
        uint32_t targetSamples = 256; // samples a view needs before it counts as converged
        double minHoldSeconds = 3.0;  // each finished view stays up at least this long
        uint32_t totalFrames = 4;     // finished views to show
        float yawStep = 0.2f;         // radians per camera move
    };

    DemoDirector() = default;
    explicit DemoDirector(const Config& config)
        : m_config(config), m_sinceShown(config.minHoldSeconds) {}

    bool update(double dt, uint32_t samples, uint32_t generation) {
        m_sinceShown += dt;
        if (m_shown >= m_config.totalFrames)
            return false;
        // Right after a camera move the renderer's sample count still belongs to the previous
        // view; only a frame from a newer generation counts.
        const bool fresh = generation > m_lastShownGeneration;
        if (fresh && samples >= m_config.targetSamples && m_sinceShown >= m_config.minHoldSeconds) {
            ++m_shown;
            m_sinceShown = 0.0;
            m_lastShownGeneration = generation;
            return true;
        }
        return false;
    }

    // True once every view has been shown and the last one has been held for minHoldSeconds.
    bool finished() const {
        return m_shown >= m_config.totalFrames && m_sinceShown >= m_config.minHoldSeconds;
    }

    uint32_t framesShown() const { return m_shown; }
    float yawStep() const { return m_config.yawStep; }

  private:
    Config m_config;
    uint32_t m_shown = 0;
    double m_sinceShown = 0.0;
    uint32_t m_lastShownGeneration = 0;
};
