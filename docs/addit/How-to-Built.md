## How to build from source code?
  - [To ensure cross-platform, it's recommended to install Vulkan SDK](#to-ensure-cross-platform-it-s-recommended-to-install-vulkan-sdk)
  - [Windows 10/11](#windows-1011)
  - [Ubuntu 20.04/22.04](#ubuntu-20-0422-04)
  - [MacOS(Intel/Apple Silicon)](#macos-intelapple-silicon)

### To ensure cross-platform, it's recommended to install Vulkan SDK
- [x] [Vulkan](https://vulkan.lunarg.com/sdk/home) is a new-generation graphics and compute API for **high-efficiency, cross-platform** access to GPUs.

### Windows 10/11
#### 1. Install MSYS2(Mingw64)
- [x] [MSYS2](https://www.msys2.org) is a collection of **tools and libraries** providing you with an easy-to-use environment for **building, installing and running native Windows software**.
#### 2. Install related packages
    pacman -Syu && pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake git \
        mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-openmp mingw-w64-x86_64-zlib \
        mingw-w64-x86_64-glslang mingw-w64-x86_64-pkgconf mingw-w64-x86_64-spirv-tools \
        mingw-w64-x86_64-glew mingw-w64-x86_64-glfw mingw-w64-x86_64-SDL2 \
        mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-libass \
        mingw-w64-x86_64-fontconfig mingw-w64-x86_64-freetype 
#### 3. Download Source Code
    git clone https://github.com/opencodewin/MediaEditor.git
#### 4. Build MEC
    cd MediaEditor && \
    mkdir build && cd build && \
    cmake .. && make -j8
#### 5. Run MEC
    ./mec.exe

### Ubuntu 20.04/22.04
#### 1. Install related packages
    sudo apt update && sudo apt install build-essential cmake git \
             ffmpeg libomp-dev zlib1g-dev glslang-dev pkg-config spirv-tools \
             libglew-dev libglfw3-dev libsdl2-dev libsdl2-image-dev libass-dev \
             libfontconfig-dev libfreetype-dev
#### 2. Download Source Code
    git clone https://github.com/opencodewin/MediaEditor.git
#### 3. Build MEC
    cd MediaEditor && \
    mkdir build && cd build && \
    cmake .. && make -j8
#### 4. Run MEC
    ./mec

### MacOS(Intel/Apple Silicon)
#### 1. Install Xcode Command Line Tools
    xcode-select --install
#### 2. Install Homebrew
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
#### 3. Install other related packages
    brew install cmake git ffmpeg libomp zlib glslang pkg-config \
                 spirv-tools glew glfw sdl2 sdl2_image libass fontconfig freetype
#### 4. Download Source Code
    git clone https://github.com/opencodewin/MediaEditor.git
#### 5. Build MEC
    cd MediaEditor && \
    mkdir build && cd build && \
    cmake .. && make -j8
#### 6. Run MEC
    ./mec.app/Contents/MacOS/mec