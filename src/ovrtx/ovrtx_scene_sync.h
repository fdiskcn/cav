/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "../base/occ_handle.h"
#include "ovrtx_usd_types.h"

class V3d_View;

namespace Mayo {

class GuiDocument;

namespace Ovrtx {

UsdScene collectSceneFromGuiDocument(const GuiDocument* guiDoc, int width, int height);
CameraState cameraFromV3dView(const OccHandle<V3d_View>& view, int width, int height);

} // namespace Ovrtx
} // namespace Mayo
