#include "Editor/Editor.h"

#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

int main()
{
    try {
        Editor editor;
        editor.run();
    }
    catch (const std::exception& e) {
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "Digging Jim Builder", MB_ICONERROR | MB_OK);
#endif
        return 1;
    }
    return 0;
}
