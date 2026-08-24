/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "widget_ovrtx_view.h"

#include "../gui/gui_document.h"
#include "../ovrtx/ovrtx_scene_sync.h"

#include <Aspect_NeutralWindow.hxx>
#include <Standard_Version.hxx>
#include <V3d_View.hxx>

#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>

#include <algorithm>
#include <cmath>

namespace Mayo {

namespace {

class OvrtxOccWindow : public Aspect_NeutralWindow {
public:
#if OCC_VERSION_HEX >= 0x070600
    double DevicePixelRatio() const override { return m_pixelRatio; }
#endif
    void SetDevicePixelRatio(double ratio) { m_pixelRatio = ratio; }

private:
    double m_pixelRatio = 1.;
};

} // namespace

WidgetOvrtxView::WidgetOvrtxView(const OccHandle<V3d_View>& view, QWidget* parent)
    : QWidget(parent),
      IWidgetOccView(view),
      m_engine(std::make_unique<Ovrtx::OvrtxEngine>())
{
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    this->setAutoFillBackground(false);
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setMinimumSize(64, 64);
    m_status = tr("Initializing NVIDIA ovrtx…");
}

WidgetOvrtxView* WidgetOvrtxView::create(const OccHandle<V3d_View>& view, QWidget* parent)
{
    return new WidgetOvrtxView(view, parent);
}

void WidgetOvrtxView::bindGuiDocument(GuiDocument* guiDoc)
{
    m_guiDoc = guiDoc;
    if (!m_guiDoc)
        return;

    m_guiDoc->signalGraphicsBoundingBoxChanged.connectSlot([=](const Bnd_Box&) {
        this->markSceneDirty();
    });
    m_guiDoc->signalNodesVisibilityChanged.connectSlot(
        [=](const GuiDocument::MapVisibilityByTreeNodeId&) {
            this->markSceneDirty();
        }
    );
    m_guiDoc->document()->signalEntityAdded.connectSlot([=](TreeNodeId) {
        this->markSceneDirty();
    });
    m_guiDoc->document()->signalEntityAboutToBeDestroyed.connectSlot([=](TreeNodeId) {
        this->markSceneDirty();
    });
    m_guiDoc->graphicsScene()->signalSelectionChanged.connectSlot([=] {
        this->markSceneDirty();
    });
    this->markSceneDirty();
}

void WidgetOvrtxView::markSceneDirty()
{
    m_sceneDirty = true;
    this->update();
}

void WidgetOvrtxView::ensureViewWindow()
{
    const OccHandle<V3d_View>& view = this->v3dView();
    if (view.IsNull())
        return;

    const int w = std::max(1, this->width());
    const int h = std::max(1, this->height());
    const double dpr = this->devicePixelRatioF();

    auto wnd = OccHandle<OvrtxOccWindow>::DownCast(view->Window());
    if (wnd.IsNull()) {
        wnd = new OvrtxOccWindow;
        wnd->SetVirtual(true);
        wnd->SetSize(w, h);
        wnd->SetDevicePixelRatio(dpr);
        view->SetWindow(wnd);
        if (!wnd->IsMapped())
            wnd->Map();
        view->MustBeResized();
        return;
    }

    int cw = 0;
    int ch = 0;
    wnd->Size(cw, ch);
    const bool sizeChanged = cw != w || ch != h;
#if OCC_VERSION_HEX >= 0x070600
    const bool dprChanged = std::abs(wnd->DevicePixelRatio() - dpr) > 1e-6;
#else
    const bool dprChanged = false;
#endif
    if (sizeChanged || dprChanged) {
        wnd->SetSize(w, h);
        wnd->SetDevicePixelRatio(dpr);
        view->MustBeResized();
    }
}

QSize WidgetOvrtxView::renderSize() const
{
    const qreal dpr = this->devicePixelRatioF();
    return QSize(
        std::max(1, int(std::lround(this->width() * dpr))),
        std::max(1, int(std::lround(this->height() * dpr)))
    );
}

void WidgetOvrtxView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    this->ensureViewWindow();
    this->markSceneDirty();
}

void WidgetOvrtxView::redraw()
{
    this->ensureViewWindow();

    const QSize sz = this->renderSize();
    if (sz.width() != m_lastWidth || sz.height() != m_lastHeight) {
        m_lastWidth = sz.width();
        m_lastHeight = sz.height();
        m_sceneDirty = true;
    }

    if (!m_engine->isReady()) {
        if (!m_engine->initialize()) {
            m_status = QString::fromStdString(m_engine->lastError());
            if (m_status.isEmpty()) {
                m_status = tr("ovrtx is unavailable. An NVIDIA RTX GPU and the ovrtx SDK runtime are required.");
            }
            this->update();
            return;
        }
        m_sceneDirty = true;
    }

    Ovrtx::UsdScene scene;
    if (m_guiDoc) {
        scene = Ovrtx::collectSceneFromGuiDocument(m_guiDoc, sz.width(), sz.height());
    }
    else {
        scene.camera = Ovrtx::cameraFromV3dView(this->v3dView(), sz.width(), sz.height());
    }

    const uint64_t digest = Ovrtx::sceneGeometryDigest(scene);
    if (m_sceneDirty || digest != m_lastDigest) {
        if (!m_engine->loadScene(scene)) {
            m_status = QString::fromStdString(m_engine->lastError());
            this->update();
            return;
        }
        m_lastDigest = digest;
        m_sceneDirty = false;
    }
    else if (!m_engine->updateCamera(scene.camera)) {
        m_status = QString::fromStdString(m_engine->lastError());
        this->update();
        return;
    }

    Ovrtx::RenderedFrame frame;
    if (!m_engine->renderFrame(&frame) || frame.rgba.empty()) {
        m_status = QString::fromStdString(m_engine->lastError());
        this->update();
        return;
    }

    m_frame = QImage(
        frame.rgba.data(),
        frame.width,
        frame.height,
        frame.width * 4,
        QImage::Format_RGBA8888
    ).copy();
    m_frame.setDevicePixelRatio(this->devicePixelRatioF());
    m_status.clear();
    this->update();
}

void WidgetOvrtxView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(this->rect(), QColor(32, 32, 36));
    if (!m_frame.isNull()) {
        const QSize logical = m_frame.size() / m_frame.devicePixelRatio();
        const QRect target(
            (this->width() - logical.width()) / 2,
            (this->height() - logical.height()) / 2,
            logical.width(),
            logical.height()
        );
        painter.drawImage(target, m_frame);
    }
    if (!m_status.isEmpty()) {
        painter.setPen(QColor(230, 230, 230));
        painter.drawText(this->rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, m_status);
    }
}

void WidgetOvrtxView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    this->ensureViewWindow();
    this->markSceneDirty();
}

} // namespace Mayo
