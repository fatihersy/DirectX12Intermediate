
mox_project("DXMaterial", "dx_material")
mox_cpp("C++20")
mox_windowed()
uuid("d54bdce0-8b62-11f0-b558-0800200c9a66")

architecture "x64"
system "windows"
systemversion "10.0.26100.0:latest"
warnings "Default"
buffersecuritycheck "on"
fatalwarnings { "warnings" }
multiprocessorcompile "On"

files {
    "**.h",
    "**.cpp",
    "**.hlsl"
}

links {
    "d3d12.lib",
    "dxgi.lib",
    "d3dcompiler.lib",
    "dxguid.lib",
    "winmm.lib",
    "comctl32.lib"
}

pchheader "stdafx.h"
pchsource "stdafx.cpp"
    
filter "configurations:*"
    linkoptions { 
        "/DELAYLOAD:d3d12.dll", 
        "/SUBSYSTEM:WINDOWS",
    }
filter {} 
    
filter "configurations:Debug"
    linkoptions { "/INCREMENTAL" }
filter {}    

filter "configurations:Release"
    linkoptions { "/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF" }
filter {}

filter "action:vs*"
    defines { "_CRT_SECURE_NO_WARNINGS" }
filter {}

filter "files:shader.hlsl"
    buildaction "CustomBuild"
    buildoutputs { "%{cfg.targetdir}/%{file.name}" }
    buildcommands { 'copy "%{file.relpath}" "%{cfg.targetdir}/%{file.name}" > NUL' }
    linkbuildoutputs "false"
filter {}

-- Use the following to build after other projects
-- dependson {
--     "ProjectName",
--     "ProjectName2",
-- }
