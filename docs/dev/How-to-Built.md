# Build for MediaEditor Community
## Precondition
### Install Git and Vulkan
- [x] [Git](https://git-scm.com/downloads/)
- [x] [Vulkan](https://vulkan.lunarg.com/sdk/home) is a new-generation graphics and compute API for **high-efficiency, cross-platform** access to GPUs.

### Git clone MediaEditor repo with submodule
``` sh
git clone --recurse-submodules https://github.com/opencodewin/MediaEditor.git
``` 

---
- [Precondition](#precondition)
  - [Install Git and Vulkan](#install-git-and-vulkan)
  - [Git clone MediaEditor repo with submodule](#git-clone-mediaeditor-repo-with-submodule)
- [Build for Windows x64 using Mingw64](#build-for-windows-x64-using-mingw64)
  - [Step 1 ~ Install MSYS2 and NSIS](#step-1-❤-install-msys2-and-nsis)
  - [Step 2 ~ Install Dependencies (MSYS2 commandline)](#step-2-❤-install-dependencies-msys2-commandline)
  - [Step 3 ~ Build source code (Mingw64 commandline)](#step-3-❤-build-source-code-mingw64-commandline)
  - [Step 4 ~ Generate Installation package (Optional)](#step-4-❤-generate-installation-package-optional)
- [Build for Ubuntu](#build-for-ubuntu)
  - [Step 1 ~ Install Dependencies](#step-1-❤-install-dependencies)
  - [Step 2 ~ Build source code](#step-2-❤-build-source-code)
  - [Step 3 ~ Generate Installation package (Optional)](#step-3-❤-generate-installation-package-optional)
- [Build for MacOS x86 and MacOS M1](#build-for-macos-x86-and-macos-m1)
  - [Step 1 ~ Install CommandLineTools and Homebrew](#step-1-❤-install-commandlinetools-and-homebrew)
  - [Step 2 ~ Install Dependencies](#step-2-❤-install-dependencies)
  - [Step 3 ~ Build source code](#step-3-❤-build-source-code)
  - [Step 4 ~ Generate Installation package (Optional)](#step-4-❤-generate-installation-package-optional)

---
## Build for Windows x64 using Mingw64
### Step 1 &ensp;❤&ensp; Install MSYS2 and NSIS
- [x] [MSYS2](https://www.msys2.org) is a collection of **tools and libraries** providing you with an easy-to-use environment for **building, installing and running native Windows software**.
- [x] [NSIS](https://nsis.sourceforge.io/Download) is a professional open-source tool for **creating Windows installation programs**.

### Step 2 &ensp;❤&ensp; Install Dependencies (MSYS2 commandline)
``` sh
pacman -Syu && pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
                         mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-openmp mingw-w64-x86_64-zlib \
                         mingw-w64-x86_64-glslang mingw-w64-x86_64-pkgconf mingw-w64-x86_64-spirv-tools \
                         mingw-w64-x86_64-glew mingw-w64-x86_64-glfw mingw-w64-x86_64-SDL2 \
                         mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-libass \
                         mingw-w64-x86_64-fontconfig mingw-w64-x86_64-freetype 
```

### Step 3 &ensp;❤&ensp; Build source code (Mingw64 commandline)
``` sh
cd MediaEditor && mkdir build && cd build && \
                  cmake .. && cmake --build . --config Release --target all -j
```

### Step 4 &ensp;❤&ensp; Generate Installation package (Optional)
#### ***Note: If you want to generate installation package, you need to place source code in the usr-path under the MSYS path, and then repeat the preceding steps.***
``` sh
cmake --build . --config Release --target install -j && cpack
```

---
## Build for Ubuntu
### Step 1 &ensp;❤&ensp; Install Dependencies
``` sh
sudo apt update && sudo apt install build-essential cmake \
                            libavformat-dev libavcodec-dev libavutil-dev libavdevice-dev libswscale-dev libswresample-dev \
                            libgmp-dev libomp-dev zlib1g-dev glslang-dev pkg-config spirv-tools \
                            libglew-dev libglfw3-dev libsdl2-dev libsdl2-image-dev libass-dev \
                            libfontconfig-dev libfreetype-dev
```

### Step 2 &ensp;❤&ensp; Build source code
``` sh
cd MediaEditor && mkdir build && cd build && \
                  cmake .. && cmake --build . --config Release --target all -j
```

### Step 3 &ensp;❤&ensp; Generate Installation package (Optional)
``` sh
cmake --build . --config Release --target install -j
```

---
## Build for MacOS x86 and MacOS M1
### Step 1 &ensp;❤&ensp; Install CommandLineTools and Homebrew
``` sh
xcode-select --install && 
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
### Step 2 &ensp;❤&ensp; Install Dependencies
``` sh
brew install cmake ffmpeg libomp zlib glslang pkg-config \
             spirv-tools glew glfw sdl2 sdl2_image libass fontconfig freetype
```

### Step 3 &ensp;❤&ensp; Build source code
``` sh
cd MediaEditor && mkdir build && cd build && \
                  cmake .. && cmake --build . --config Release --target all -j
```

### Step 4 &ensp;❤&ensp; Generate Installation package (Optional)
#### ***Note: <u>MacOS M1</u> has the strict signature mechanism, so we recommend using our released version. If you want to create your own installation package, please refer to the [<u>instructions</u>](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution) provided by Apple.***
``` sh
cmake --build . --config Release --target install -j && cpack
```
