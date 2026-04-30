#include "stdafx.h"
#include "app.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    argRecieved[static_cast<size_t>(ARG_CONSOLE_PID)] = std::to_wstring(ATTACH_PARENT_PROCESS);

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int itrArgc = 1; itrArgc < argc; ++itrArgc)
    {
        for (int itrArgA{}; itrArgA < ARG_MAX; ++itrArgA)
        {
            std::wstring& arg = argAccept[itrArgA];

            if (wcsncmp(argv[itrArgc], arg.data(), arg.size()) == 0)
            {
                argRecieved[static_cast<size_t>(itrArgA)] = argv[itrArgc] + arg.size();
            }
        }
    }
    LocalFree(argv);

    app app(1280, 720, L"DXTerrain", hInstance, nCmdShow);
    app.OnInit();

    return app.Run();
}
