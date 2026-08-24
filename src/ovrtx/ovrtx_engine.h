/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "ovrtx_usd_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Mayo {
namespace Ovrtx {

struct RenderedFrame {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // tightly packed RGBA8, top-left origin after copy
};

// Owns an ovrtx renderer + ovstage instance. Fails gracefully when no RTX GPU
// is present: initialize() returns false and lastError() explains why.
class OvrtxEngine {
public:
    OvrtxEngine();
    ~OvrtxEngine();

    OvrtxEngine(const OvrtxEngine&) = delete;
    OvrtxEngine& operator=(const OvrtxEngine&) = delete;

    bool initialize();
    void shutdown();
    bool isReady() const { return m_ready; }
    const std::string& lastError() const { return m_lastError; }

    bool loadScene(const UsdScene& scene);
    bool updateCamera(const CameraState& camera);
    bool renderFrame(RenderedFrame* outFrame);

private:
    bool waitRendererOp(uint64_t opIndex, const char* what);
    bool waitStagePopulate(uint64_t opIndex, const char* what);
    bool advanceWriteFloor();
    bool stepAndReadback(RenderedFrame* outFrame);

    void* m_renderer = nullptr; // ovrtx_renderer_t*
    void* m_stage = nullptr;    // ovstage_instance_t*
    uint64_t m_ordinal = 1;
    bool m_attached = false;
    bool m_ready = false;
    bool m_stageInitialized = false;
    int m_width = 0;
    int m_height = 0;
    std::string m_lastError;
};

} // namespace Ovrtx
} // namespace Mayo
