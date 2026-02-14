
mox_project("DXMaterial", "dx_material")
mox_cpp("C++23")
mox_windowed()
mox_use_vcpkg()
uuid("d54bdce0-8b62-11f0-b558-0800200c9a66")

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
    "assimp-vc143-mt.lib",
    "imgui.lib"
}

pchheader "stdafx.h"
pchsource "stdafx.cpp"
    
filter "configurations:*"
    linkoptions { 
        "/DELAYLOAD:d3d12.dll", 
        "/DELAYLOAD:dxcompiler.dll", 
        "/SUBSYSTEM:WINDOWS",
    }
filter {} 
    
filter "configurations:Debug"
    linkoptions { "/INCREMENTAL" }
filter {}    

filter "configurations:Release"
    linkoptions { "/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF" }
filter {}

filter "files:VS.hlsl"
    buildaction "CustomBuild"
    buildoutputs { "%{wks.location}/app/%{file.name}" }
    buildcommands { 'copy "%{file.relpath}" "%{wks.location}/app/%{file.name}" > NUL' }
    linkbuildoutputs "false"
filter {}

filter "files:PS.hlsl"
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
