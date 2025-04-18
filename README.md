### **This mod is built using Epinter's**
# ***Template Project for SKSE - CommonLibSSE-NG***

## ***Runtime requirements***

- [Skyrim Script Extender (SKSE)](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## ***Build requirements***

- [CMake](https://cmake.org/)
- [vcpkg](https://vcpkg.io/en/)
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/community/)
- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)

#### ***CommonLibSSE-NG***

To use CommonLibSSE-NG as a git-submodule instead of overlay-ports, clone it to extern/CommonLibSSE-NG and edit vcpkg.json removing "commonlibsse-ng" and adding its dependencies.

## ***Building***

In `Developer Command Prompt for VS 2022` or `Developer PowerShell for VS 2022`, run:

~~~
git clone https://github.com/epinter/skse-clibng-template.git
cd skse-clibng-template
~~~

then

~~~
.\cmake\build.ps1
~~~

or

~~~
.\cmake\build.ps1 -buildPreset ALL-relwithdebinfo
~~~

or

~~~
.\cmake\build.ps1 -buildPreset ALL-debug
~~~

or

~~~
cmake -B build -S . --preset ALL --fresh
cmake --build build --preset ALL-release
~~~

Then get the .dll in build/Release, or the .zip (ready to install using mod manager) in build.
