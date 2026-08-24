/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "ovrtx_usd_types.h"

namespace Mayo {
namespace Ovrtx {

// Build a USD camera-to-world matrix (row-major, translation in last row).
// Camera looks along local -Z with +Y up.
Mat4d lookAtCameraToWorld(Vec3f eye, Vec3f center, Vec3f up);

// Horizontal aperture in millimetres (USD default 35mm-film width).
inline constexpr float kUsdHorizontalApertureMm = 20.955f;

float horizontalFovDegrees(float fovYDegrees, float aspect);
float focalLengthMm(float fovYDegrees, float aspect, float horizontalApertureMm = kUsdHorizontalApertureMm);
float verticalApertureMm(float aspect, float horizontalApertureMm = kUsdHorizontalApertureMm);

} // namespace Ovrtx
} // namespace Mayo
