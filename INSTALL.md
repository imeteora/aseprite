# What platforms are supported?

You should be able to compile Aseprite successfully on the following
platforms:

* Windows + MSVC 2012 + DirectX SDK
* Mac OS X 10.8 Mountain Lion + Xcode 5.1.1 + Mac OS X 10.4 SDK universal
* Linux + gcc with some C++11 support, this port is not compiled
  regularly so you can expect some errors in the master branch.

# How can I compile Aseprite?

Aseprite uses the latest version of [CMake](http://www.cmake.org/)
(3.0) as its build system. You will not need any extra library
because the repository already contains the source code of all
dependencies, even a modified version of the Allegro library is
included in master branch.

The following are the steps to compile Aseprite (in this case we have
the source code in a directory called `aseprite-source`):

1. Make a build directory to leave all the files that are result of
   the compilation process (`.exe`, `.lib`, `.obj`, `.a`, `.o`, etc).

        C:\...\>cd aseprite-source
        C:\...\aseprite-source>mkdir build

   In this way, if you want to start with a fresh copy of Aseprite
   source code, you can remove the `build` directory and start again.

2. Enter in the new directory and execute cmake giving to it
   your compiler as generator:

        C:\...\aseprite-source>cd build

   If you have nmake (MSVC compilers):

        C:\...\aseprite-source\build>cmake .. -G "NMake Makefiles"

   If you have Visual Studio you can generate a solution:

        C:\...\aseprite-source\build>cmake .. -G "Visual Studio 11 2012"
        C:\...\aseprite-source\build>cmake .. -G "Visual Studio 12 2013"

   If you are on Linux:

        /.../aseprite-source/build$ cmake .. -G "Unix Makefiles"

   For more information in [CMake wiki](http://www.vtk.org/Wiki/CMake_Generator_Specific_Information).
    
3. After you have executed one of the `cmake .. -G <generator>`
   commands, you have to compile the project executing make, nmake,
   opening the solution, etc.

4. When the project is compiled, you can find the executable file
   inside `build/bin/aseprite.exe`.

## Mac OS X details

You need the old Mac OS X 10.4 SDK universal, which can be obtained
from Xcode 3.1 Developer Tools (Xcode 3.1 Developer DVD,
`xcode31_2199_developerdvd.dmg`). You can get it from Apple developer
website (you need to be registered):

  https://developer.apple.com/downloads/

Inside the `Packages` folder, there is a MacOSX10.4.Universal.pkg,
install it (it will be installed in `/SDKs/MacOSX10.4u.sdk`), and run
cmake with the following parameters:

    -DCMAKE_OSX_ARCHITECTURES:STRING=i386
    -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=10.4
    -DCMAKE_OSX_SYSROOT:STRING=/SDKs/MacOSX10.4u.sdk

# How to use installed third party libraries?

If you don't want to use the embedded code of third party libraries
(i.e. to use your installed versions), you can disable static linking
configuring each `USE_SHARED_` option.

After running `cmake -G`, you edit `build/CMakeCache.txt` file, and
enable the `USE_SHARED_` flag (set its value to `ON`) of the library
that you want to be linked dynamically.

# How to profile Aseprite?

You must compile with `Profile` configuration. For example on Linux:

    /.../aseprite-source/build$ cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE:STRING=Profile -DCOMPILER_GCC:BOOL=ON
