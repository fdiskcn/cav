/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Mayo {
namespace Ovrtx {

struct Vec3f {
    float x = 0;
    float y = 0;
    float z = 0;
};

struct Mat4d {
    // Row-major 4x4, USD/ovrtx convention: translation in last row (m[12], m[13], m[14]).
    std::array<double, 16> m = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
};

struct UsdMesh {
    std::string primName;
    std::vector<Vec3f> points;
    std::vector<int> faceVertexIndices;
    std::vector<Vec3f> normals; // vertex normals, optional
    Vec3f displayColor{ 0.82f, 0.78f, 0.65f };
    bool visible = true;
};

struct CameraState {
    Vec3f eye{ 0.f, 0.f, 5.f };
    Vec3f center{ 0.f, 0.f, 0.f };
    Vec3f up{ 0.f, 1.f, 0.f };
    float fovYDegrees = 45.f;
    float aspect = 16.f / 9.f;
    int width = 1280;
    int height = 720;
    bool orthographic = false;
    float orthoHeight = 100.f;
    float zNear = 0.1f;
    float zFar = 1.0e6f;
};

struct UsdScene {
    std::vector<UsdMesh> meshes;
    CameraState camera;
    Vec3f backgroundTop{ 0.31f, 0.31f, 0.35f };
    Vec3f backgroundBottom{ 0.38f, 0.38f, 0.42f };
};

Vec3f zUpToYUp(Vec3f p);
std::string sanitizePrimName(std::string_view raw, int fallbackIndex);

// Hash of geometry, materials, visibility and camera intrinsics (not eye/center/up).
// Used to decide whether ovstage must be re-populated versus a camera xform update.
uint64_t sceneGeometryDigest(const UsdScene& scene);

} // namespace Ovrtx
} // namespace Mayo
