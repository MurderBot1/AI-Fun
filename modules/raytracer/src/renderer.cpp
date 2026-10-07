#include "raytracer/renderer.h"

#include <algorithm>
#include <chrono>

namespace rt {

std::shared_ptr<Scene> makeDemoScene() {
    auto scene = std::make_shared<Scene>();

    Material ground;
    ground.albedo = {0.45f, 0.5f, 0.45f};
    const int groundMat = scene->addMaterial(ground);

    Material diffuse;
    diffuse.albedo = {0.8f, 0.25f, 0.2f};
    const int diffuseMat = scene->addMaterial(diffuse);

    Material metal;
    metal.type = MaterialType::Metal;
    metal.albedo = {0.85f, 0.75f, 0.5f};
    metal.roughness = 0.05f;
    const int metalMat = scene->addMaterial(metal);

    Material glass;
    glass.type = MaterialType::Dielectric;
    glass.ior = 1.5f;
    const int glassMat = scene->addMaterial(glass);

    Material light;
    light.type = MaterialType::Emissive;
    light.emission = {12.0f, 11.0f, 9.0f};
    const int lightMat = scene->addMaterial(light);

    scene->addQuad({-30, 0, -30}, {-30, 0, 30}, {30, 0, 30}, {30, 0, -30}, groundMat);
    scene->addSphere({{-2.2f, 1.0f, 0.0f}, 1.0f, diffuseMat});
    scene->addSphere({{0.0f, 1.0f, 0.0f}, 1.0f, glassMat});
    scene->addSphere({{2.2f, 1.0f, 0.0f}, 1.0f, metalMat});
    scene->addSphere({{0.0f, 6.0f, -1.0f}, 1.5f, lightMat});

    Rng rng(42);
    for (int i = 0; i < 40; ++i) {
        const float x = rng.nextFloat() * 14.0f - 7.0f;
        const float z = rng.nextFloat() * 10.0f - 7.0f;
        if (std::fabs(x) < 3.5f && std::fabs(z) < 1.6f)
            continue;
        Material m;
        m.albedo = {rng.nextFloat(), rng.nextFloat(), rng.nextFloat()};
        if (rng.nextFloat() < 0.35f) {
            m.type = MaterialType::Metal;
            m.roughness = rng.nextFloat() * 0.4f;
        }
        scene->addSphere({{x, 0.25f, z}, 0.25f, scene->addMaterial(m)});
    }

    scene->build();
    return scene;
}

Renderer::Renderer(int width, int height, int threads)
    : m_width(std::max(1, width)), m_height(std::max(1, height)) {
    m_threads =
        threads > 0 ? threads : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    m_accum.assign(static_cast<size_t>(m_width) * m_height, Vec3{});
    m_frame.assign(static_cast<size_t>(m_width) * m_height * 4, 0);
}

Renderer::~Renderer() {
    stop();
}

void Renderer::setScene(std::shared_ptr<const Scene> scene) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.scene = std::move(scene);
    m_dirty = true;
}

void Renderer::setCamera(const Camera& camera) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.camera = camera;
    m_dirty = true;
}

void Renderer::setMaxDepth(int depth) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.maxDepth = std::max(1, depth);
    m_dirty = true;
}

void Renderer::setSampleLimit(uint32_t limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.sampleLimit = limit; // does not restart accumulation
}

namespace {
float schlick(float cosine, float ior) {
    float r0 = (1.0f - ior) / (1.0f + ior);
    r0 *= r0;
    return r0 + (1.0f - r0) * std::pow(1.0f - cosine, 5.0f);
}
} // namespace

Vec3 Renderer::trace(const Scene& scene, Ray ray, int maxDepth, Rng& rng) const {
    Vec3 radiance{0, 0, 0};
    Vec3 throughput{1, 1, 1};

    for (int depth = 0; depth < maxDepth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, 1e-3f, 1e30f, hit)) {
            radiance += throughput * scene.sky(ray.direction);
            break;
        }

        const Material& mat = scene.material(hit.material);
        radiance += throughput * mat.emission;

        switch (mat.type) {
        case MaterialType::Emissive:
            return radiance;
        case MaterialType::Lambertian: {
            Vec3 dir = hit.normal + rng.unitVector();
            if (dot(dir, dir) < 1e-8f)
                dir = hit.normal;
            ray = {hit.point, normalize(dir)};
            throughput = throughput * mat.albedo;
            break;
        }
        case MaterialType::Metal: {
            Vec3 dir = reflect(ray.direction, hit.normal) + rng.inUnitSphere() * mat.roughness;
            if (dot(dir, hit.normal) <= 0.0f)
                return radiance; // scattered into the surface: absorbed
            ray = {hit.point, normalize(dir)};
            throughput = throughput * mat.albedo;
            break;
        }
        case MaterialType::Dielectric: {
            const float ratio = hit.frontFace ? 1.0f / mat.ior : mat.ior;
            const float cosT = std::min(dot(-ray.direction, hit.normal), 1.0f);
            const float sinT = std::sqrt(std::max(0.0f, 1.0f - cosT * cosT));
            Vec3 dir;
            if (ratio * sinT > 1.0f || schlick(cosT, ratio) > rng.nextFloat()) {
                dir = reflect(ray.direction, hit.normal);
            } else {
                const Vec3 perp = (ray.direction + hit.normal * cosT) * ratio;
                const Vec3 par = hit.normal * -std::sqrt(std::fabs(1.0f - dot(perp, perp)));
                dir = perp + par;
            }
            ray = {hit.point, normalize(dir)};
            break;
        }
        }

        // Russian roulette keeps long paths cheap without biasing the result.
        if (depth >= 3) {
            const float p = std::min(0.95f, std::max(0.05f, maxComponent(throughput)));
            if (rng.nextFloat() > p)
                break;
            throughput *= 1.0f / p;
        }
    }
    return radiance;
}

void Renderer::renderRows(const Scene& scene, const Camera& camera, int maxDepth,
                          uint32_t sampleIndex, std::atomic<int>& nextRow) {
    const float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    for (int y = nextRow++; y < m_height; y = nextRow++) {
        for (int x = 0; x < m_width; ++x) {
            const uint64_t pixel = static_cast<uint64_t>(y) * m_width + x;
            Rng rng(pixel * 0x9E3779B97F4A7C15ULL + sampleIndex, pixel);
            const float u = (x + rng.nextFloat()) / m_width;
            const float v = 1.0f - (y + rng.nextFloat()) / m_height; // image row 0 is the top
            Vec3 c = trace(scene, camera.getRay(u, v, aspect, rng), maxDepth, rng);
            // Clamp fireflies and reject NaNs from degenerate paths.
            if (!(c.x == c.x && c.y == c.y && c.z == c.z))
                c = {0, 0, 0};
            const float m = maxComponent(c);
            if (m > 12.0f)
                c = c * (12.0f / m);
            m_accum[pixel] += c;
        }
    }
}

bool Renderer::step() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active.sampleLimit = m_pending.sampleLimit;
        if (m_dirty) {
            m_active = m_pending;
            m_dirty = false;
            std::fill(m_accum.begin(), m_accum.end(), Vec3{});
            m_samples = 0;
        }
    }
    if (!m_active.scene)
        return false;
    if (m_active.sampleLimit != 0 && m_samples >= m_active.sampleLimit)
        return false;

    std::atomic<int> nextRow{0};
    const int workers = std::min(m_threads, m_height);
    std::vector<std::thread> pool;
    for (int i = 1; i < workers; ++i)
        pool.emplace_back([&] {
            renderRows(*m_active.scene, m_active.camera, m_active.maxDepth, m_samples, nextRow);
        });
    renderRows(*m_active.scene, m_active.camera, m_active.maxDepth, m_samples, nextRow);
    for (auto& t : pool)
        t.join();

    ++m_samples;
    publish();
    return true;
}

void Renderer::publish() {
    const float inv = 1.0f / static_cast<float>(m_samples);
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < m_accum.size(); ++i) {
        const Vec3 c = m_accum[i] * inv;
        for (int ch = 0; ch < 3; ++ch) {
            const float v = std::min(1.0f, std::max(0.0f, c[ch]));
            m_frame[i * 4 + ch] = static_cast<uint8_t>(std::pow(v, 1.0f / 2.2f) * 255.0f + 0.5f);
        }
        m_frame[i * 4 + 3] = 255;
    }
    m_publishedSamples = m_samples;
}

uint32_t Renderer::copyFrame(std::vector<uint8_t>& rgba) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    rgba = m_frame;
    return m_publishedSamples.load();
}

void Renderer::start() {
    if (m_running.exchange(true))
        return;
    m_thread = std::thread([this] {
        while (m_running) {
            if (!step())
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });
}

void Renderer::stop() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

} // namespace rt
