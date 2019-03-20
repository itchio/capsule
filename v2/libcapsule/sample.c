#include <windows.h>
#include <stdio.h>

int main() {
    HMODULE mod = LoadLibraryW(L".\\target\\debug\\capsule.dll");
    if (!mod) {
        fprintf(stderr, "Failed to load DLL, last error: %08lx", GetLastError());
        return 1;
    }
}
