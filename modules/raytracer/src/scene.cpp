#include "raytracer/scene.h"

#include <algorithm>
#include <utility>

namespace rt {

int Scene::addMaterial(const Material& m) {
    m_materials.push_back(m);
    return static_cast<int>(m_materials.size()) - 1;
}

void Scene::addSphere(const Sphere& s) {
    m_spheres.push_back(s);
}

void Scene::addTriangle(const Triangle& t) {
    m_triangles.push_back(t);
}

void Scene::addQuad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, int material) {
    addTriangle({a, b, c, material});
    addTriangle({a, c, d, material});
}

Vec3 Scene::sky(const Vec3& dir) const {
    float t = 0.5f * (dir.y + 1.0f);
    return lerp(skyBottom, skyTop, t) * skyIntensity;
}

void Scene::Bounds::grow(const Vec3& p) {
    min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
    max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
}

void Scene::Bounds::grow(const Bounds& b) {
    grow(b.min);
    grow(b.max);
}

Scene::Bounds Scene::primBounds(uint32_t ref) const {
    Bounds b;
    if (ref & kTriangleBit) {
        const Triangle& t = m_triangles[ref & ~kTriangleBit];
        b.grow(t.v0);
        b.grow(t.v1);
        b.grow(t.v2);
    } else {
        const Sphere& s = m_spheres[ref];
        const Vec3 r{s.radius, s.radius, s.radius};
        b.grow(s.center - r);
        b.grow(s.center + r);
    }
    return b;
}

void Scene::build() {
    m_prims.clear();
    m_nodes.clear();
    for (uint32_t i = 0; i < m_spheres.size(); ++i)
        m_prims.push_back(i);
    for (uint32_t i = 0; i < m_triangles.size(); ++i)
        m_prims.push_back(i | kTriangleBit);
    if (m_prims.empty())
        return;
    m_nodes.reserve(m_prims.size() * 2);
    m_nodes.emplace_back();
    buildNode(0, 0, static_cast<uint32_t>(m_prims.size()));
}

// Fills m_nodes[nodeIndex] for the primitives m_prims[begin, end), splitting recursively.
void Scene::buildNode(uint32_t nodeIndex, uint32_t begin, uint32_t end) {
    Bounds bounds, centroids;
    for (uint32_t i = begin; i < end; ++i) {
        Bounds pb = primBounds(m_prims[i]);
        bounds.grow(pb);
        centroids.grow((pb.min + pb.max) * 0.5f);
    }
    m_nodes[nodeIndex].bmin = bounds.min;
    m_nodes[nodeIndex].bmax = bounds.max;

    const uint32_t n = end - begin;
    const Vec3 extent = centroids.max - centroids.min;
    const int axis =
        extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);

    if (n <= 4 || extent[axis] <= 0.0f) {
        m_nodes[nodeIndex].left = begin;
        m_nodes[nodeIndex].count = n;
        return;
    }

    const uint32_t mid = begin + n / 2;
    std::nth_element(m_prims.begin() + begin, m_prims.begin() + mid, m_prims.begin() + end,
                     [&](uint32_t a, uint32_t b) {
                         Bounds ba = primBounds(a), bb = primBounds(b);
                         return (ba.min[axis] + ba.max[axis]) < (bb.min[axis] + bb.max[axis]);
                     });

    const uint32_t first = static_cast<uint32_t>(m_nodes.size());
    m_nodes.emplace_back();
    m_nodes.emplace_back();
    m_nodes[nodeIndex].left = first;
    m_nodes[nodeIndex].count = 0;
    buildNode(first, begin, mid);
    buildNode(first + 1, mid, end);
}

bool Scene::intersectPrim(uint32_t ref, const Ray& ray, float tMin, float tMax, Hit& hit) const {
    float t;
    Vec3 outward;
    int mat;
    if (ref & kTriangleBit) {
        // Moller-Trumbore.
        const Triangle& tri = m_triangles[ref & ~kTriangleBit];
        const Vec3 e1 = tri.v1 - tri.v0, e2 = tri.v2 - tri.v0;
        const Vec3 p = cross(ray.direction, e2);
        const float det = dot(e1, p);
        if (std::fabs(det) < 1e-9f)
            return false;
        const float inv = 1.0f / det;
        const Vec3 s = ray.origin - tri.v0;
        const float u = dot(s, p) * inv;
        if (u < 0.0f || u > 1.0f)
            return false;
        const Vec3 q = cross(s, e1);
        const float v = dot(ray.direction, q) * inv;
        if (v < 0.0f || u + v > 1.0f)
            return false;
        t = dot(e2, q) * inv;
        outward = normalize(cross(e1, e2));
        mat = tri.material;
    } else {
        const Sphere& sp = m_spheres[ref];
        const Vec3 oc = ray.origin - sp.center;
        const float b = dot(oc, ray.direction);
        const float c = dot(oc, oc) - sp.radius * sp.radius;
        const float disc = b * b - c; // direction is unit length
        if (disc < 0.0f)
            return false;
        const float sq = std::sqrt(disc);
        t = -b - sq;
        if (t < tMin || t > tMax)
            t = -b + sq;
        outward = (ray.at(t) - sp.center) / sp.radius;
        mat = sp.material;
    }
    if (t < tMin || t > tMax)
        return false;

    hit.t = t;
    hit.point = ray.at(t);
    hit.frontFace = dot(ray.direction, outward) < 0.0f;
    hit.normal = hit.frontFace ? outward : -outward;
    hit.material = mat;
    return true;
}

bool Scene::intersectBruteForce(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    bool any = false;
    for (uint32_t i = 0; i < m_spheres.size(); ++i)
        if (intersectPrim(i, ray, tMin, tMax, hit)) {
            any = true;
            tMax = hit.t;
        }
    for (uint32_t i = 0; i < m_triangles.size(); ++i)
        if (intersectPrim(i | kTriangleBit, ray, tMin, tMax, hit)) {
            any = true;
            tMax = hit.t;
        }
    return any;
}

namespace {
// Slab test; returns true if the ray overlaps the box within [tMin, tMax].
bool hitBox(const Vec3& bmin, const Vec3& bmax, const Ray& ray, const Vec3& invDir, float tMin,
            float tMax) {
    for (int a = 0; a < 3; ++a) {
        float t0 = (bmin[a] - ray.origin[a]) * invDir[a];
        float t1 = (bmax[a] - ray.origin[a]) * invDir[a];
        if (t0 > t1)
            std::swap(t0, t1);
        tMin = t0 > tMin ? t0 : tMin;
        tMax = t1 < tMax ? t1 : tMax;
        if (tMax < tMin)
            return false;
    }
    return true;
}
} // namespace

bool Scene::intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    if (m_nodes.empty())
        return false;

    const Vec3 invDir{1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z};
    uint32_t stack[64];
    int sp = 0;
    stack[sp++] = 0;
    bool any = false;

    while (sp > 0) {
        const Node& node = m_nodes[stack[--sp]];
        if (!hitBox(node.bmin, node.bmax, ray, invDir, tMin, tMax))
            continue;
        if (node.count > 0) {
            for (uint32_t i = 0; i < node.count; ++i)
                if (intersectPrim(m_prims[node.left + i], ray, tMin, tMax, hit)) {
                    any = true;
                    tMax = hit.t;
                }
        } else {
            stack[sp++] = node.left;
            stack[sp++] = node.left + 1;
        }
    }
    return any;
}

} // namespace rt
