# Compiling

## 1. CMake

### 1.1. Example

In libdumb project root, run:
```
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS:BOOL=ON ..
make
make install
```

### 1.2. Steps

1. Create a new temporary build directory and cd into it
2. Run libdumb cmake file with cmake (eg. `cmake -DCMAKE_INSTALL_PREFIX=/install/dir -DBUILD_SHARED_LIBS:BOOL=ON -DCMAKE_BUILD_TYPE=Release path/to/dumb/cmake/dir`).
3. Run make (eg. just `make` or `mingw32-make` or something).
4. If needed, run make install.

### 1.3. Flags

* CMAKE_INSTALL_PREFIX sets the installation path prefix
* CMAKE_BUILD_TYPE sets the build type (eg. Release, Debug, RelWithDebInfo, MinSizeRel). Debug libraries will be named libdumbd, release libraries libdumb.
* BUILD_SHARED_LIBS selects whether cmake should build dynamic or static library. On by default. (On=shared, OFF=static)
* BUILD_EXAMPLES selects example binaries. Note that example binaries require argtable2 and SDL2 libraries. On by default.
* BUILD_ALLEGRO4 enables allegro 4 support libraries to be built. On by default.
* USE_SSE enables or disables SSE instructions usage. On by default.
* You may also need to tell cmake what kind of makefiles to create with the "-G" flag. Eg. for MSYS one would say something like `cmake -G "MSYS Makefiles" .`.

## 2. Visual Studio

TODO
