#include <stdafx.h>
#include <app.h>

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    app app(1366, 768, L"Hello World", hInstance, nCmdShow);

    app.OnInit();

    app.Run();

    return EXIT_SUCCESS;
}
