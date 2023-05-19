# TIVTC and TDeInt

Current versions:

- TIVTC: v1.0.27 (20230511)
- TDeint: v1.8 (20201214)

This is a modernization effort on tritical's TIVTC (v1.0.5) and TDeInt (v1.1.1) plugin for Avisynth by pinterf.

All credit goes to tritical, thanks for his work.

Since December 27th 2020 project can be built under Linux and macOS (x86/x64 only) as well. For build instructions see end of this readme.

## Links

- General filter info: http://avisynth.nl/index.php/TIVTC
- This mod is based on the original code http://web.archive.org/web/20140420181748/http://bengal.missouri.edu/~kes25c/TIVTCv105.zip
- Project source: https://github.com/pinterf/TIVTC
- Doom9 topic: https://forum.doom9.org/showthread.php?t=82264

## Build Instructions

Note: ENABLE_INTEL_SIMD does nothing, this plugin cannot be built on non-x86 architectures

### Windows Visual Studio MSVC

* build from IDE

### Windows GCC

(mingw installed by msys2)
Note: project root is TIVTC/src

From the 'build' folder under project root:

    del ..\CMakeCache.txt
    cmake .. -G "MinGW Makefiles" -DENABLE_INTEL_SIMD:bool=on
    @rem test: cmake .. -G "MinGW Makefiles" -DENABLE_INTEL_SIMD:bool=off
    cmake --build . --config Release  

### Linux build instructions

* Clone repo

      git clone https://github.com/pinterf/TIVTC
      cd TIVTC/src
      cmake -B build -S .
      cmake --build build

  Useful hints:        

  build after clean:

      cmake --build build --clean-first

  delete CMake cache

      rm build/CMakeCache.txt

* Find binaries at

      build/TIVTC/libtivtc.so
      build/TDeint/libtdeint.so

* Install binaries

      cd build
      sudo make install
