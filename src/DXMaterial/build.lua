-- Arguments: Project name, Output name, Output directory (binary only)
mox_project("DXMaterial", "dx_material", "bin/DXMaterial/")
mox_cpp("C++20")
mox_windowed()
mox_use_vcpkg()
uuid("d54bdce0-8b62-11f0-b558-0800200c9a66")

architecture "x64"
system "windows"
systemversion "10.0.28000.0:latest"
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
    "user32.lib",
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
