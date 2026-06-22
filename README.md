# F-15 Strike Eagle 2 source port

This is a work in progress project to port, fix and enhance the [reconstructed source code](https://github.com/neuviemeporte/f15se2-re) of the Microprose game F-15 Strike Eagle 2 v451.03 (the definitive 1991 Desert Storm expansion disk version) on modern architectures.

Unlike the old project, whose aim was a bug-for-bug, instruction-level faithful recreation of the game for the orginal MS-DOS platform and the MS C v5.1 compiler, here we aim to keep as much of the game's spirit intact, while taking it forward into the 21st century with modern language features, better graphics and enhanced features.

This is still in an extremely early stage, code needs moving from the old project and adapting into the new framework, which will happen gradually.

The project is based on the [SDL3 library](https://github.com/libsdl-org/SDL/releases) for the graphical frontend. Initially, we're going to keep the original, software-based 3d rendering engine that outputs to a flat framebuffer, but the goal is to switch to a modern 3D rendering API like OpenGL or Vulkan at some point in the future.

## Progress

None, just starting.

## Building

The build system is [CMake](https://cmake.org/download/) with [Ninja](https://github.com/ninja-build/ninja/releases) being used as the generator backend. It is intended to be built with Clang. To build, run:

```
cmake --preset <preset-name>
cmake --build build
```

The project includes the default preset `base-ninja` in `CMakePresets.json`. It's possible to manually override platform-specific values for the build using CMake's user presets. For example, to build on Windows using [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), I use this `CMakeUserPresets.json`:

```
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-clang",
      "inherits": "base-ninja",
      "displayName": "Local Windows Clang + SDL3",
      "environment": {
        "CC": "D:/utility/llvm-mingw-20260616-ucrt-x86_64/bin/clang.exe",
        "CXX": "D:/utility/llvm-mingw-20260616-ucrt-x86_64/bin/clang++.exe"
      },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_PREFIX_PATH": "D:/code/SDL3-3.4.10/x86_64-w64-mingw32"
      }
    }
  ]
}
```

With this, I run `cmake --preset windows-clang` followed by `cmake --build build` to obtain `build/f15se2.exe`. It also needs `SDL3.dll` from `SDL3-3.4.10\x86_64-w64-mingw32\bin` in the same directory to run.