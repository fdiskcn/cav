/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#pragma once

#include "../ovrtx/ovrtx_engine.h"
#include "widget_occ_view.h"

#include <QtGui/QImage>
#include <QtWidgets/QWidget>
#include <cstdint>
#include <memory>

namespace Mayo {

class GuiDocument;

// Qt viewport that displays frames produced by NVIDIA ovrtx instead of OCCT OpenGL.
class WidgetOvrtxView : public QWidget, public IWidgetOccView {
    Q_OBJECT
public:
    WidgetOvrtxView(const OccHandle<V3d_View>& view, QWidget* parent = nullptr);

    void bindGuiDocument(GuiDocument* guiDoc);

    void redraw() override;
    QWidget* widget() override { return this; }
    bool supportsWidgetOpacity() const override { return true; }

    static WidgetOvrtxView* create(const OccHandle<V3d_View>& view, QWidget* parent);

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void markSceneDirty();
    void ensureViewWindow();
    QSize renderSize() const;

    GuiDocument* m_guiDoc = nullptr;
    std::unique_ptr<Ovrtx::OvrtxEngine> m_engine;
    QImage m_frame;
    QString m_status;
    bool m_sceneDirty = true;
    bool m_occWindowFailed = false;
    uint64_t m_lastDigest = 0;
    int m_lastWidth = 0;
    int m_lastHeight = 0;
};

} // namespace Mayo
