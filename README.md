<div align="center">
  <img src="https://btzy.github.io/orbital/Circuit%20Sandbox.png"><br/><br/>
</div>

**Circuit Sandbox** is a digital circuit sandbox game where users can create arbitrarily complex circuits out of simple components.  Circuit Sandbox is designed from the ground up to support large and complex simulations efficiently.

Circuit Sandbox is currently in development.

## Download binaries

Pre-built binaries are [available for download](https://github.com/btzy/circuit-sandbox/releases) for Windows 10.  They should run straight out of the box, no installation required.

If you are on Windows 8.1, you might need to install the [Windows 10 Universal C Runtime](https://www.microsoft.com/en-us/download/details.aspx?id=48234) before running Circuit Sandbox.

For other operating systems, you'll have to build it from source.

## How to play

Read the [user manual](https://btzy.github.io/circuit-sandbox/manuals/latest.pdf) for an introduction to Circuit Sandbox.

There are also sample circuits in the `samples` folder of this repository.

## Build from source

A reasonably conforming C++17 compiler is required to compile Circuit Sandbox.

Circuit Sandbox has been tested to compile with Visual Studio 2017 (MSVC 15.7) on Windows and GCC 7.3 on Linux.  It might also work with MinGW-w64 on Windows, but a few tweaks might be needed to get it use newer versions of `Windows.h`.

It probably also compiles on Mac OSX with Clang, but this is untested.

*Note: The C runtime has to be linked dynamically due to how Native File Dialog works.*

### Dependencies

* [Boost libraries](https://www.boost.org/)
  * Headers: Tribool, Process, Endian, Interprocess
  * Link Libraries: System, Filesystem
* [SDL2](https://www.libsdl.org/download-2.0.php)
* [SDL2_ttf](https://www.libsdl.org/projects/SDL_ttf/)
* [Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) (NFD)

### Building on Windows (Visual Studio 2017)

The Windows releases are built with Boost 1.67.0, SDL2 2.0.8, SDL2_ttf 2.0.14, and the latest NFD from its master branch.  Boost and NFD are statically linked to Circuit Sandbox; SDL2 and SDL2_ttf are dynamically linked as recommended by SDL2.  The C and C++ runtimes are dynamically linked.  To minimise unexpected issues, it is best to build it in the same way.

1. Set-up all the dependencies:

    1. Download Boost and follow [these instructions](https://www.boost.org/doc/libs/1_67_0/more/getting_started/windows.html) to compile Boost on your computer.  Make sure you compile it using the correct toolset (`toolset=msvc-14.1`).  You might also want to build static libraries (`link=static`).
    2. Download SDL2 and SDL2_ttf, making sure you get the development libraries (not the runtime binaries), and that they are for Visual Studio (not MinGW).  Those downloads contain binaries (.dll and .lib), so there is nothing to build.  Just unzip the files and place them somewhere on your computer.
    3. Download or clone NFD onto your computer, and use the Visual Studio solution and project files  provided by NFD to build it.  You don't need to configure anything - they should build correctly out of the box.

2. Open the solution file for Circuit Sandbox in Visual Studio, and define the following [property macros](https://docs.microsoft.com/en-us/cpp/ide/working-with-project-properties#bkmkPropertySheets) that point to the include directories and linker search directories from step 1 (their values will depend on where you have placed the dependencies):

    * `Boost_Include`
    * `SDL_Include`
    * `SDLTTF_Include`
    * `NFD_Include`
    * `Boost_Lib_Debug_x86`
    * `Boost_Lib_Release_x86`
    * `SDL_Lib_Release_x86`
    * `SDLTTF_Lib_Release_x86`
    * `NFD_Lib_Debug_x86`
    * `NFD_Lib_Release_x86`
    * `Boost_Lib_Debug_x64`
    * `Boost_Lib_Release_x64`
    * `SDL_Lib_Release_x64`
    * `SDLTTF_Lib_Release_x64`
    * `NFD_Lib_Debug_x64`
    * `NFD_Lib_Release_x64`

3. Build Circuit Sandbox; it should work!
### Building on Mac (ARM)

```
mkdir dependencies && cd dependencies
wget https://archives.boost.io/release/1.85.0/source/boost_1_85_0_rc3.tar.gz && tar -xf boost_1_85_0_rc3.tar.gz
wget https://github.com/libsdl-org/SDL/releases/download/release-2.30.5/SDL2-2.30.5.tar.gz && tar -xf SDL2-2.30.5.tar.gz 
wget https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz && tar -xf SDL2_ttf-2.22.0.tar.gz 

# Note: wrong version of NFD, but use btzy's
wget https://github.com/mlabbe/nativefiledialog/archive/refs/tags/release_116.tar.gz && tar -xf release_116.tar.gz

# correct
wget https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v1.2.1.tar.gz -O btzy.tar.gz && tar -xf btzy.tar.gz

brew install sdl2
brew --prefix sdl2
g++ -S CircuitSandbox/main.cpp --verbose
g++ -S CircuitSandbox/main.cpp --verbose -I`brew --prefix sdl2`/include/SDL2
g++ -S CircuitSandbox/main.cpp --verbose -I ./dependencies/SDL2-2.30.5/include/ -I ./dependencies/SDL2_ttf-2.22.0/ 
g++ --std=c++17 -S CircuitSandbox/main.cpp --verbose -I ./dependencies/SDL2-2.30.5/include/ -I ./dependencies/SDL2_ttf-2.22.0/ -I ./dependencies/nativefiledialog-release_116/src/include/ 
g++ --std=c++17 -S CircuitSandbox/main.cpp --verbose -I ./dependencies/SDL2-2.30.5/include/ -I ./dependencies/SDL2_ttf-2.22.0/ -I ./dependencies/nativefiledialog-release_116/src/include/ -I dependencies/boost_1_85_0/

# works
g++ --std=c++17 -S CircuitSandbox/main.cpp --verbose -I ./dependencies/SDL2-2.30.5/include/ -I ./dependencies/SDL2_ttf-2.22.0/ -I ./dependencies/boost_1_85_0/ -I ./dependencies/nativefiledialog-extended-1.2.1/src/include/

cd dependencies/nativefiledialog-extended-1.2.1
cmake .
make -j4

cd dependencies/SDL2-2.30.5 
mkdir build && cd build
cmake -S /Users/bytedance/Documents/circuit-sandbox/dependencies/SDL2-2.30.5 -B .
make -j4

g++ main.s 
g++ main.s -L ./dependencies/SDL2-2.30.5/build/ --verbose -lSDL2
g++ main.s -L ./dependencies/SDL2-2.30.5/build/ --verbose -lSDL2 -ObjC -framework Cocoa

as -arch arm64 -o main.o main.s
g++ main.o -L ./dependencies/SDL2-2.30.5/build/ -lSDL2 -ObjC -framework Cocoa -arch arm64  --verbose

g++ main.o -L ./dependencies/SDL2-2.30.5/build/ -lSDL2 -ObjC -framework Cocoa -arch arm64 --verbose -framework CoreAudio -framework AudioToolbox -framework CoreHaptics -framework CoreVideo -framework ForceFeedback -framework GameController -framework Metal -framework IOKit -framework Carbon -lSDL2_ttf -L ./dependencies/SDL2_ttf-2.22.0/build

brew install freetype 
cd dependencies/SDL2_ttf-2.22.0
mkdir build && cd build
cmake ..
make -j4

g++ main.o -L ./dependencies/SDL2-2.30.5/build/ -lSDL2 -ObjC -framework Cocoa -arch arm64 --verbose -framework CoreAudio -framework AudioToolbox -framework CoreHaptics -framework CoreVideo -framework ForceFeedback -framework GameController -framework Metal -framework IOKit -framework Carbon -lSDL2_ttf -L ./dependencies/SDL2_ttf-2.22.0/build -lNFD -L ./dependencies/nativefiledialog-extended-1.2.1/src/
```

Then go download [X11](https://www.xquartz.org/) for mac

Idk why this doesn't work? 
```
g++ --std=c++17 -S CircuitSandbox/main.cpp --verbose -I ./dependencies/SDL2-2.30.5/include/ -I ./dependencies/SDL2_ttf-2.22.0/ -I ./dependencies/boost_1_85_0/ -I ./dependencies/nativefiledialog-extended-1.2.1/src/include/ -S CircuitSandbox/x11_dpi_scaling.cpp -I /usr/X11/include/ -D __APPLE__
```

Use `make -r` to avoid [Implicit Rules](https://web.mit.edu/gnu/doc/html/make_10.html#SEC88)

## Licensing

Circuit Sandbox is licensed under the GNU General Public License version 3.  For the complete license text, see the file 'COPYING'. 

This license applies to all files in this repository, except the following:

* CircuitSandbox/resources/ButtonIcons.ttf and everything in the CircuitSandbox/resources/icons directory, which are from [Google Material Icons](https://material.io/tools/icons/) and licensed under the Apache License version 2.0.
* CircuitSandbox/resources/OpenSans-Bold.ttf, which is from the [Open Sans](https://fonts.google.com/specimen/Open+Sans) typeface and licensed under the Apache License version 2.0.
* CircuitSandbox/unicode.hpp, which is from [Unicode Utilities](https://github.com/btzy/unicode-utilities) and licensed under the MIT License.

In addition to the items stated above, Circuit Sandbox releases also contain binary code from SDL 2.0, SDL_ttf 2.0, Boost C++ Libraries, Native File Dialog Extended, and compiler runtimes for MSVC (Windows), Clang (Mac OS X), and GCC (Linux).  SDL_ttf 2.0 further depends on FreeType and Zlib, which are also packaged with the releases.  They have their own licenses, which may be different from the license of Circuit Sandbox.

## Credits

* [Bernard Teo Zhi Yi](https://github.com/btzy)
* [Kuan Wei Heng](https://github.com/xsot)

This project was built for [CP2106 Independent Software Development Project (Orbital)](https://orbital.comp.nus.edu.sg/) at the National University of Singapore (NUS).  NUS owns the copyright for all source code and assets up to and including the v0.4 release.
