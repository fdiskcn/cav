#!/usr/bin/env bash
set -euo pipefail
# Virtual display for headless tests. Disable core dumps.
ulimit -c 0 || true
if ! pgrep -x Xvfb >/dev/null 2>&1; then
  Xvfb "${DISPLAY:-:99}" -screen 0 1280x1024x24 +extension GLX >/tmp/xvfb.log 2>&1 &
  sleep 1
fi
export DISPLAY="${DISPLAY:-:99}"
export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
export LP_NUM_THREADS="${LP_NUM_THREADS:-1}"
export QT_OPENGL="${QT_OPENGL:-software}"
export MESA_GL_VERSION_OVERRIDE="${MESA_GL_VERSION_OVERRIDE:-2.1}"
export MESA_GLSL_VERSION_OVERRIDE="${MESA_GLSL_VERSION_OVERRIDE:-120}"
exec "$@"
