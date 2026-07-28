# Building the project

This project supports the following platforms:

| OS          | x86_64             |
| ----------- | ------------------ |
| **Windows** | :white_check_mark: |
| **Linux**   | :hourglass:        |

Linux support is in progress and currently applies to **`DXFoliage` only**
(`DXMaterial`/`DXTerrain` remain Windows-only). The Linux toolchain builds
and links today; the Vulkan renderer backend and Wayland platform backend
are still being implemented, so the binary does not render yet.

## Requirements

* **Python 3** with venv support
* **CMake** (required for some Conan packages)
* **Windows SDK** (Windows)
* **Clang** or **GCC** (Linux), **MSVC** or **Clang-CL** (Windows)
* **Visual Studio 2019+** with the C++ workloads (Windows, only when using the `visualstudio` build system)
* **vcpkg** dependencies are fetched automatically during `init`

### Linux

Almost everything is managed for you: Conan provides Wayland, the Wayland
protocol definitions, `wayland-scanner` and xkbcommon; vcpkg provides the
Vulkan headers. Two things still come from the system:

```sh
sudo apt install xkb-data           # keymap data (xkeyboard-config); already
                                    # present on any desktop install
sudo apt install mesa-vulkan-drivers vulkan-tools   # a Vulkan driver + tools
```

A Vulkan-capable driver is a runtime requirement rather than a build one —
`mesa-vulkan-drivers` also provides the `lavapipe` software rasteriser,
which is enough to run without a suitable GPU.

## Dependencies

External dependencies are managed through two package managers, both handled automatically by the `init` step:

* **Conan** — assimp, imgui
* **vcpkg** — directxtk12, directx-dxc

## Getting started

### Setting up the project

Run the following command in the root of the repository:

```bat
mox.bat init          &:: Windows
./mox.sh init         # Linux
```

This will download and compile all external dependencies (Conan and vcpkg), then generate the appropriate project/build files. Please be patient on the first run.

### Writing code and compiling

The default build system is **Makefile** with the **Clang** compiler (as configured in `mox.lua`). After running `init`, build with:

```bat
mox.bat build --conf Debug          &:: Windows
./mox.sh build --conf Debug         # Linux
```

If you prefer Visual Studio on Windows, you can switch the build system in `mox.lua` (`cmox_build_system = "visualstudio"`) and re-run `init`. A `.sln` file will be generated in the root directory that you can open with Visual Studio.

### Running

To run a compiled executable in the correct working directory:

```bat
mox.bat run --conf Debug EXE [args...]
./mox.sh run --conf Debug EXE [args...]
```

Where `EXE` is the project name (e.g. `DXMaterial`, `DXTerrain`). Additional arguments after the executable name are forwarded to the application.
