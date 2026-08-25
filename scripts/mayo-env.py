#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Select a Mayo build environment (platform × GPU) and configure / build / test / package.

Compile and unit tests do not require CUDA. NVIDIA RTX is only needed to produce
RTX pixels. Linux environments can run in Docker from macOS / Windows.
"""

from __future__ import print_function

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
STATE_PATH = os.path.join(ROOT, ".mayo-env-current")
DOCKERFILE = os.path.join(ROOT, "docker", "linux-env.Dockerfile")
DOCKER_IMAGE = "mayo-linux-dev:24.04"
PACKAGES_DIR = os.path.join(ROOT, "cmake", "_deps", "packages")

OVRTX_ZIP = {
    ("Linux", "arm64"): "ovrtx@0.4.1.364340.7f330117.manylinux_2_35_aarch64.zip",
    ("Linux", "x64"): "ovrtx@0.4.1.364340.7f330117.manylinux_2_35_x86_64.zip",
    ("Windows", "x64"): "ovrtx@0.4.1.364340.7f330117.windows-x86_64.zip",
}
OVSTAGE_ZIP = {
    ("Linux", "arm64"): "ovstage@0.1.1.355824.553acd42.manylinux_2_35_aarch64.zip",
    ("Linux", "x64"): "ovstage@0.1.1.355824.553acd42.manylinux_2_35_x86_64.zip",
    ("Windows", "x64"): "ovstage@0.1.1.355824.553acd42.windows-x86_64.zip",
}

ENVIRONMENTS = [
    {
        "id": "macos-gl",
        "hosts": ["Darwin"],
        "gpu": "opengl",
        "gpu_label": "Apple / AMD / Intel（OCCT OpenGL）",
        "renderer": "occt_gl",
        "downloads_sdk": False,
        "needs_nvidia_runtime": False,
        "docker": False,
        "description": "macOS 通用显卡。ovrtx 没有 macOS 包。无 CUDA 即可构建测试。",
    },
    {
        "id": "linux-gl",
        "hosts": ["Linux"],
        "gpu": "opengl",
        "gpu_label": "AMD / Intel / NVIDIA / Mesa（OCCT OpenGL）",
        "renderer": "occt_gl",
        "downloads_sdk": False,
        "needs_nvidia_runtime": False,
        "docker": True,
        "description": "Linux 通用显卡。无 CUDA。可用 Docker 在 macOS 上构建测试。",
    },
    {
        "id": "linux-nvidia-rtx",
        "hosts": ["Linux"],
        "gpu": "nvidia-rtx",
        "gpu_label": "NVIDIA RTX（ovrtx）",
        "renderer": "ovrtx",
        "downloads_sdk": True,
        "needs_nvidia_runtime": True,
        "docker": True,
        "description": "Linux ovrtx 视口。编译/单测不需要 CUDA；出图才需要 NVIDIA GPU。",
    },
    {
        "id": "windows-gl",
        "hosts": ["Windows"],
        "gpu": "opengl",
        "gpu_label": "AMD / Intel / NVIDIA（OCCT OpenGL）",
        "renderer": "occt_gl",
        "downloads_sdk": False,
        "needs_nvidia_runtime": False,
        "docker": False,
        "description": "Windows x64 通用显卡。无 CUDA 即可构建测试。",
    },
    {
        "id": "windows-nvidia-rtx",
        "hosts": ["Windows"],
        "gpu": "nvidia-rtx",
        "gpu_label": "NVIDIA RTX（ovrtx）",
        "renderer": "ovrtx",
        "downloads_sdk": True,
        "needs_nvidia_runtime": True,
        "docker": False,
        "description": "Windows ovrtx 视口。编译/单测不需要 CUDA；出图才需要 NVIDIA GPU。",
    },
    {
        "id": "usd-tests",
        "hosts": ["Darwin", "Linux", "Windows"],
        "gpu": "none",
        "gpu_label": "无 GPU（仅 USD 管线单测）",
        "renderer": "none",
        "downloads_sdk": False,
        "needs_nvidia_runtime": False,
        "docker": False,
        "special": "usd",
        "description": "不依赖 Qt / OpenCascade / CUDA。验证网格→USDA。",
    },
]


def host_system():
    system = platform.system()
    if system in ("Darwin", "Linux", "Windows"):
        return system
    return system


def host_arch():
    machine = platform.machine().lower()
    if machine in ("aarch64", "arm64"):
        return "arm64"
    if machine in ("x86_64", "amd64"):
        return "x64"
    return machine


def nvidia_smi_ok():
    exe = shutil.which("nvidia-smi")
    if not exe:
        return False
    try:
        subprocess.run(
            [exe, "-L"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
        return True
    except (OSError, subprocess.CalledProcessError):
        return False


def docker_ok():
    if not shutil.which("docker"):
        return False
    try:
        subprocess.run(
            ["docker", "info"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
        return True
    except (OSError, subprocess.CalledProcessError):
        return False


def docker_memory_bytes():
    if not docker_ok():
        return 0
    try:
        out = subprocess.check_output(
            ["docker", "info", "--format", "{{.MemTotal}}"],
            text=True,
        ).strip()
        return int(out)
    except (OSError, subprocess.CalledProcessError, ValueError):
        return 0


def disable_core_dumps():
    """Avoid multi-GB cores after Mesa/OCCT abort filling a CI disk."""
    if host_system() != "Linux":
        return
    try:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    except (ImportError, ValueError, OSError):
        pass


def x11_socket_path(display):
    spec = (display or "").split(",")[0].strip()
    if not spec or spec.startswith("unix:"):
        return None
    _host, sep, screen = spec.partition(":")
    if not sep or not screen:
        return None
    num = screen.split(".")[0]
    if not num.isdigit():
        return None
    return os.path.join("/tmp/.X11-unix", "X" + num)


def x_display_available(display):
    sock = x11_socket_path(display)
    if sock and os.path.exists(sock):
        return True
    xdpyinfo = shutil.which("xdpyinfo")
    if not xdpyinfo:
        return False
    try:
        subprocess.run(
            [xdpyinfo, "-display", display],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
        return True
    except (OSError, subprocess.CalledProcessError):
        return False


def cmake_exe():
    exe = shutil.which("cmake")
    if not exe:
        sys.exit("未找到 cmake，请先安装 CMake ≥ 3.16（使用 preset 需要 ≥ 3.21）。")
    return exe


def cmake_version_tuple():
    out = subprocess.check_output([cmake_exe(), "--version"], text=True)
    first = out.splitlines()[0].strip().split()[-1]
    parts = first.split(".")
    return tuple(int(p.split("-")[0]) for p in parts[:3])


def cmake_supports_presets():
    return cmake_version_tuple() >= (3, 21, 0)


def cmake_build_cmd(target, config, preset=False):
    """Always pass --config so Visual Studio (multi-config) matches ctest -C."""
    cmd = [cmake_exe(), "--build"]
    if preset:
        cmd += ["--preset", target]
    else:
        cmd += [target]
    cmd += ["--config", config]
    jobs = os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL")
    cmd += ["--parallel", jobs] if jobs else ["--parallel"]
    return cmd


def env_by_id(env_id):
    for env in ENVIRONMENTS:
        if env["id"] == env_id:
            return env
    sys.exit("未知环境：%s。运行 `python3 scripts/mayo-env.py list` 查看。" % env_id)


def preset_name(env, debug):
    if env.get("special") == "usd":
        return "usd-tests"
    name = env["id"]
    if debug and not name.endswith("-debug"):
        name += "-debug"
    return name


def bindir_for(env, debug):
    return os.path.join(ROOT, "build", preset_name(env, debug))


def read_state():
    if not os.path.isfile(STATE_PATH):
        return None
    with open(STATE_PATH, "r", encoding="utf-8") as handle:
        return json.load(handle)


def write_state(env_id, debug):
    with open(STATE_PATH, "w", encoding="utf-8") as handle:
        json.dump({"id": env_id, "debug": bool(debug)}, handle)
        handle.write("\n")


def resolve_env(args):
    env_id = getattr(args, "env", None)
    debug = bool(getattr(args, "debug", False))
    if env_id:
        return env_by_id(env_id), debug
    state = read_state()
    if not state:
        sys.exit("请指定环境，或先运行 configure。例：python3 scripts/mayo-env.py list")
    env = env_by_id(state["id"])
    if not debug:
        debug = bool(state.get("debug"))
    return env, debug


def runnable_via(env, force_native=False, force_docker=False):
    host = host_system()
    if force_docker:
        if env.get("docker") and docker_ok():
            return "docker"
        return None
    if host in env["hosts"] and not force_docker:
        return "native"
    if force_native:
        return None
    if env.get("docker") and docker_ok():
        return "docker"
    return None


def availability_label(env):
    via = runnable_via(env)
    if via == "native":
        return "本机"
    if via == "docker":
        return "Docker"
    return "否（CI）"


def local_sdk_file(mapping):
    name = mapping.get((host_system(), host_arch()))
    if not name:
        return None
    path = os.path.join(PACKAGES_DIR, name)
    return path if os.path.isfile(path) else None


def extra_cmake_args(env):
    args = []
    fetch_dir = os.environ.get("MAYO_FETCHCONTENT_BASE_DIR")
    if fetch_dir:
        args.append("-DFETCHCONTENT_BASE_DIR=" + fetch_dir)
    occ = os.environ.get("OpenCASCADE_DIR")
    if occ:
        args.append("-DOpenCASCADE_DIR=" + occ.replace("\\", "/"))
    if host_system() == "Windows":
        # Match Mayo's Windows CI: copy Qt/OCC/ovrtx DLLs next to test exes.
        args.append("-DMayo_PostBuildCopyRuntimeDLLs=ON")
    if env.get("downloads_sdk"):
        ovrtx = os.environ.get("OVRTX_LOCAL_PACKAGE") or local_sdk_file(OVRTX_ZIP)
        ovstage = os.environ.get("OVSTAGE_LOCAL_PACKAGE") or local_sdk_file(OVSTAGE_ZIP)
        if ovrtx:
            args.append("-DOVRTX_LOCAL_PACKAGE=" + ovrtx)
        if ovstage:
            args.append("-DOVSTAGE_LOCAL_PACKAGE=" + ovstage)
    return args


def run(cmd, cwd=None):
    print("+ " + " ".join(cmd), flush=True)
    kwargs = {}
    # Isolate cmake/ctest from the Actions runner session. A GL abort that
    # signals the process group must not look like the job was cancelled.
    if host_system() == "Linux":
        kwargs["start_new_session"] = True
    subprocess.check_call(cmd, cwd=cwd or ROOT, **kwargs)


def ensure_linux_image():
    inspect = subprocess.run(
        ["docker", "image", "inspect", DOCKER_IMAGE],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if inspect.returncode == 0:
        return
    print("+ docker build -t %s -f docker/linux-env.Dockerfile docker/" % DOCKER_IMAGE, flush=True)
    subprocess.check_call(
        ["docker", "build", "-t", DOCKER_IMAGE, "-f", DOCKERFILE, os.path.join(ROOT, "docker")]
    )


def docker_run_mayo_env(argv):
    ensure_linux_image()
    mem = docker_memory_bytes()
    if mem and mem < 12 * 1024 * 1024 * 1024:
        print(
            "警告：Docker 内存约 %.1fGiB，Linux 测试建议至少 16GiB。"
            % (mem / (1024.0 ** 3)),
            file=sys.stderr,
            flush=True,
        )
    jobs = os.environ.get("MAYO_DOCKER_JOBS", "2")
    cmd = [
        "docker",
        "run",
        "--rm",
        "--init",
        "--ulimit",
        "core=0",
        "--shm-size",
        "1g",
        "--platform",
        "linux/" + ("arm64" if host_arch() == "arm64" else "amd64"),
        "-v",
        ROOT + ":/src",
        "-w",
        "/src",
        "-e",
        # Persist ovrtx/ovstage extracts on the bind-mounted repo (gitignored).
        "MAYO_FETCHCONTENT_BASE_DIR=/src/cmake/_deps",
        "-e",
        "CMAKE_BUILD_PARALLEL_LEVEL=" + jobs,
        "-e",
        "DISPLAY=:99",
        "-e",
        "LIBGL_ALWAYS_SOFTWARE=0",
        "-e",
        "LP_NUM_THREADS=1",
        "-e",
        "QT_OPENGL=software",
        "-e",
        "MESA_GL_VERSION_OVERRIDE=2.1",
        "-e",
        "MESA_GLSL_VERSION_OVERRIDE=120",
        "-e",
        "MAYO_SKIP_GL_TESTS=1",
        DOCKER_IMAGE,
        "python3",
        "scripts/mayo-env.py",
    ] + list(argv) + ["--native"]
    run(cmd)


def maybe_docker(args, rest_argv):
    env, _debug = resolve_env(args)
    force_native = bool(getattr(args, "native", False))
    force_docker = bool(getattr(args, "docker", False))
    via = runnable_via(env, force_native=force_native, force_docker=force_docker)
    if via == "docker" and not force_native:
        docker_run_mayo_env(rest_argv)
        return True
    if via is None:
        sys.exit(
            "环境 `%s` 无法在本机运行（当前 %s %s）。\n"
            "Linux 环境请安装 Docker，或在 GitHub Actions 工作流 ci_environments.yml 中测试。"
            % (env["id"], host_system(), host_arch())
        )
    return False


def ensure_display():
    """Start Xvfb when no X server is listening.

    Do not set LIBGL_ALWAYS_SOFTWARE here. That Mesa llvmpipe path aborts in
    Docker and is already applied only in docker_run_mayo_env / entrypoint.
    GitHub-hosted Linux matches Mayo's Xvfb-only CI (no software-GL force,
    no MAYO_SKIP_GL_TESTS).
    """
    if host_system() != "Linux":
        return
    disable_core_dumps()
    display = os.environ.get("DISPLAY") or ":99"
    os.environ["DISPLAY"] = display
    if x_display_available(display):
        return
    xvfb = shutil.which("Xvfb")
    if not xvfb:
        return
    subprocess.Popen(
        [xvfb, display, "-screen", "0", "1280x1024x24", "+extension", "GLX"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    for _ in range(15):
        time.sleep(0.2)
        if x_display_available(display):
            return
    print("警告：Xvfb 可能未在 %s 上就绪。" % display, file=sys.stderr, flush=True)


def cmd_list(args):
    host = host_system()
    print("当前主机：%s %s" % (host, host_arch()))
    print("NVIDIA 驱动：%s（仅 RTX 出图需要；编译/单测不需要）" % ("可用" if nvidia_smi_ok() else "未检测到"))
    print("Docker     ：%s" % ("可用" if docker_ok() else "未检测到"))
    print("")
    print("%-22s %-10s %-36s %s" % ("环境", "如何运行", "显卡 / 视口", "说明"))
    print("-" * 110)
    for env in ENVIRONMENTS:
        label = availability_label(env)
        if args.all or label != "否（CI）":
            print("%-22s %-10s %-36s %s" % (env["id"], label, env["gpu_label"], env["description"]))
    if not args.all:
        print("")
        print("用 --all 列出全部平台。无 CUDA 时仍可构建测试 nvidia-rtx 环境。")


def cmd_suggest(args):
    host = host_system()
    has_nvidia = nvidia_smi_ok()
    if host == "Darwin":
        native = "macos-gl"
        extra = "linux-gl / linux-nvidia-rtx（Docker，无 CUDA 编译测试）"
        reason = "本机走 OpenGL；Linux 环境用 Docker 做无 CUDA 构建测试。"
    elif host == "Linux":
        native = "linux-nvidia-rtx" if has_nvidia else "linux-gl"
        extra = "linux-nvidia-rtx（无 CUDA 也可编译/单测）"
        reason = "检测到 NVIDIA 驱动，建议 RTX 视口。" if has_nvidia else "未检测到 NVIDIA，建议 OpenGL；仍可编译 linux-nvidia-rtx。"
    elif host == "Windows":
        native = "windows-nvidia-rtx" if has_nvidia else "windows-gl"
        extra = "windows-nvidia-rtx（无 CUDA 也可编译/单测）"
        reason = "检测到 NVIDIA 驱动，建议 RTX 视口。" if has_nvidia else "未检测到 NVIDIA，建议 OpenGL。"
    else:
        sys.exit("不支持的主机：%s" % host)
    print("本机建议：%s" % native)
    print("无 CUDA 全平台：usd-tests + %s + %s" % (native, extra))
    print("原因：%s" % reason)
    print("")
    print("跑本机可测的全部环境：")
    print("  python3 scripts/mayo-env.py platform")


def configure_usd(debug):
    bindir = os.path.join(ROOT, "build", "usd-tests")
    cmake = cmake_exe()
    cmd = [
        cmake,
        "-S",
        os.path.join(ROOT, "tests", "ovrtx"),
        "-B",
        bindir,
        "-DCMAKE_BUILD_TYPE=" + ("Debug" if debug else "Release"),
    ]
    # Do not force g++ on Windows: GitHub-hosted images may have a stray MinGW
    # g++ that CMake would then pick instead of MSVC.
    if host_system() == "Linux":
        compiler = shutil.which("g++")
        if compiler:
            cmd.append("-DCMAKE_CXX_COMPILER=" + compiler)
    run(cmd)
    write_state("usd-tests", debug)
    return bindir


def configure_app(env, debug):
    name = preset_name(env, debug)
    cmake = cmake_exe()
    bindir = bindir_for(env, debug)
    build_type = "Debug" if debug else "Release"
    extra = extra_cmake_args(env)
    if cmake_supports_presets():
        run([cmake, "--preset", name, "-S", ROOT] + extra)
    else:
        os.makedirs(bindir, exist_ok=True)
        cmd = [
            cmake,
            "-S",
            ROOT,
            "-B",
            bindir,
            "-DCMAKE_BUILD_TYPE=" + build_type,
            "-DMayo_BuildTests=ON",
            "-DMayo_GpuBackend=" + env["gpu"],
            "-DMayo_UseOvrtx=" + ("ON" if env["gpu"] == "nvidia-rtx" else "OFF"),
            "-DCMAKE_INSTALL_PREFIX=" + os.path.join(ROOT, "dist", name),
        ] + extra
        run(cmd)
    write_state(env["id"], debug)
    return bindir


def has_cache(bindir):
    return os.path.isfile(os.path.join(bindir, "CMakeCache.txt"))


def cmd_configure(args):
    if maybe_docker(args, _argv_for_docker("configure", args)):
        return
    env, debug = resolve_env(args)
    if env.get("special") == "usd":
        configure_usd(debug)
    else:
        configure_app(env, debug)


def cmd_build(args):
    if maybe_docker(args, _argv_for_docker("build", args)):
        return
    env, debug = resolve_env(args)
    name = preset_name(env, debug)
    if env.get("special") == "usd":
        configure_usd(debug)
        config = "Debug" if debug else "Release"
        run(cmake_build_cmd(os.path.join(ROOT, "build", "usd-tests"), config))
        return
    configure_app(env, debug)
    config = "Debug" if debug else "Release"
    if cmake_supports_presets():
        run(cmake_build_cmd(name, config, preset=True))
    else:
        run(cmake_build_cmd(bindir_for(env, debug), config))


def cmd_test(args):
    if maybe_docker(args, _argv_for_docker("test", args)):
        return
    env, debug = resolve_env(args)
    config = "Debug" if debug else "Release"
    ctest = shutil.which("ctest")
    if not ctest:
        sys.exit("未找到 ctest。")
    if env.get("special") == "usd":
        bindir = os.path.join(ROOT, "build", "usd-tests")
    else:
        bindir = bindir_for(env, debug)
    cmd_build(args)
    ensure_display()
    run([ctest, "--test-dir", bindir, "--output-on-failure", "-C", config])


def cmd_package(args):
    if maybe_docker(args, _argv_for_docker("package", args)):
        return
    env, debug = resolve_env(args)
    if env.get("special") == "usd":
        sys.exit("usd-tests 只跑测试，没有可交付安装包。")
    cmake = cmake_exe()
    name = preset_name(env, debug)
    bindir = bindir_for(env, debug)
    if not has_cache(bindir):
        cmd_build(args)
    config = "Debug" if debug else "Release"
    prefix = os.path.join(ROOT, "dist", name)
    run([cmake, "--install", bindir, "--prefix", prefix, "--config", config])
    cpack = shutil.which("cpack")
    if cpack:
        gen = "ZIP" if host_system() == "Windows" else "TGZ"
        run([cpack, "-G", gen, "-C", config, "-B", os.path.join(ROOT, "dist")], cwd=bindir)
    print("")
    print("安装目录：%s" % prefix)
    print("归档目录：%s" % os.path.join(ROOT, "dist"))


def cmd_all(args):
    if maybe_docker(args, _argv_for_docker("all", args)):
        return
    env, debug = resolve_env(args)
    args.env = env["id"]
    args.debug = debug
    args.native = True
    cmd_configure(args)
    cmd_build(args)
    try:
        cmd_test(args)
    except subprocess.CalledProcessError:
        print("测试失败。", file=sys.stderr)
        if not getattr(args, "keep_going", False):
            raise
        print("已指定 --keep-going，继续打包。", file=sys.stderr)
    if env.get("special") != "usd":
        cmd_package(args)


def cmd_info(args):
    host = host_system()
    print("主机          : %s %s" % (host, host_arch()))
    print("CMake         : %s" % ".".join(str(p) for p in cmake_version_tuple()))
    print("preset 支持   : %s" % ("是" if cmake_supports_presets() else "否（将回退到 -D 参数）"))
    print("nvidia-smi    : %s（出图才需要）" % ("是" if nvidia_smi_ok() else "否"))
    if docker_ok():
        mem = docker_memory_bytes()
        mem_s = "%.1fGiB" % (mem / (1024.0 ** 3)) if mem else "未知"
        print("Docker        : 是（内存 %s，Linux 测试建议 ≥16GiB）" % mem_s)
    else:
        print("Docker        : 否")
    state = read_state()
    if state:
        env = env_by_id(state["id"])
        print("当前环境      : %s%s" % (state["id"], " (debug)" if state.get("debug") else ""))
        print("构建目录      : %s" % os.path.join(ROOT, "build", preset_name(env, state.get("debug"))))
    else:
        print("当前环境      : （尚未 configure）")
    print("可运行环境    :")
    for env in ENVIRONMENTS:
        print("  %-20s %s" % (env["id"], availability_label(env)))


def cmd_platform(args):
    """Build and test every environment this machine can run without CUDA."""
    only = set(args.only or [])
    results = []
    for env in ENVIRONMENTS:
        if only and env["id"] not in only:
            continue
        via = runnable_via(env, force_native=args.native, force_docker=args.docker)
        if via is None:
            results.append((env["id"], "skip", "本机无法运行，交给 CI"))
            continue
        print("")
        print("======== %s (%s) ========" % (env["id"], via))
        child = argparse.Namespace(
            env=env["id"],
            debug=bool(args.debug),
            native=(via == "native") or args.native,
            docker=(via == "docker") or args.docker,
            keep_going=False,
        )
        try:
            if args.package and env.get("special") != "usd":
                cmd_all(child)
            else:
                # configure + build are inside cmd_test (and inside one container for Docker).
                cmd_test(child)
            results.append((env["id"], "pass", via))
        except subprocess.CalledProcessError as err:
            results.append((env["id"], "fail", "exit %s" % err.returncode))
            if not args.keep_going:
                _print_platform_report(results)
                raise
    _print_platform_report(results)
    if any(status == "fail" for _env, status, _note in results):
        sys.exit(1)


def _print_platform_report(results):
    print("")
    print("======== 环境测试报告 ========")
    print("%-22s %-8s %s" % ("环境", "结果", "备注"))
    print("-" * 60)
    for env_id, status, note in results:
        print("%-22s %-8s %s" % (env_id, status, note))
    report = os.path.join(ROOT, "dist", "platform-report.txt")
    os.makedirs(os.path.dirname(report), exist_ok=True)
    with open(report, "w", encoding="utf-8") as handle:
        handle.write("Mayo no-CUDA platform report\n")
        handle.write("host=%s %s\n" % (host_system(), host_arch()))
        handle.write("nvidia-smi=%s\n" % ("yes" if nvidia_smi_ok() else "no"))
        handle.write("docker=%s\n" % ("yes" if docker_ok() else "no"))
        mem = docker_memory_bytes()
        if mem:
            handle.write("docker_memory_giB=%.1f\n" % (mem / (1024.0 ** 3)))
        for env_id, status, note in results:
            handle.write("%s\t%s\t%s\n" % (env_id, status, note))
    print("报告：%s" % report)


def _argv_for_docker(command, args):
    argv = [command]
    env_id = getattr(args, "env", None)
    if env_id:
        argv.append(env_id)
    if getattr(args, "debug", False):
        argv.append("--debug")
    if command == "all" and getattr(args, "keep_going", False):
        argv.append("--keep-going")
    return argv


def add_env_argument(parser):
    parser.add_argument(
        "env",
        nargs="?",
        help="环境 id，例如 macos-gl、linux-nvidia-rtx。省略则使用上次 configure 的环境。",
    )
    parser.add_argument("--debug", action="store_true", help="使用 Debug 配置")
    parser.add_argument("--native", action="store_true", help="强制本机运行，不使用 Docker")
    parser.add_argument("--docker", action="store_true", help="强制用 Docker 跑 Linux 环境")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="按平台和显卡选择 Mayo 构建环境。编译与单测不需要 CUDA。",
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_list = sub.add_parser("list", help="列出环境（含 Docker / CI）")
    p_list.add_argument("--all", action="store_true", help="列出全部平台")
    p_list.set_defaults(func=cmd_list)

    p_suggest = sub.add_parser("suggest", help="根据本机 OS / Docker / 显卡推荐")
    p_suggest.set_defaults(func=cmd_suggest)

    p_info = sub.add_parser("info", help="显示本机与当前环境")
    p_info.set_defaults(func=cmd_info)

    p_cfg = sub.add_parser("configure", help="CMake 配置所选环境")
    add_env_argument(p_cfg)
    p_cfg.set_defaults(func=cmd_configure)

    p_build = sub.add_parser("build", help="编译")
    add_env_argument(p_build)
    p_build.set_defaults(func=cmd_build)

    p_test = sub.add_parser("test", help="运行 ctest（无 CUDA）")
    add_env_argument(p_test)
    p_test.set_defaults(func=cmd_test)

    p_pkg = sub.add_parser("package", help="安装到 dist/<环境> 并打 TGZ/ZIP")
    add_env_argument(p_pkg)
    p_pkg.set_defaults(func=cmd_package)

    p_all = sub.add_parser("all", help="configure + build + test + package")
    add_env_argument(p_all)
    p_all.add_argument("--keep-going", action="store_true", help="测试失败仍继续打包")
    p_all.set_defaults(func=cmd_all)

    p_plat = sub.add_parser(
        "platform",
        help="无 CUDA 下构建并测试本机能跑的全部环境（含 Docker 中的 Linux）",
    )
    p_plat.add_argument("--debug", action="store_true")
    p_plat.add_argument("--native", action="store_true", help="只用本机，不启动 Docker")
    p_plat.add_argument("--docker", action="store_true", help="Linux 环境强制 Docker")
    p_plat.add_argument("--package", action="store_true", help="测试通过后打包")
    p_plat.add_argument("--keep-going", action="store_true", help="某个环境失败后继续")
    p_plat.add_argument(
        "--only",
        nargs="+",
        metavar="ENV",
        help="只跑指定环境，例如 --only usd-tests linux-nvidia-rtx",
    )
    p_plat.set_defaults(func=cmd_platform)

    parsed = parser.parse_args(argv)
    parsed.func(parsed)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as err:
        sys.exit(err.returncode)
