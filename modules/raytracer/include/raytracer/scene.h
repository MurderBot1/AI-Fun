#pragma once

#include "raytracer/math.h"

#include <vector>

namespace rt {

enum class MaterialType { Lambertian, Metal, Dielectric, Emissive };

struct Material {
    MaterialType type = MaterialType::Lambertian;
    Vec3 albedo{0.8f, 0.8f, 0.8f};
    float roughness = 0.0f; // metal fuzz
    float ior = 1.5f;       // dielectric index of refraction
    Vec3 emission{0, 0, 0}; // emissive radiance
};

struct Sphere {
    Vec3 center;
    float radius = 1.0f;
    int material = 0;
};

struct Triangle {
    Vec3 v0, v1, v2;
    int material = 0;
};

struct Hit {
    float t = 0;
    Vec3 point;
    Vec3 normal; // always faces against the incoming ray
    bool frontFace = true;
    int material = 0;
};

class Scene {
  public:
    int addMaterial(const Material& m);
    void addSphere(const Sphere& s);
    void addTriangle(const Triangle& t);
    // Two triangles; corners given in order around the quad.
    void addQuad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, int material);

    // Must be called after the last add*() and before intersect().
    void build();

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const;
    // Brute-force reference used by the tests.
    bool intersectBruteForce(const Ray& ray, float tMin, float tMax, Hit& hit) const;

    const Material& material(int index) const { return m_materials[index]; }
    size_t primitiveCount() const { return m_spheres.size() + m_triangles.size(); }

    Vec3 skyTop{0.45f, 0.65f, 1.0f};
    Vec3 skyBottom{1.0f, 1.0f, 1.0f};
    float skyIntensity = 1.0f;

    Vec3 sky(const Vec3& dir) const;

  private:
    struct Node {
        Vec3 bmin, bmax;
        uint32_t left = 0;  // inner: index of left child (right = left + 1); leaf: first primitive
        uint32_t count = 0; // 0 for inner nodes
    };
    struct Bounds {
        Vec3 min{1e30f, 1e30f, 1e30f};
        Vec3 max{-1e30f, -1e30f, -1e30f};
        void grow(const Vec3& p);
        void grow(const Bounds& b);
    };

    // Primitive reference: top bit set = triangle, otherwise sphere.
    static constexpr uint32_t kTriangleBit = 0x80000000u;

    Bounds primBounds(uint32_t ref) const;
    bool intersectPrim(uint32_t ref, const Ray& ray, float tMin, float tMax, Hit& hit) const;
    void buildNode(uint32_t nodeIndex, uint32_t begin, uint32_t end);

    std::vector<Material> m_materials;
    std::vector<Sphere> m_spheres;
    std::vector<Triangle> m_triangles;
    std::vector<uint32_t> m_prims;
    std::vector<Node> m_nodes;
};

} // namespace rt
