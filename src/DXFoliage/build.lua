-- Arguments: Project name, Output name, Output directory (binary only)
mox_project("DXFoliage", "dx_foliage", "bin/DXFoliage/")
mox_cpp("C++20")
mox_windowed()
mox_use_vcpkg()
uuid("9cf7f7ea-bf2c-4e6e-971f-3301f9b45514")

architecture "x64"
-- Target OS comes from --mox_target_os (see scripts/libmox.lua) rather
-- than being hardcoded, so this same file builds for Windows or Linux.
mox_target_system()
warnings "Default"
fatalwarnings { "All" }
multiprocessorcompile "On"

filter "system:windows"
systemversion "10.0.28000.0:latest"
buffersecuritycheck "On"
filter {}

filter "action:vs*"
--buildoptions { "/ZW" } Not supported with C++23 Preview and premake5 beta8 forces to that
buildoptions { "/sdl" }
filter {}

files {
    "**.h",
    "**.cpp",
    "**.hlsl",
    "**.hlsli",
}

-- Per-OS source selection. The tree is arranged so this is purely
-- directory-based: each platform/graphics backend lives in its own folder
-- (platform/win32, platform/wayland, rhi/dx12, rhi/vulkan) alongside the
-- vendored imgui backend it owns.
filter "system:windows"
removefiles {
    "platform/linux/**",
    "platform/wayland/**",
    "rhi/vulkan/**",
}
filter {}

filter "system:linux"
removefiles {
    "platform/win32/**",
    "rhi/dx12/**",
}
filter {}

-- Which backends are compiled in — read by rhi/RendererBackendFactory.cpp
-- to validate --rhi= and pick a fallback.
filter "system:windows"
defines { "D12F_RHI_HAS_DX12" }
filter {}
-- Linux only for now. rhi/vulkan/ creates its surface through
-- VK_KHR_wayland_surface, so a Windows build would also need the
-- VK_KHR_win32_surface path before this can be defined there.
filter "system:linux"
defines { "D12F_RHI_HAS_VULKAN" }
filter {}

-- Per-OS linking uses a plain Lua `if`, NOT a premake filter. Helpers like
-- mox_link_vcpkg() end with `filter {}` internally, which would clear an
-- enclosing `filter "system:windows"` and let the Windows .libs leak into
-- the Linux link line (verified: that's exactly what happened). The target
-- OS is fixed at generation time anyway, so branching in Lua is both
-- correct and clearer here.
if _OPTIONS["mox_target_os"] == "linux" then
    -- Wayland's xdg-shell protocol bindings are generated from XML once
    -- at `mox init` (see init.py GenerateWaylandProtocols) rather than per
    -- build: they're a fixed API surface pinned by the wayland-protocols
    -- version in conanfile.py, not output derived from our sources.
    files {
        "%{wks.location}/dependencies/wayland/*.c",
    }
    includedirs {
        "%{wks.location}/dependencies/wayland",
    }

    -- Conan's PremakeDeps generator reports empty includedirs/libdirs/libs
    -- for wayland, because that recipe publishes its output as components
    -- (wayland-client, wayland-server, ...) and the generator doesn't
    -- flatten those. The conan deploy layout is deterministic though, so
    -- resolve it directly rather than hand-maintaining a version number.
    -- Anchored to the repo root (premake chdirs into each project's
    -- directory while evaluating its build.lua, so a relative glob would
    -- resolve against src/DXFoliage instead).
    local repoRoot = path.getabsolute(_MAIN_SCRIPT_DIR .. "/..")

    -- Adds a conan package's include/ and lib/ directories. `required`
    -- distinguishes "this must be there or the build is broken" from
    -- transitive packages that may or may not be deployed separately.
    local function useConanPackage(name, required)
        local root = repoRoot .. "/dependencies/full_deploy/host/" .. name .. "/*/*/*"
        local includes = os.matchdirs(root .. "/include")
        local libs = os.matchdirs(root .. "/lib")
        if #includes == 0 and #libs == 0 then
            if required then
                error(name .. " not found under dependencies/full_deploy - run `mox init` first")
            end
            return false
        end
        if #includes > 0 then externalincludedirs { includes[1] } end
        if #libs > 0 then libdirs { libs[1] } end
        return true
    end

    useConanPackage("wayland", true)

    -- shaderc (HLSL -> SPIR-V) is a static library built on glslang and
    -- SPIRV-Tools. Conan deploys those as separate packages, but linking
    -- them individually invites duplicate-symbol trouble: libshaderc.a
    -- and libshaderc_combined.a both define the shaderc API, and
    -- libshaderc_combined.a already has glslang and SPIRV-Tools archived
    -- into it. So link the combined library alone.
    useConanPackage("shaderc", true)
    links { "shaderc_combined" }

    links {
        "wayland-client",
        "vulkan",
    }

    -- The Vulkan backend compiles HLSL at pipeline-creation time, so the
    -- .hlsl files have to be findable at runtime. VulkanShaderCompiler
    -- looks for shaders/ next to the binary, so put them there.
    --
    -- Copied straight from the source tree rather than from the
    -- app/<project>/ staging directory the **.hlsl rule below fills.
    -- Both work, but that rule stages for a debugger whose working
    -- directory is app/ (see debugdir in libmox.lua), which is a
    -- different destination than "next to the binary" — chaining off it
    -- would make this copy depend on an unrelated rule's layout.
    postbuildcommands {
        'mkdir -p "%{cfg.targetdir}/shaders"',
        'cp -r "%{prj.location}/shaders/." "%{cfg.targetdir}/shaders/"',
    }

    -- The generated protocol bindings are C, and can't consume the
    -- project's C++ precompiled header ("C99 was disabled in precompiled
    -- file ... but is currently enabled").
    filter { "files:**/dependencies/wayland/*.c" }
    flags { "NoPCH" }
    filter {}
else
    mox_link_vcpkg("DirectXTK12")
    links {
        "d3d12.lib",
        "dxgi.lib",
        "dxcompiler.lib",
        "dxguid.lib",
        "winmm.lib",
        "user32.lib",
        "comctl32.lib",
        "delayimp.lib",
        "assimp-vc142-mt.lib",
        "imgui.lib",
        "OpenEXRUtil-3_4.lib",
        "OpenEXR-3_4.lib",
        "IlmThread-3_4.lib",
        "OpenEXRCore-3_4.lib",
        "Iex-3_4.lib",
        "Imath-3_2.lib"
    }
end
-- Wayland/xkbcommon come from system packages via pkg-config rather than
-- vcpkg (see mox_link_pkgconfig's comment for why).
mox_link_pkgconfig("wayland-client")
mox_link_pkgconfig("xkbcommon")

pchheader "stdafx.h"
pchsource "stdafx.cpp"

-- Linker options. These are all MSVC-linker flags, so they're gated on
-- the target system (NOT the action — gmake2 is also used to build *for
-- Windows* via clang, which still needs them).
--
-- /ENTRY:mainCRTStartup tells the CRT startup to call main(argc, argv)
-- instead of WinMain(HINSTANCE, ...) — main.cpp defines a plain
-- int main(int, char**) so the same source builds on both platforms.
-- /SUBSYSTEM:WINDOWS is unaffected: the process still launches with no
-- console auto-created, exactly as before.
filter { "system:windows", "action:vs*" }
linkoptions {
    "/DELAYLOAD:d3d12.dll",
    "/DELAYLOAD:dxcompiler.dll",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:mainCRTStartup",
}
filter {}
filter { "system:windows", "action:gmake or gmake2" }
linkoptions {
    "-Xlinker /SUBSYSTEM:WINDOWS",
    "-Xlinker /DELAYLOAD:d3d12.dll",
    "-Xlinker /DELAYLOAD:dxcompiler.dll",
    "-Xlinker /ENTRY:mainCRTStartup",
}
filter {}

filter { "action:vs*", "configurations:Debug" }
linkoptions { "/INCREMENTAL" }
filter {}
filter { "action:vs*", "configurations:Release" }
linkoptions { "/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF" }
filter {}

filter { "files:**.hlsl" }
buildaction "CustomBuild"
buildoutputs {
    "%{wks.location}/app/%{prj.name}/%{file.reldirectory}/%{file.name}"
}
buildcommands {
    'mkdir -p "%{wks.location}/app/%{prj.name}/%{file.reldirectory}"',
    'cp "%{file.relpath}" "%{wks.location}/app/%{prj.name}/%{file.reldirectory}/%{file.name}"'
}
linkbuildoutputs "false"
filter {}

filter { "files:**.hlsli" }
buildaction "CustomBuild"
buildoutputs {
    "%{wks.location}/app/%{prj.name}/%{file.reldirectory}/%{file.name}"
}
buildcommands {
    'mkdir -p "%{wks.location}/app/%{prj.name}/%{file.reldirectory}"',
    'cp "%{file.relpath}" "%{wks.location}/app/%{prj.name}/%{file.reldirectory}/%{file.name}"'
}
linkbuildoutputs "false"
filter {}

-- Use the following to build after other projects
-- dependson {
--     "ProjectName",
--     "ProjectName2",
-- }
