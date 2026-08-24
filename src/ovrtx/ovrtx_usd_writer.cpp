/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_usd_writer.h"
#include "ovrtx_camera.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Mayo {
namespace Ovrtx {

namespace {

void writeV3(std::ostringstream& os, Vec3f v)
{
    os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}

void writePointArray(std::ostringstream& os, const std::vector<Vec3f>& pts)
{
    os << '[';
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i)
            os << ", ";
        writeV3(os, pts[i]);
    }
    os << ']';
}

void writeIntArray(std::ostringstream& os, const std::vector<int>& values)
{
    os << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            os << ", ";
        os << values[i];
    }
    os << ']';
}

void writeMat(std::ostringstream& os, const Mat4d& m)
{
    os << '(';
    for (int r = 0; r < 4; ++r) {
        if (r)
            os << ", ";
        os << '(';
        for (int c = 0; c < 4; ++c) {
            if (c)
                os << ", ";
            os << m.m[static_cast<size_t>(r * 4 + c)];
        }
        os << ')';
    }
    os << ')';
}

Vec3f bboxMin(const std::vector<Vec3f>& pts)
{
    Vec3f m{ 1e30f, 1e30f, 1e30f };
    for (const Vec3f& p : pts) {
        m.x = std::min(m.x, p.x);
        m.y = std::min(m.y, p.y);
        m.z = std::min(m.z, p.z);
    }
    return m;
}

Vec3f bboxMax(const std::vector<Vec3f>& pts)
{
    Vec3f m{ -1e30f, -1e30f, -1e30f };
    for (const Vec3f& p : pts) {
        m.x = std::max(m.x, p.x);
        m.y = std::max(m.y, p.y);
        m.z = std::max(m.z, p.z);
    }
    return m;
}

} // namespace

std::string writeUsda(const UsdScene& scene)
{
    const CameraState& cam = scene.camera;
    const int width = std::max(1, cam.width);
    const int height = std::max(1, cam.height);
    const float aspect = float(width) / float(height);
    const Mat4d camXform = lookAtCameraToWorld(cam.eye, cam.center, cam.up);
    const float focal = focalLengthMm(cam.fovYDegrees, aspect);
    const float hAperture = cam.orthographic
        ? std::max(cam.orthoHeight, 0.001f) * aspect
        : kUsdHorizontalApertureMm;
    const float vAperture = cam.orthographic
        ? std::max(cam.orthoHeight, 0.001f)
        : verticalApertureMm(aspect);

    std::ostringstream os;
    os << std::setprecision(9);
    os << "#usda 1.0\n(\n";
    os << "    defaultPrim = \"World\"\n";
    os << "    metersPerUnit = 0.001\n";
    os << "    upAxis = \"Y\"\n";
    os << ")\n\n";

    os << "def Xform \"World\"\n{\n";
    os << "    def Camera \"Camera\" (\n";
    os << "        prepend apiSchemas = [\"OmniRtxCameraAutoExposureAPI_1\", \"OmniRtxCameraExposureAPI_1\"]\n";
    os << "    )\n    {\n";
    os << "        token projection = \"" << (cam.orthographic ? "orthographic" : "perspective") << "\"\n";
    os << "        float focalLength = " << focal << "\n";
    os << "        float horizontalAperture = " << hAperture << "\n";
    os << "        float verticalAperture = " << vAperture << "\n";
    os << "        float2 clippingRange = (" << std::max(cam.zNear, 0.001f) << ", " << cam.zFar << ")\n";
    os << "        float fStop = 0\n";
    os << "        bool omni:rtx:autoExposure:enabled = 1\n";
    os << "        matrix4d xformOp:transform = ";
    writeMat(os, camXform);
    os << "\n";
    os << "        uniform token[] xformOpOrder = [\"xformOp:transform\"]\n";
    os << "    }\n\n";

    os << "    def DomeLight \"Sky\"\n    {\n";
    os << "        float inputs:intensity = 800\n";
    os << "        color3f inputs:color = (0.35, 0.35, 0.38)\n";
    os << "    }\n\n";

    os << "    def DistantLight \"KeyLight\"\n    {\n";
    os << "        float inputs:intensity = 2500\n";
    os << "        float inputs:angle = 3\n";
    os << "        color3f inputs:color = (1, 0.98, 0.94)\n";
    os << "        double3 xformOp:rotateXYZ = (-35, 40, 0)\n";
    os << "        uniform token[] xformOpOrder = [\"xformOp:rotateXYZ\"]\n";
    os << "    }\n\n";

    os << "    def Xform \"Geometry\"\n    {\n";
    int index = 0;
    for (const UsdMesh& mesh : scene.meshes) {
        if (mesh.points.empty() || mesh.faceVertexIndices.size() < 3)
            continue;
        const std::string prim = mesh.primName.empty()
            ? sanitizePrimName("Mesh", index)
            : mesh.primName;
        const int triCount = static_cast<int>(mesh.faceVertexIndices.size() / 3);
        std::vector<int> counts(static_cast<size_t>(std::max(triCount, 0)), 3);

        os << "        def Mesh \"" << prim << "\"\n        {\n";
        os << "            bool doubleSided = 1\n";
        os << "            uniform token subdivisionScheme = \"none\"\n";
        if (!mesh.visible)
            os << "            token visibility = \"invisible\"\n";
        os << "            point3f[] points = ";
        writePointArray(os, mesh.points);
        os << "\n";
        os << "            int[] faceVertexCounts = ";
        writeIntArray(os, counts);
        os << "\n";
        os << "            int[] faceVertexIndices = ";
        writeIntArray(os, mesh.faceVertexIndices);
        os << "\n";
        if (mesh.normals.size() == mesh.points.size()) {
            os << "            normal3f[] normals = ";
            writePointArray(os, mesh.normals);
            os << " (\n                interpolation = \"vertex\"\n            )\n";
        }
        os << "            color3f[] primvars:displayColor = [";
        writeV3(os, mesh.displayColor);
        os << "]\n";
        os << "            float3[] extent = [";
        writeV3(os, bboxMin(mesh.points));
        os << ", ";
        writeV3(os, bboxMax(mesh.points));
        os << "]\n";
        os << "        }\n";
        ++index;
    }
    os << "    }\n";
    os << "}\n\n";

    os << "def Scope \"Render\"\n{\n";
    os << "    def Scope \"Vars\"\n    {\n";
    os << "        def RenderVar \"LdrColor\"\n        {\n";
    os << "            uniform string sourceName = \"LdrColor\"\n";
    os << "        }\n";
    os << "    }\n";
    os << "    def Scope \"Products\"\n    {\n";
    os << "        def RenderProduct \"MainCam\"\n        {\n";
    os << "            rel camera = </World/Camera>\n";
    os << "            rel orderedVars = </Render/Vars/LdrColor>\n";
    os << "            uniform int2 resolution = (" << width << ", " << height << ")\n";
    os << "        }\n";
    os << "    }\n";
    os << "}\n";

    return os.str();
}

} // namespace Ovrtx
} // namespace Mayo
