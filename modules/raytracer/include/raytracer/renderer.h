#pragma once

#include "raytracer/camera.h"
#include "raytracer/scene.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rt {

// Builds the default showcase scene (ground, glass/metal/diffuse spheres, a light).
std::shared_ptr<Scene> makeDemoScene();

// Progressive CPU path tracer. Each step() adds one sample per pixel to an accumulation buffer;
// start() runs step() on a background thread so a UI can poll copyFrame().
class Renderer {
  public:
    Renderer(int width, int height, int threads = 0);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    int width() const { return m_width; }
    int height() const { return m_height; }

    // Thread-safe; any change restarts accumulation at the next step.
    void setScene(std::shared_ptr<const Scene> scene);
    void setCamera(const Camera& camera);
    void setMaxDepth(int depth);
    // Stop accumulating once this many samples exist (0 = unlimited).
    void setSampleLimit(uint32_t limit);

    // Renders one sample per pixel (blocking). Returns false if idle (limit reached / no scene).
    bool step();

    void start();
    void stop();

    // Copies the current tonemapped RGBA8 image (width*height*4 bytes). Returns sample count.
    uint32_t copyFrame(std::vector<uint8_t>& rgba) const;
    uint32_t sampleCount() const { return m_publishedSamples.load(); }
    // Increments each time accumulation restarts (camera/scene/depth change) and a frame from the
    // new state has been published. Lets callers tell a fresh frame from a stale sample count.
    uint32_t publishedGeneration() const { return m_publishedGeneration.load(); }

  private:
    struct Params {
        std::shared_ptr<const Scene> scene;
        Camera camera;
        int maxDepth = 8;
        uint32_t sampleLimit = 0;
    };

    Vec3 trace(const Scene& scene, Ray ray, int maxDepth, Rng& rng) const;
    void renderRows(const Scene& scene, const Camera& camera, int maxDepth, uint32_t sampleIndex,
                    std::atomic<int>& nextRow);
    void publish();

    int m_width, m_height, m_threads;

    mutable std::mutex m_mutex; // guards m_pending, m_dirty, m_frame
    Params m_pending;
    bool m_dirty = true;
    std::vector<uint8_t> m_frame;

    // Worker-side state (only touched from step()).
    Params m_active;
    std::vector<Vec3> m_accum;
    uint32_t m_samples = 0;
    uint32_t m_generation = 0;

    std::atomic<uint32_t> m_publishedSamples{0};
    std::atomic<uint32_t> m_publishedGeneration{0};
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace rt
