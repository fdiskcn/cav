/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "ovrtx_usd_types.h"

#include <string>

namespace Mayo {
namespace Ovrtx {

// Serializes a CAD-derived scene to USDA that ovrtx/ovstage can load.
// Geometry is authored in Y-up. Camera, dome light and an RTX RenderProduct
// are always present so the viewport can render an empty document.
std::string writeUsda(const UsdScene& scene);

inline constexpr const char* kCameraPrimPath = "/World/Camera";
inline constexpr const char* kRenderProductPath = "/Render/Products/MainCam";
inline constexpr const char* kGeometryRootPath = "/World/Geometry";

} // namespace Ovrtx
} // namespace Mayo
