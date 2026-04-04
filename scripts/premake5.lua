-- premake5.lua root script with vcpkg support
--
-- Copyright (c) 2025 Moxibyte GmbH

include "libmox.lua"
include "../dependencies/conandeps.premake5.lua"
include "../mox.lua"

hmox_test_requirements = {}

newoption {
    trigger = "mox_premake_arch",
    value = "ARCH",
    description = "Defines the architecture to use for building the project (premake native)",
    category = "MoxPP",
    allowed = {
        { "x86", "32-Bit x86" },
        { "x86_64", "64-Bit x86" },
        { "ARM", "32-Bit ARM" },
        { "ARM64", "64-Bit ARM" },
    },
    default = "x86_64"
}
newoption {
    trigger = "mox_conan_arch",
    value = "ARCH",
    description = "Defines the architecture to use by the dependencies",
    category = "MoxPP",
    allowed = {
        { "x86", "32-Bit x86" },
        { "x86_64", "64-Bit x86" },
        { "armv7", "armv7 (32-Bit)" },
        { "armv8", "armv8 (64-Bit)" },
    },
    default = "x86_64"
}
newoption {
    trigger = "mox_gcc_prefix",
    value = "PREFIX",
    description = "GCC Prefix for cross compiling",
    category = "MoxPP",
    default = "none"
}
newoption {
    trigger = "mox_version",
    value = "VERSION",
    description = "Set the version string injected as preprocessor macro",
    category = "MoxPP",
    default = "unknown"
}
newoption {
    trigger = "mox_conan_release_only",
    value = "CONAN_RELEASE_ONLY",
    description = "Indicates that conan only generated release dependencies",
    category = "MoxPP",
    allowed = {
        { "True", "True" },
        { "False", "False" },
    },
    default = "False"
}
newoption {
    trigger = "mox_vcpkg_root",
    value = "PATH",
    description = "Path to vcpkg root directory",
    category = "MoxPP",
    default = ""
}
newoption {
    trigger = "mox_vs_toolset",
    description = "Visual Studio platform toolset (e.g. v143, v142, v141)",
    category = "MoxPP",
}
newoption {
    trigger = "mox_compiler",
    description = "Compiler to use for makefile builds (msvc, gcc, clang, ClangCL)",
    category = "MoxPP",
}
newoption {
    trigger = "mox_target_os",
    value = "OS",
    description = "Target operating system (windows, linux)",
    category = "MoxPP",
    default = "windows"
}

-- Extract if conan is release only
hmox_conan_release_only = _OPTIONS["mox_conan_release_only"] == "True"

-- vcpkg configuration
if _OPTIONS["mox_vcpkg_root"] and _OPTIONS["mox_vcpkg_root"] ~= "" then
    hmox_vcpkg_root = _OPTIONS["mox_vcpkg_root"]

    -- Derive vcpkg triplet from architecture and target OS
    local vcpkg_arch_map = { x86 = "x86", x86_64 = "x64", ARM = "arm", ARM64 = "arm64" }
    local vcpkg_arch = vcpkg_arch_map[_OPTIONS["mox_premake_arch"]] or "x64"
    hmox_vcpkg_triplet = vcpkg_arch .. "-" .. _OPTIONS["mox_target_os"]

    -- vcpkg_installed is passed as root, packages are at {triplet}/include, etc.
    _G.vcpkg = {
        root = hmox_vcpkg_root,
        triplet = hmox_vcpkg_triplet,
        includePath = hmox_vcpkg_root .. "/" .. hmox_vcpkg_triplet .. "/include",
        libPath = hmox_vcpkg_root .. "/" .. hmox_vcpkg_triplet .. "/lib",
        debugLibPath = hmox_vcpkg_root .. "/" .. hmox_vcpkg_triplet .. "/debug/lib",
        binPath = hmox_vcpkg_root .. "/" .. hmox_vcpkg_triplet .. "/bin",
        debugBinPath = hmox_vcpkg_root .. "/" .. hmox_vcpkg_triplet .. "/debug/bin"
    }
    
    print("vcpkg configured at: " .. hmox_vcpkg_root)
else
    _G.vcpkg = nil
end

-- Helper function for projects to use vcpkg
function mox_use_vcpkg()
    if _G.vcpkg == nil then
        print("Warning: vcpkg not configured. Skipping vcpkg includes/libs.")
        return
    end

    -- For clang/gcc with makefiles, use -isystem so vcpkg headers
    -- take priority over the compiler's bundled system headers.
    -- Also add the "directx" subdirectory since vcpkg's directx-headers
    -- package installs headers under include/directx/ (e.g. directx/d3d12.h)
    -- but code uses #include <d3d12.h> directly.
    if _ACTION == "gmake" or _ACTION == "gmake2" then
        externalincludedirs {
            _G.vcpkg.includePath .. "/directx",
            _G.vcpkg.includePath,
        }
    else
        includedirs { _G.vcpkg.includePath }
    end

    filter "configurations:Debug"
        libdirs { _G.vcpkg.debugLibPath }
    filter {}

    filter "configurations:Release"
        libdirs { _G.vcpkg.libPath }
    filter {}
end

-- Helper function to link vcpkg library
function mox_link_vcpkg(libname)
    if _G.vcpkg == nil then
        print("Warning: vcpkg not configured. Skipping vcpkg library: " .. libname)
        return
    end
    
    filter "configurations:Debug"
        links { libname .. ".lib" }
    filter {}
    
    filter "configurations:Release"
        links { libname .. ".lib" }
    filter {}
end

-- Make helper functions global
_G.mox_use_vcpkg = mox_use_vcpkg
_G.mox_link_vcpkg = mox_link_vcpkg

workspace(cmox_product_name)
    -- Workspace configuration
    configurations(cmox_configurations_n)
    architecture(_OPTIONS["mox_premake_arch"])
    location "../"

    -- Set platform toolset if specified (Visual Studio only)
    if _ACTION and string.startswith(_ACTION, "vs") then
        if _OPTIONS["mox_vs_toolset"] and _OPTIONS["mox_vs_toolset"] ~= "" then
            toolset("msc-" .. _OPTIONS["mox_vs_toolset"])
        end
    end

    -- Custom workspace configuration
    if cmox_function_setupworkspace~=nil then
        cmox_function_setupworkspace()
    end

    -- Load projects
    if cmox_project_architecture == "single" then
        hmox_project_dir = "../" .. cmox_src_folder .. "/"
        include(hmox_project_dir .. "build.lua")
    elseif cmox_project_architecture == "flat" then
        for dir in mox_discover_subfolders("../" .. cmox_src_folder)
        do
            hmox_project_dir = "../" .. cmox_src_folder .. "/" .. dir .. "/"
            local buildFile = hmox_project_dir .. "build.lua"
            if os.isfile(buildFile) then
                include(buildFile)
            end
        end
    elseif cmox_project_architecture == "hierarchical" then
        for dir in mox_discover_subfolders("../" .. cmox_src_folder)
        do
            group(dir)
            for subdir in mox_discover_subfolders("../" .. cmox_src_folder .. "/" .. dir)
            do
                hmox_project_dir = "../" .. cmox_src_folder .. "/" .. dir .. "/" .. subdir .. "/"
                local buildFile =  hmox_project_dir .. "build.lua"
                if os.isfile(buildFile) then
                    include(buildFile)
                end
            end
        end
    elseif cmox_project_architecture == "manual" then
        cmox_function_includeprojects()
    end

    -- Unittest
    if cmox_unit_test_src ~= nil then
        include("../" .. cmox_unit_test_src .. "/build.lua")
    end
    