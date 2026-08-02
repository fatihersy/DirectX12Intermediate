from conan import ConanFile


class MoxPPRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "PremakeDeps"

    def requirements(self):
        # Needed everywhere: ASSERT macros are used across the shared
        # headers, and imgui is used by the debug UI on every backend.
        self.requires("libassert/2.2.1")
        self.requires("imgui/1.92.5")

        # assimp (model loading) and openexr (HDR images) are only used by
        # DXMaterial and DXTerrain, both of which are Windows-only —
        # DXFoliage, the one project that builds on Linux, references
        # neither. Skipping them on Linux avoids compiling two large
        # libraries from source for nothing.
        if self.settings.os == "Windows":
            self.requires("assimp/6.0.2")
            self.requires("openexr/3.4.11")

        if self.settings.os == "Linux":
            # Windowing + input for the Wayland platform backend. Managed
            # here rather than assumed present as system packages, so a
            # Linux build needs no apt prerequisites.
            #   wayland           -> libwayland-client (+ the wayland-scanner tool)
            #   wayland-protocols -> xdg-shell.xml, from which wayland-scanner
            #                        generates the window-management bindings
            #   xkbcommon         -> translates the keymap the compositor sends
            #                        us into keysyms
            self.requires("wayland/1.24.0")
            # 1.45 rather than 1.31 for cursor-shape-v1 (added in 1.32),
            # which sets the pointer by semantic name and avoids loading
            # cursor themes through libwayland-cursor - Wayland core has no
            # "restore default cursor" request, so without it, un-hiding a
            # cursor means building a wl_buffer from a theme image.
            self.requires("wayland-protocols/1.45")


            # Vulkan Memory Allocator. Vulkan makes the application own GPU
            # memory: every resource otherwise needs create / query
            # requirements / choose a memory type / allocate / bind, and each
            # of those is somewhere to get alignment or memory-type flags
            # wrong. VMA collapses it into one call and sub-allocates from a
            # few large blocks instead of asking the driver per resource.
            #
            # Adopted for that reduced surface rather than for the allocation
            # limit - this GPU reports maxMemoryAllocationCount = UINT32_MAX,
            # so the spec's 4096 floor is a portability concern, not a live
            # one. AMD ships d3d12-memory-allocator as a near-identical
            # counterpart, so both backends can eventually share one
            # allocation model.
            self.requires("vulkan-memory-allocator/3.3.0")
            # with_x11=False: this is a Wayland-only target, and the X11
            # backend would drag in xorg/system — i.e. require the X11 dev
            # packages to be installed system-wide.
            self.requires("xkbcommon/1.13.1", options={"with_x11": False})

    def build_requirements(self):
        if self.settings.os == "Linux":
            # wayland-scanner has to run on the *build* machine (it
            # generates C from XML at build time), so it's a tool
            # requirement, not a library one.
            self.tool_requires("wayland/1.24.0")

        # self.requires("gtest/1.16.0")

    # def configure(self):
    # self.options["assimp"].shared = True
