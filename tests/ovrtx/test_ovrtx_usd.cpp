/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_usd_types.h"
#include "ovrtx_camera.h"
#include "ovrtx_usd_writer.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace Mayo::Ovrtx;

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool cond, const char* what)
{
    if (cond) {
        ++g_passed;
        std::cout << "PASS  " << what << '\n';
    }
    else {
        ++g_failed;
        std::cout << "FAIL  " << what << '\n';
    }
}

bool approx(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

bool approx(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

bool contains(const std::string& hay, const char* needle)
{
    return hay.find(needle) != std::string::npos;
}

} // namespace

int main()
{
    {
        const Vec3f y = zUpToYUp({ 1.f, 2.f, 3.f });
        check(approx(y.x, 1.f) && approx(y.y, 3.f) && approx(y.z, -2.f),
              "zUpToYUp(1,2,3) -> (1,3,-2)");
    }

    check(sanitizePrimName("Part-01", 0) == "Part_01", "sanitizePrimName keeps alnum and maps dash");
    check(sanitizePrimName("$$$", 7) == "Mesh_7", "sanitizePrimName empty fallback");
    check(sanitizePrimName("9abc", 1) == "Mesh_1_9abc", "sanitizePrimName leading digit");
    check(sanitizePrimName("", 3) == "Mesh_3", "sanitizePrimName empty string");

    {
        const Mat4d m = lookAtCameraToWorld({ 0.f, 0.f, 5.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
        check(approx(m.m[0], 1.) && approx(m.m[5], 1.) && approx(m.m[10], 1.),
              "lookAt basis is identity for +Z eye");
        check(approx(m.m[12], 0.) && approx(m.m[13], 0.) && approx(m.m[14], 5.) && approx(m.m[15], 1.),
              "lookAt translation is last row");

        const double xlen = std::sqrt(m.m[0] * m.m[0] + m.m[1] * m.m[1] + m.m[2] * m.m[2]);
        const double ylen = std::sqrt(m.m[4] * m.m[4] + m.m[5] * m.m[5] + m.m[6] * m.m[6]);
        const double zlen = std::sqrt(m.m[8] * m.m[8] + m.m[9] * m.m[9] + m.m[10] * m.m[10]);
        const double xy = m.m[0] * m.m[4] + m.m[1] * m.m[5] + m.m[2] * m.m[6];
        const double yz = m.m[4] * m.m[8] + m.m[5] * m.m[9] + m.m[6] * m.m[10];
        const double zx = m.m[8] * m.m[0] + m.m[9] * m.m[1] + m.m[10] * m.m[2];
        check(approx(xlen, 1.) && approx(ylen, 1.) && approx(zlen, 1.), "lookAt axes unit length");
        check(approx(xy, 0.) && approx(yz, 0.) && approx(zx, 0.), "lookAt axes orthogonal");
    }

    {
        const Mat4d m = lookAtCameraToWorld({ 0.f, 5.f, 0.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
        const bool finite = std::isfinite(m.m[0]) && std::isfinite(m.m[5]) && std::isfinite(m.m[10]);
        check(finite, "lookAt with parallel up vector stays finite");
    }

    {
        const float focal = focalLengthMm(45.f, 1.f);
        const float expected = kUsdHorizontalApertureMm / (2.f * std::tan(0.5f * 45.f * 0.017453292519943295f));
        check(approx(focal, expected, 1e-3f), "focalLengthMm 45deg square");
        check(focal > 10.f && focal < 40.f, "focalLengthMm in plausible 35mm-film range");
    }

    {
        UsdScene scene;
        scene.camera.width = 640;
        scene.camera.height = 480;
        scene.camera.fovYDegrees = 45.f;
        const std::string usda = writeUsda(scene);
        check(contains(usda, "#usda 1.0"), "USDA header");
        check(contains(usda, "metersPerUnit = 0.001"), "millimetre stage units");
        check(contains(usda, "upAxis = \"Y\""), "Y-up stage");
        check(contains(usda, "def Camera \"Camera\""), "camera prim");
        check(contains(usda, "def DomeLight \"Sky\""), "dome light");
        check(contains(usda, "def DistantLight \"KeyLight\""), "key light");
        check(contains(usda, "def RenderProduct \"MainCam\""), "render product");
        check(contains(usda, "rel camera = </World/Camera>"), "product camera relationship");
        check(contains(usda, "sourceName = \"LdrColor\""), "LdrColor AOV");
        check(contains(usda, "int2 resolution = (640, 480)"), "product resolution");
        check(!contains(usda, "def Mesh"), "empty scene has no mesh");
    }

    {
        UsdScene scene;
        scene.camera.width = 320;
        scene.camera.height = 240;
        UsdMesh tri;
        tri.primName = "Triangle";
        tri.points = { { 0.f, 0.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f } };
        tri.faceVertexIndices = { 0, 1, 2 };
        tri.displayColor = { 0.2f, 0.4f, 0.6f };
        scene.meshes.push_back(tri);

        UsdMesh empty;
        empty.primName = "ShouldSkip";
        scene.meshes.push_back(empty);

        UsdMesh hidden;
        hidden.primName = "Hidden";
        hidden.points = { { 0.f, 0.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f } };
        hidden.faceVertexIndices = { 0, 1, 2 };
        hidden.visible = false;
        scene.meshes.push_back(hidden);

        const std::string usda = writeUsda(scene);
        check(contains(usda, "def Mesh \"Triangle\""), "triangle mesh prim");
        check(contains(usda, "int[] faceVertexIndices = [0, 1, 2]"), "triangle indices");
        check(contains(usda, "int[] faceVertexCounts = [3]"), "triangle counts");
        check(contains(usda, "color3f[] primvars:displayColor"), "displayColor authored");
        check(!contains(usda, "ShouldSkip"), "empty mesh omitted");
        check(contains(usda, "def Mesh \"Hidden\""), "hidden mesh present");
        check(contains(usda, "token visibility = \"invisible\""), "hidden mesh invisible");
    }

    std::cout << '\n' << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
