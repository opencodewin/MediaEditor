# Build MediaEditor Community

## Step 1: Install additional components 🐙
- [x] [Git](https://git-scm.com/downloads/)
- [x] [Vulkan](https://vulkan.lunarg.com/sdk/home) is a new-generation graphics and compute API for **high-efficiency, cross-platform** access to GPUs.

## Step 2: Download source code 🐙
    git clone --recurse-submodules https://github.com/opencodewin/MediaEditor.git

## Step 3: Build source code 🐙
### Currently supported platforms are:
-   [Microsoft Windows 10 and above](#⭐️⭐️⭐️-microsoft-windows)
-   [Ubuntu 20.04 LTS and above](#⭐️⭐️⭐️-ubuntu)
-   [MacOS, includes apple silicon](#⭐️⭐️⭐️-macos)

### ⭐️⭐️⭐️ Microsoft Windows
#### 1. Install MSYS2(Mingw64)
- [x] [MSYS2](https://www.msys2.org) is a collection of **tools and libraries** providing you with an easy-to-use environment for **building, installing and running native Windows software**.

#### 2. Because of the specificity of MSYS2, that the source code (Step 2) is placed in the MSYS2 path, such as:
    C:\msys64\home\fans\MediaEditor

#### 3. Install related packages. Open the MSYS2 commandline and enter
    pacman -Syu && pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
        mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-openmp mingw-w64-x86_64-zlib \
        mingw-w64-x86_64-glslang mingw-w64-x86_64-pkgconf mingw-w64-x86_64-spirv-tools \
        mingw-w64-x86_64-glew mingw-w64-x86_64-glfw mingw-w64-x86_64-SDL2 \
        mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-libass \
        mingw-w64-x86_64-fontconfig mingw-w64-x86_64-freetype 

#### 4. Build MEC. Open the **Mingw64** commandline and enter
    cd MediaEditor && mkdir build && cd build && \
        cmake .. && cmake --build . --config Release --target all -j

### ⭐️⭐️⭐️ Ubuntu
#### 1. Install related packages. Open the terminal and enter
    sudo apt update && sudo apt install build-essential cmake \
             ffmpeg libomp-dev zlib1g-dev glslang-dev pkg-config spirv-tools \
             libglew-dev libglfw3-dev libsdl2-dev libsdl2-image-dev libass-dev \
             libfontconfig-dev libfreetype-dev
#### 2. Build MEC
    cd MediaEditor && mkdir build && cd build && \
        cmake .. && cmake --build . --config Release --target all -j

### ⭐️⭐️⭐️ MacOS
#### 1. Install Xcode Command Line Tools
    xcode-select --install
#### 2. Install Homebrew
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
#### 3. Install related packages. Open the terminal and enter
    brew install cmake ffmpeg libomp zlib glslang pkg-config \
                 spirv-tools glew glfw sdl2 sdl2_image libass fontconfig freetype
#### 4. Build MEC
    cd MediaEditor && mkdir build && cd build && \
        cmake .. && cmake --build . --config Release --target all -j

## Step 4: Generate installation package 🐙
### 🌼🌼🌼 Generate the installation package on **Microsoft Windows** using the cpack tool
#### >>> First, you need to install NSIS v3.0.8.
- [x] [NSIS](https://nsis.sourceforge.io/Download) is a professional open-source tool for **creating Windows installation programs**.
#### >>> Then, go back to the **Mingw64** commandline. Find "build" path and enter
    cmake --build . --config Release --target install -j && cpack

### 🌼🌼🌼 Generate the installation package on **MacOS** using the cpack tool
#### >>> Go back to the terminal. Find the "build" path and enter
    cmake --build . --config Release --target install -j && cpack

### 🌼🌼🌼 Generate the AppImage on **Ubuntu**
#### >>> Go back to the terminal. Find the "build" path and enter
    cmake --build . --config Release --target install -j