/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_engine.h"
#include "ovrtx_camera.h"
#include "ovrtx_usd_writer.h"

#include <ovrtx/ovrtx.h>
#include <ovrtx/ovrtx_attributes.h>
#include <ovrtx/ovrtx_config.h>
#include <ovrtx/ovrtx_types.h>
#include <ovstage/ovstage.h>

#include <algorithm>
#include <cstring>
#include <string_view>

namespace Mayo {
namespace Ovrtx {

namespace {

ovrtx_renderer_t* asRenderer(void* p)
{
    return static_cast<ovrtx_renderer_t*>(p);
}

ovstage_instance_t* asStage(void* p)
{
    return static_cast<ovstage_instance_t*>(p);
}

bool isRendererError(ovrtx_result_t r)
{
    return r.status == OVRTX_API_ERROR;
}

template<typename ResultT>
bool isEnqueueError(ResultT r)
{
    return r.status == OVRTX_API_ERROR;
}

std::string ovrtxError(std::string_view what)
{
    const ovx_string_t err = ovrtx_get_last_error();
    std::string msg = "ovrtx ";
    msg.append(what);
    msg.append(" failed");
    if (err.ptr && err.length > 0) {
        msg.append(": ");
        msg.append(err.ptr, err.length);
    }
    return msg;
}

std::string ovstageError(ovstage_instance_t* stage, std::string_view what)
{
    ovx_string_t err = ovstage_population_get_last_error();
    if ((!err.ptr || err.length == 0) && stage)
        err = ovstage_get_last_error();
    std::string msg = "ovstage ";
    msg.append(what);
    msg.append(" failed");
    if (err.ptr && err.length > 0) {
        msg.append(": ");
        msg.append(err.ptr, err.length);
    }
    return msg;
}

ovrtx_render_var_output_handle_t findOutput(
        const ovrtx_render_product_set_outputs_t& outputs,
        const char* name)
{
    const size_t nlen = std::strlen(name);
    for (size_t i = 0; i < outputs.output_count; ++i) {
        const ovrtx_render_product_output_t& product = outputs.outputs[i];
        for (size_t f = 0; f < product.output_frame_count; ++f) {
            const ovrtx_render_product_frame_output_t& frame = product.output_frames[f];
            for (size_t v = 0; v < frame.render_var_count; ++v) {
                const ovrtx_render_product_render_var_output_t& var = frame.output_render_vars[v];
                if (var.render_var_name.ptr
                    && var.render_var_name.length == nlen
                    && std::strncmp(var.render_var_name.ptr, name, nlen) == 0) {
                    return var.output_handle;
                }
            }
        }
    }
    return static_cast<ovrtx_render_var_output_handle_t>(-1);
}

} // namespace

OvrtxEngine::OvrtxEngine() = default;

OvrtxEngine::~OvrtxEngine()
{
    this->shutdown();
}

bool OvrtxEngine::initialize()
{
    if (m_ready)
        return true;

    ovx_string_t ovrtxRoot{
        OVX_CONFIG_EXECUTABLE_DIR_TOKEN "/ovrtx",
        sizeof(OVX_CONFIG_EXECUTABLE_DIR_TOKEN "/ovrtx") - 1
    };
    ovrtx_config_entry_t rendererEntries[] = {
        ovrtx_config_entry_binary_package_root_path(ovrtxRoot)
    };
    ovrtx_config_t rendererConfig{};
    rendererConfig.entries = rendererEntries;
    rendererConfig.entry_count = 1;

    ovrtx_renderer_t* renderer = nullptr;
    ovrtx_result_t created = ovrtx_create_renderer(&rendererConfig, &renderer);
    if (isRendererError(created) || !renderer) {
        m_lastError = ovrtxError("create_renderer");
        return false;
    }
    m_renderer = renderer;

    ovx_string_t ovstageRoot{
        OVX_CONFIG_EXECUTABLE_DIR_TOKEN "/ovstage",
        sizeof(OVX_CONFIG_EXECUTABLE_DIR_TOKEN "/ovstage") - 1
    };
    ovstage_config_entry_t stageEntries[] = {
        ovstage_config_entry_binary_package_root_path(ovstageRoot)
    };
    ovstage_config_t stageConfig{};
    stageConfig.entries = stageEntries;
    stageConfig.entry_count = 1;
    const ovstage_api_status_t initStatus = ovstage_initialize(&stageConfig);
    if (initStatus != OVSTAGE_OK) {
        m_lastError = ovstageError(nullptr, "initialize");
        this->shutdown();
        return false;
    }
    m_stageInitialized = true;

    ovstage_instance_desc_t desc{};
    desc.name = "mayo";
    ovstage_instance_t* stage = nullptr;
    const ovstage_api_status_t instStatus = ovstage_create_instance(&desc, &stage);
    if (instStatus != OVSTAGE_OK || !stage) {
        m_lastError = ovstageError(stage, "create_instance");
        this->shutdown();
        return false;
    }
    m_stage = stage;

    const ovrtx_result_t attach = ovrtx_attach_ovstage(renderer, stage);
    if (isRendererError(attach)) {
        m_lastError = ovrtxError("attach_ovstage");
        this->shutdown();
        return false;
    }
    m_attached = true;
    m_ready = true;
    return true;
}

void OvrtxEngine::shutdown()
{
    m_ready = false;
    auto* renderer = asRenderer(m_renderer);
    auto* stage = asStage(m_stage);
    if (m_attached && renderer) {
        ovrtx_detach_ovstage(renderer);
        m_attached = false;
    }
    if (stage) {
        ovstage_destroy_instance(stage);
        m_stage = nullptr;
    }
    if (renderer) {
        ovrtx_destroy_renderer(renderer);
        m_renderer = nullptr;
    }
    if (m_stageInitialized) {
        ovstage_shutdown();
        m_stageInitialized = false;
    }
}

bool OvrtxEngine::waitRendererOp(uint64_t opIndex, const char* what)
{
    ovrtx_op_wait_result_t wait{};
    const ovrtx_result_t r = ovrtx_wait_op(
        asRenderer(m_renderer), opIndex, ovrtx_timeout_infinite, &wait
    );
    if (isRendererError(r)) {
        m_lastError = ovrtxError(what);
        return false;
    }
    if (wait.num_error_ops > 0) {
        m_lastError = ovrtxError(what);
        return false;
    }
    return true;
}

bool OvrtxEngine::waitStagePopulate(uint64_t opIndex, const char* what)
{
    ovstage_population_op_wait_result_t wait{};
    const ovstage_api_status_t st = ovstage_population_wait_op(
        asStage(m_stage), opIndex, OVSTAGE_TIMEOUT_INFINITE, &wait
    );
    if (st != OVSTAGE_OK || wait.error_op_id_count > 0) {
        m_lastError = ovstageError(asStage(m_stage), what);
        return false;
    }
    return true;
}

bool OvrtxEngine::advanceWriteFloor()
{
    ovstage_write_floor_desc_t desc{};
    desc.ordinal = m_ordinal;
    desc.scope = OVSTAGE_SCOPE_ALL;
    const ovstage_enqueue_result_t en = ovstage_advance_write_floor(asStage(m_stage), &desc);
    if (en.status != OVSTAGE_OK) {
        m_lastError = ovstageError(asStage(m_stage), "advance_write_floor");
        return false;
    }
    ovstage_op_wait_result_t wait{};
    const ovstage_api_status_t st = ovstage_wait_op(
        asStage(m_stage), en.op_index, OVSTAGE_TIMEOUT_INFINITE, &wait
    );
    ovstage_release_op(asStage(m_stage), en.op_index);
    if (st != OVSTAGE_OK || wait.error_op_id_count > 0) {
        m_lastError = ovstageError(asStage(m_stage), "wait advance_write_floor");
        return false;
    }
    return true;
}

bool OvrtxEngine::loadScene(const UsdScene& scene)
{
    if (!m_ready && !this->initialize())
        return false;

    const std::string usda = writeUsda(scene);
    m_width = std::max(1, scene.camera.width);
    m_height = std::max(1, scene.camera.height);
    ++m_ordinal;

    const ovstage_population_enqueue_result_t pop = ovstage_population_open_usd_from_string(
        asStage(m_stage),
        { usda.c_str(), usda.size() },
        m_ordinal,
        /*time=*/0.0,
        OVSTAGE_POPULATION_DOMAIN_RENDERING
    );
    if (pop.status != OVSTAGE_OK) {
        m_lastError = ovstageError(asStage(m_stage), "open_usd_from_string");
        return false;
    }
    if (!this->waitStagePopulate(pop.op_index, "open_usd_from_string"))
        return false;
    if (!this->advanceWriteFloor())
        return false;

    const ovrtx_enqueue_result_t upd = ovrtx_update_from_stage(asRenderer(m_renderer), m_ordinal);
    if (isEnqueueError(upd)) {
        m_lastError = ovrtxError("update_from_stage");
        return false;
    }
    return this->waitRendererOp(upd.op_index, "update_from_stage");
}

bool OvrtxEngine::updateCamera(const CameraState& camera)
{
    if (!m_ready)
        return false;

    const Mat4d mat = lookAtCameraToWorld(camera.eye, camera.center, camera.up);
    ovrtx_xform_matrix44d_t xform{};
    for (int i = 0; i < 16; ++i)
        xform.v[i] = mat.m[static_cast<size_t>(i)];

    const ovx_string_t path{ kCameraPrimPath, std::strlen(kCameraPrimPath) };
    const ovrtx_enqueue_result_t en = ovrtx_set_xform_mat(
        asRenderer(m_renderer), &path, 1, &xform
    );
    if (isEnqueueError(en)) {
        m_lastError = ovrtxError("set_xform_mat");
        return false;
    }
    return this->waitRendererOp(en.op_index, "set_xform_mat");
}

bool OvrtxEngine::stepAndReadback(RenderedFrame* outFrame)
{
    ovx_string_t product{ kRenderProductPath, std::strlen(kRenderProductPath) };
    ovrtx_render_product_set_t products{};
    products.render_products = &product;
    products.num_render_products = 1;

    ovrtx_step_result_handle_t stepHandle = 0;
    const ovrtx_enqueue_result_t step = ovrtx_step_with_stage(
        asRenderer(m_renderer), products, 1.0 / 60.0, m_ordinal, &stepHandle
    );
    if (isEnqueueError(step)) {
        m_lastError = ovrtxError("step_with_stage");
        return false;
    }
    if (!this->waitRendererOp(step.op_index, "step_with_stage")) {
        ovrtx_destroy_results(asRenderer(m_renderer), stepHandle);
        return false;
    }

    ovrtx_render_product_set_outputs_t outputs{};
    const ovrtx_result_t fetched = ovrtx_fetch_results(
        asRenderer(m_renderer), stepHandle, ovrtx_timeout_infinite, &outputs
    );
    if (isRendererError(fetched)) {
        m_lastError = ovrtxError("fetch_results");
        ovrtx_destroy_results(asRenderer(m_renderer), stepHandle);
        return false;
    }

    const ovrtx_render_var_output_handle_t ldr = findOutput(outputs, "LdrColor");
    if (ldr == static_cast<ovrtx_render_var_output_handle_t>(-1)) {
        m_lastError = "LdrColor output not found";
        ovrtx_destroy_results(asRenderer(m_renderer), stepHandle);
        return false;
    }

    ovrtx_map_output_description_t mapDesc{};
    mapDesc.device_type = OVRTX_MAP_DEVICE_TYPE_CPU;
    ovrtx_render_var_output_t mapped{};
    const ovrtx_result_t mappedR = ovrtx_map_render_var_output(
        asRenderer(m_renderer), ldr, &mapDesc, ovrtx_timeout_infinite, &mapped
    );
    if (isRendererError(mappedR) || mapped.num_tensors != 1 || !mapped.tensors || !mapped.tensors[0].dl) {
        m_lastError = ovrtxError("map_render_var_output");
        ovrtx_destroy_results(asRenderer(m_renderer), stepHandle);
        return false;
    }

    const DLTensor& tensor = *mapped.tensors[0].dl;
    if (tensor.ndim != 3 || !tensor.shape || !tensor.data) {
        m_lastError = "Unexpected LdrColor tensor layout";
        ovrtx_cuda_sync_t none{};
        ovrtx_unmap_render_var_output(asRenderer(m_renderer), mapped.map_handle, none);
        ovrtx_destroy_results(asRenderer(m_renderer), stepHandle);
        return false;
    }

    const int h = static_cast<int>(tensor.shape[0]);
    const int w = static_cast<int>(tensor.shape[1]);
    const int c = static_cast<int>(tensor.shape[2]);
    outFrame->width = w;
    outFrame->height = h;
    outFrame->rgba.resize(static_cast<size_t>(w * h * 4));
    const auto* src = static_cast<const uint8_t*>(tensor.data);
    if (c >= 4) {
        std::memcpy(outFrame->rgba.data(), src, outFrame->rgba.size());
    }
    else {
        for (int i = 0; i < w * h; ++i) {
            outFrame->rgba[static_cast<size_t>(i * 4 + 0)] = src[static_cast<size_t>(i * c + 0)];
            outFrame->rgba[static_cast<size_t>(i * 4 + 1)] = c > 1 ? src[static_cast<size_t>(i * c + 1)] : src[static_cast<size_t>(i * c)];
            outFrame->rgba[static_cast<size_t>(i * 4 + 2)] = c > 2 ? src[static_cast<size_t>(i * c + 2)] : src[static_cast<size_t>(i * c)];
            outFrame->rgba[static_cast<size_t>(i * 4 + 3)] = 255;
        }
    }

    ovrtx_cuda_sync_t none{};
    ovrtx_unmap_render_var_output(asRenderer(m_renderer), mapped.map_handle, none);
    ovrtx_destroy_results(asRenderer(m_renderer), stepHandle);
    return true;
}

bool OvrtxEngine::renderFrame(RenderedFrame* outFrame)
{
    if (!outFrame)
        return false;
    if (!m_ready && !this->initialize())
        return false;
    return this->stepAndReadback(outFrame);
}

} // namespace Ovrtx
} // namespace Mayo
