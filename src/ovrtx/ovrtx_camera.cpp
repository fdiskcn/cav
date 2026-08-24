/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_camera.h"

#include <cmath>

namespace Mayo {
namespace Ovrtx {

namespace {

Vec3f sub(Vec3f a, Vec3f b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3f cross(Vec3f a, Vec3f b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(Vec3f a, Vec3f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3f normalize(Vec3f v)
{
    const float len2 = dot(v, v);
    if (len2 <= 1e-20f)
        return { 0.f, 0.f, 1.f };
    const float inv = 1.f / std::sqrt(len2);
    return { v.x * inv, v.y * inv, v.z * inv };
}

} // namespace

Mat4d lookAtCameraToWorld(Vec3f eye, Vec3f center, Vec3f up)
{
    // Camera +Z points away from the target (USD camera looks down local -Z).
    Vec3f zaxis = normalize(sub(eye, center));
    Vec3f xaxis = cross(up, zaxis);
    if (dot(xaxis, xaxis) <= 1e-12f) {
        const Vec3f alt = std::fabs(zaxis.y) > 0.9f ? Vec3f{ 1.f, 0.f, 0.f } : Vec3f{ 0.f, 1.f, 0.f };
        xaxis = cross(alt, zaxis);
    }
    xaxis = normalize(xaxis);
    const Vec3f yaxis = cross(zaxis, xaxis);

    Mat4d m;
    m.m = {
        xaxis.x, xaxis.y, xaxis.z, 0.,
        yaxis.x, yaxis.y, yaxis.z, 0.,
        zaxis.x, zaxis.y, zaxis.z, 0.,
        eye.x,   eye.y,   eye.z,   1.
    };
    return m;
}

float horizontalFovDegrees(float fovYDegrees, float aspect)
{
    const float vfov = fovYDegrees * 0.017453292519943295f;
    const float hfov = 2.f * std::atan(std::tan(vfov * 0.5f) * aspect);
    return hfov * 57.29577951308232f;
}

float focalLengthMm(float fovYDegrees, float aspect, float horizontalApertureMm)
{
    const float hfov = horizontalFovDegrees(fovYDegrees, aspect) * 0.017453292519943295f;
    const float half = std::tan(hfov * 0.5f);
    if (half <= 1e-8f)
        return 50.f;
    return horizontalApertureMm / (2.f * half);
}

float verticalApertureMm(float aspect, float horizontalApertureMm)
{
    if (aspect <= 1e-8f)
        return horizontalApertureMm;
    return horizontalApertureMm / aspect;
}

} // namespace Ovrtx
} // namespace Mayo
