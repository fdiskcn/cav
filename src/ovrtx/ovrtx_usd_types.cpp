/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_usd_types.h"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace Mayo {
namespace Ovrtx {

Vec3f zUpToYUp(Vec3f p)
{
    // OCCT Z-up (X right, Y forward, Z up) -> USD Y-up (X right, Y up, Z back).
    return { p.x, p.z, -p.y };
}

std::string sanitizePrimName(std::string_view raw, int fallbackIndex)
{
    std::string out;
    out.reserve(raw.size() + 8);
    for (char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_')
            out.push_back(c);
        else
            out.push_back('_');
    }
    while (!out.empty() && out.front() == '_')
        out.erase(out.begin());
    if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front())))
        out = "Mesh_" + std::to_string(fallbackIndex) + (out.empty() ? std::string() : ("_" + out));
    return out;
}

namespace {

uint64_t mixU64(uint64_t h, uint64_t v)
{
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

uint64_t mixF32(uint64_t h, float f)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    return mixU64(h, bits);
}

} // namespace

uint64_t sceneGeometryDigest(const UsdScene& scene)
{
    uint64_t h = mixU64(scene.meshes.size(), (uint64_t(scene.camera.width) << 32) | uint32_t(scene.camera.height));
    h = mixU64(h, scene.camera.orthographic ? 1 : 0);
    h = mixF32(h, scene.camera.fovYDegrees);
    h = mixF32(h, scene.camera.orthoHeight);
    h = mixF32(h, scene.camera.zNear);
    h = mixF32(h, scene.camera.zFar);
    h = mixF32(h, scene.camera.aspect);
    for (const UsdMesh& mesh : scene.meshes) {
        h = mixU64(h, mesh.points.size());
        h = mixU64(h, mesh.faceVertexIndices.size());
        h = mixU64(h, mesh.visible ? 1 : 0);
        h = mixF32(h, mesh.displayColor.x);
        h = mixF32(h, mesh.displayColor.y);
        h = mixF32(h, mesh.displayColor.z);
        if (!mesh.points.empty()) {
            h = mixF32(h, mesh.points.front().x);
            h = mixF32(h, mesh.points.front().y);
            h = mixF32(h, mesh.points.front().z);
            h = mixF32(h, mesh.points.back().x);
            h = mixF32(h, mesh.points.back().y);
            h = mixF32(h, mesh.points.back().z);
        }
        if (mesh.points.size() > 2) {
            const Vec3f& mid = mesh.points[mesh.points.size() / 2];
            h = mixF32(h, mid.x);
            h = mixF32(h, mid.y);
            h = mixF32(h, mid.z);
        }
    }
    return h;
}

} // namespace Ovrtx
} // namespace Mayo
