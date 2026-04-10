# Building the project

This project supports the following platforms:

| OS          | x86_64             |
| ----------- | ------------------ |
| **Windows** | :white_check_mark: |
| **Linux**   | :white_check_mark: |

## Requirements

- **Python 3** with venv support
- **CMake** (required for some Conan packages)
- **Clang** or **GCC** (Linux), **MSVC** or **Clang-CL** (Windows)
- **Visual Studio 2019+** with the C++ workloads (Windows, only when using the `visualstudio` build system)
- **vcpkg** dependencies are fetched automatically during `init`

## Dependencies

External dependencies are managed through two package managers, both handled automatically by the `init` step:

- **Conan** — assimp, imgui
- **vcpkg** — directxtk12, directx-dxc

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

## Releases

This project is designed to automatically set its version macro and package all artifacts. This can be done via a manual `mox deploy` call. The call expects the environment variable `MOXPP_VERSION` to be set to the current version string. The deploy process can be seen in `scripts/deploy.py`.

## Actions

All actions are invoked through the mox launcher (`mox.bat` on Windows, `./mox.sh` on Linux).

### Core workflow

- **init** — Initializes the repository: generates project/build files, acquires dependencies via Conan and vcpkg, and downloads build tools.
  Usage: `mox init [--conf CONF] [--arch ARCH] [--build-system visualstudio|makefile] [--compiler msvc|gcc|clang|clang-cl] [--target-os windows|linux] [--vs-version YEAR] [--skip-conan] [--skip-vcpkg] [--conan-release-only]`

- **build** — Builds the project using the configured build system.
  Usage: `mox build [--conf CONF] [--build-system visualstudio|makefile] [--arch ARCH]`

- **deploy** — Packages a build into versioned zip archives (binaries and source).
  Usage: `mox deploy [--conf CONF] [--arch ARCH]`

- **run** — Runs a compiled executable in the correct working directory.
  Usage: `mox run [--conf CONF] [--arch ARCH] EXE [args...]`

- **test** — Initializes, builds (Release by default), and runs the `unittest` executable. Returns the test application's exit code.
  Usage: `mox test [--conf CONF] [--arch ARCH]`

- **clean** — Removes generated files of a given category.
  Usage: `mox clean [mode]`
  Modes: `output` (default), `project`, `dependencies`, `vcpkg`, `all`.

### Convenience shortcuts

- **autogen** — Runs `init`, `build`, and `deploy` in sequence.
  Usage: `mox autogen [--conf CONF] [--arch ARCH]`

- **rebuild** — Cleans build output and rebuilds the project.

- **regen** — Cleans project files and re-runs `init`. Accepts all `init` arguments.

- **reinit** — Full reset: cleans everything (output, project files, dependencies) and re-runs `init` from scratch.

### Utilities

- **graph** — Generates an HTML visualization of the Conan dependency graph.

- **archive** — Creates a timestamped git archive of all committed changes in the `./archive` directory.

- **copydlls** — Copies dependency binaries (.dll/.so) from Conan and vcpkg into the `dlls/` directory, organized by configuration and architecture.
  Usage: `mox copydlls [ARCH]`

- **distdlls** — Copies binaries from a source path to a destination path (used internally as a post-build step).
  Usage: `mox distdlls SRC DST`

### Notes

`--conf` always refers to your project's build configuration (as defined in `mox.lua`, e.g. `Debug` or `Release`). `--arch` is the target architecture — omit it to use your system's native architecture, or supply a different value to cross-compile.
