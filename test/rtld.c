#include <dlfcn.h>
#include <stdio.h>
#include <OpenGL/OpenGL.h>

int main() {
    printf("RTLD_LAZY = %x\n", RTLD_LAZY);
    printf("RTLD_NOLOAD = %x\n", RTLD_NOLOAD);
    void *handle = dlopen("/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib", RTLD_LAZY | RTLD_NOLOAD);
    printf("handle = %p\n", handle);

    CGLFlushDrawable(0);
    printf("called CGLFlushDrawable\n");
    return 0;
}

