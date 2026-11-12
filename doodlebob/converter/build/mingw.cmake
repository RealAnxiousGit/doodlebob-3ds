# Cross-compiles tools (the preprocessor) for 64-bit Windows from Ubuntu with mingw-w64, statically linked.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc")
