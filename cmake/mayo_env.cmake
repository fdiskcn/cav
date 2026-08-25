# SPDX-License-Identifier: BSD-2-Clause
#
# Build-environment identity: platform × GPU backend.
# Users pick an environment via CMakePresets.json or scripts/mayo-env.py.
#   Mayo_GpuBackend = auto | opengl | nvidia-rtx

set(_Mayo_UseOvrtxHelp
    "Replace the built-in OCCT OpenGL viewport with NVIDIA ovrtx (downloads the ovrtx/ovstage SDK)"
)
set(_Mayo_GpuBackendHelp
    "GPU/viewport backend: auto, opengl (Apple/AMD/Intel/NVIDIA via OCCT OpenGL), nvidia-rtx (ovrtx)"
)

# NVIDIA ovrtx currently ships Windows x64 and Linux x86_64/aarch64 packages only.
set(Mayo_OvrtxPlatformSupported OFF)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(Mayo_OvrtxPlatformSupported ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64|aarch64|arm64")
    set(Mayo_OvrtxPlatformSupported ON)
endif()

if(NOT DEFINED Mayo_GpuBackend OR Mayo_GpuBackend STREQUAL "")
    set(Mayo_GpuBackend "auto" CACHE STRING "${_Mayo_GpuBackendHelp}")
endif()
set_property(CACHE Mayo_GpuBackend PROPERTY STRINGS auto opengl nvidia-rtx)

option(Mayo_UseOvrtx "${_Mayo_UseOvrtxHelp}" ${Mayo_OvrtxPlatformSupported})

string(TOLOWER "${Mayo_GpuBackend}" _Mayo_GpuBackendNorm)
if(_Mayo_GpuBackendNorm STREQUAL "nvidia-rtx"
   OR _Mayo_GpuBackendNorm STREQUAL "ovrtx"
   OR _Mayo_GpuBackendNorm STREQUAL "rtx")
    set(Mayo_GpuBackend "nvidia-rtx" CACHE STRING "${_Mayo_GpuBackendHelp}" FORCE)
    set(Mayo_UseOvrtx ON CACHE BOOL "${_Mayo_UseOvrtxHelp}" FORCE)
elseif(_Mayo_GpuBackendNorm STREQUAL "opengl"
       OR _Mayo_GpuBackendNorm STREQUAL "occt_gl"
       OR _Mayo_GpuBackendNorm STREQUAL "gl"
       OR _Mayo_GpuBackendNorm STREQUAL "system-gl")
    set(Mayo_GpuBackend "opengl" CACHE STRING "${_Mayo_GpuBackendHelp}" FORCE)
    set(Mayo_UseOvrtx OFF CACHE BOOL "${_Mayo_UseOvrtxHelp}" FORCE)
elseif(_Mayo_GpuBackendNorm STREQUAL "auto")
    set(Mayo_GpuBackend "auto" CACHE STRING "${_Mayo_GpuBackendHelp}" FORCE)
else()
    message(FATAL_ERROR
        "Unknown Mayo_GpuBackend='${Mayo_GpuBackend}'. "
        "Use auto, opengl, or nvidia-rtx."
    )
endif()

if(Mayo_UseOvrtx AND NOT Mayo_OvrtxPlatformSupported)
    if(Mayo_GpuBackend STREQUAL "nvidia-rtx")
        message(FATAL_ERROR
            "Mayo_GpuBackend=nvidia-rtx is not available on "
            "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}. "
            "NVIDIA ovrtx packages exist for Windows x64 and Linux x86_64/aarch64 only. "
            "Use Mayo_GpuBackend=opengl (CMake preset macos-gl / linux-gl / windows-gl)."
        )
    endif()
    message(WARNING
        "Mayo_UseOvrtx=ON is not supported on ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}. "
        "NVIDIA ovrtx packages exist for Windows/Linux x86_64 and aarch64 only. "
        "Falling back to the OCCT OpenGL viewport."
    )
    set(Mayo_UseOvrtx OFF CACHE BOOL "${_Mayo_UseOvrtxHelp}" FORCE)
endif()

# Host identity used in logs, install metadata, and package file names.
if(WIN32)
    set(Mayo_EnvOs "windows")
elseif(APPLE)
    set(Mayo_EnvOs "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(Mayo_EnvOs "linux")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" Mayo_EnvOs)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(Mayo_EnvArch "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(Mayo_EnvArch "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
    set(Mayo_EnvArch "armv7")
else()
    set(Mayo_EnvArch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(Mayo_UseOvrtx)
    set(Mayo_EnvGpu "nvidia-rtx")
    set(Mayo_EnvRenderer "ovrtx")
    set(Mayo_EnvGpuLabel "NVIDIA RTX (ovrtx)")
    set(Mayo_EnvRuntimeNote
        "Runtime requires an NVIDIA RTX GPU and a matching NVIDIA driver (libcuda / nvcuda)."
    )
else()
    set(Mayo_EnvGpu "opengl")
    set(Mayo_EnvRenderer "occt_gl")
    set(Mayo_EnvGpuLabel "OCCT OpenGL (Apple / AMD / Intel / NVIDIA / Mesa)")
    set(Mayo_EnvRuntimeNote
        "Viewport uses OpenCascade OpenGL. Works without an NVIDIA GPU."
    )
endif()

set(Mayo_EnvId "${Mayo_EnvOs}-${Mayo_EnvArch}-${Mayo_EnvGpu}")
set(Mayo_PackageFileName "mayo-${PROJECT_VERSION}-${Mayo_EnvId}")

message(STATUS "--------------------------------------------------")
message(STATUS "Mayo build environment")
message(STATUS "  id         = ${Mayo_EnvId}")
message(STATUS "  platform   = ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  gpu        = ${Mayo_EnvGpuLabel}")
message(STATUS "  renderer   = ${Mayo_EnvRenderer}")
message(STATUS "  GpuBackend = ${Mayo_GpuBackend}")
message(STATUS "  UseOvrtx   = ${Mayo_UseOvrtx} (sdk_packages=${Mayo_OvrtxPlatformSupported})")
message(STATUS "  package    = ${Mayo_PackageFileName}")
message(STATUS "  note       = ${Mayo_EnvRuntimeNote}")
message(STATUS "--------------------------------------------------")

file(WRITE "${CMAKE_BINARY_DIR}/MAYO_ENVIRONMENT.txt"
"Mayo build environment
id=${Mayo_EnvId}
version=${PROJECT_VERSION}
platform=${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}
gpu=${Mayo_EnvGpu}
renderer=${Mayo_EnvRenderer}
Mayo_GpuBackend=${Mayo_GpuBackend}
Mayo_UseOvrtx=${Mayo_UseOvrtx}
${Mayo_EnvRuntimeNote}
")

macro(mayo_install_environment_file)
    install(
        FILES "${CMAKE_BINARY_DIR}/MAYO_ENVIRONMENT.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mayo
    )
endmacro()

macro(mayo_setup_packaging)
    set(CPACK_PACKAGE_NAME "mayo")
    set(CPACK_PACKAGE_VENDOR "${Mayo_CompanyName}")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_FILE_NAME "${Mayo_PackageFileName}")
    set(CPACK_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/dist")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_STRIP_FILES OFF)
    if(WIN32)
        set(CPACK_GENERATOR "ZIP")
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()
    include(CPack)
endmacro()
