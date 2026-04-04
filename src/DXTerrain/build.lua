-- Arguments: Project name, Output name, Output directory (binary only)
mox_project("DXTerrain", "dx_terrain", "bin/DXTerrain/")
mox_cpp("C++20")
mox_windowed()
mox_use_vcpkg()
uuid("356f4221-1407-43e9-879d-4bac273b14cc")

architecture "x64"
system "windows"
systemversion "10.0.26100.0:latest"
warnings "Default"
buffersecuritycheck "On"
fatalwarnings { "All" }
multiprocessorcompile "On"

filter "action:vs*"
--buildoptions { "/ZW" } Not supported with C++23 Preview and premake5 beta8 forces to that
buildoptions { "/sdl" }
filter {}

files {
    "**.h",
    "**.cpp",
    "**.hlsl"
}

mox_link_vcpkg("DirectXTK12")

links {
    "d3d12.lib",
    "dxgi.lib",
    "dxcompiler.lib",
    "dxguid.lib",
    "winmm.lib",
    "comctl32.lib",
    "delayimp.lib",
    "assimp-vc142-mt.lib",
    "imgui.lib"
}

pchheader "stdafx.h"
pchsource "stdafx.cpp"

-- Linker options: VS uses MSVC-style flags directly; gmake must wrap them
-- with -Xlinker so clang++ forwards them to lld-link / link.exe.
filter { "action:vs*", "configurations:*" }
linkoptions {
    "/DELAYLOAD:d3d12.dll",
    "/DELAYLOAD:dxcompiler.dll",
    "/SUBSYSTEM:WINDOWS",
}
filter {}
filter { "action:gmake or gmake2", "configurations:*" }
linkoptions {
    "-Xlinker /SUBSYSTEM:WINDOWS",
    "-Xlinker /DELAYLOAD:d3d12.dll",
    "-Xlinker /DELAYLOAD:dxcompiler.dll",
}
filter {}

filter { "action:vs*", "configurations:Debug" }
linkoptions { "/INCREMENTAL" }
filter {}
filter { "action:vs*", "configurations:Release" }
linkoptions { "/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF" }
filter {}

filter "files:**.hlsl"
buildaction "CustomBuild"
buildoutputs { "%{wks.location}/app/%{file.name}" }
buildcommands { 'copy "%{file.relpath}" "%{wks.location}/app/%{file.name}" > NUL' }
linkbuildoutputs "false"
filter {}

-- Use the following to build after other projects
-- dependson {
--     "ProjectName",
--     "ProjectName2",
-- }
