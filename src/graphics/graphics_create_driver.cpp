/****************************************************************************
** Copyright (c) 2016, Fougue SAS <https://www.fougue.pro>
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

// --
// NOTE
// This file isolates inclusion of <Aspect_DisplayConnection.hxx> which is problematic on X11/Linux
// <X.h> #defines constants like "None" which causes name clash with GuiDocument::ViewTrihedronMode::None
// --

#include "../base/occ_handle.h"

#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Standard_Failure.hxx>
#include <cstdlib>
#include <functional>
#include <new>

namespace Mayo {

using FunctionCreateGraphicsDriver = std::function<OccHandle<Graphic3d_GraphicDriver>()>;

static bool delayOpenGlDriverInit()
{
    // Software GL: delay the dummy GLX window; QOpenGLWidget initializes later.
    const char* soft = std::getenv("LIBGL_ALWAYS_SOFTWARE");
    return soft && soft[0] == '1';
}

static FunctionCreateGraphicsDriver& getFunctionCreateGraphicsDriver()
{
    static FunctionCreateGraphicsDriver fn = []{
        try {
            auto disp = makeOccHandle<Aspect_DisplayConnection>();
            const bool delayInit = delayOpenGlDriverInit();
            auto driver = makeOccHandle<OpenGl_GraphicDriver>(disp, !delayInit);
            if (delayInit) {
                driver->ChangeOptions().buffersNoSwap = true;
                driver->ChangeOptions().swapInterval = 0;
            }
            return OccHandle<Graphic3d_GraphicDriver>(driver);
        }
        catch (const Standard_Failure&) {
            return OccHandle<Graphic3d_GraphicDriver>();
        }
        catch (const std::bad_alloc&) {
            return OccHandle<Graphic3d_GraphicDriver>();
        }
    };
    return fn;
}

void setFunctionCreateGraphicsDriver(FunctionCreateGraphicsDriver fn)
{
    getFunctionCreateGraphicsDriver() = std::move(fn);
}

OccHandle<Graphic3d_GraphicDriver> graphicsCreateDriver()
{
    const auto& fn = getFunctionCreateGraphicsDriver();
    if (fn)
        return fn();

    return {};
}

} // namespace Mayo
