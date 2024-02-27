<div align="center">

  # MediaEditor Community

  A lightweight, cross-platform and open-source software for non-linear editing.<br>
  <sub>Available for Linux, macOS and Windows.</sub>

  <a href="LICENSE"><img alt="License" src="docs/assets/license.svg" /></a>
  <a href="https://github.com/opencodewin/MediaEditor/releases/tag/v0.9.9"><img alt="Devlop" src="docs/assets/code-doc.svg" /></a>
  <a href="https://github.com/opencodewin/MediaEditor/pulls"><img alt="PRs Welcome" src="docs/assets/prs.svg" /></a>
  <a href="#HOW-TO-CONTRIBUTE"><img alt="Contributions Welcome" src="docs/assets/contribute.svg" /></a>
  <a href="https://github.com/opencodewin/MediaEditor/commits/master"><img alt="Commits" src="docs/assets/latest.svg" /></a>

  [**Use it now →**](docs/dev/Quick-Start.md)
  <br>

  <img src="docs/assets/multi_viewpoint.gif" style="width:70%;height:auto;" alt="MediaEditor Community" />

</div>

## NEWS
#### ***Note:*** For ease of reading, we'll abbreviate <u>***MediaEditor Community***</u> to <u>***Mec***</u>.

<div>
<a href="https://github.com/opencodewin/MediaEditor/releases/tag/v0.9.9"><img alt="v0.9.9" src="docs/assets/version_4.svg" /></a>

<b>&nbsp;&nbsp;🎉🎉🎉&nbsp; Released on February 27, 2024.</b>

  - **🔥 Supported graphic operation mode for video attribute editing.**
  >>><img src="docs/assets/graphic_op.png" style="width:auto;height:165;" />
  - **🔥 Supported creating masks to limit the pixel range of events.**
  >>><img src="docs/assets/mask.png" style="width:auto;height:160;" />
  - **🔥 Supported scaling, feathering, and key-frame for masks.**
  >>><img src="docs/assets/mask_zoom.png" style="width:auto;height:160;" />
  - **🔥 Supported multi-dimensional Joint key-frame mode for events.**
  >>><img src="docs/assets/keyframe.png" style="width:auto;height:132;" />
  - **🔥 Supported loading media from embedded file-browser to timeline.**
  - **🔥 Supported media preview from media bank and embedded file-browsers.**
  >>><img src="docs/assets/media_bank_preview.png" style="width:auto;height:275;" />
  - **🔥 Supported simple media management, including searching, sorting, and classification.**
  - 💡 Optimized software performance.
  - 💡 Optimized media output by implementing hardware support.
  - 💡 Added toolbar and slidebar for timeline.
  >>><img src="docs/assets/timeline_new.png" style="width:auto;height:120;" />
  - 💡 Added graphic event list for filter Editing.
  >>><img src="docs/assets/event_list.png" style="width:auto;height:165;" />
  - 💡 Fixed a lot of bugs.
---
</div><br>


<div>
<a href="https://github.com/opencodewin/MediaEditor/releases/tag/v0.9.8"><img alt="v0.9.8" src="docs/assets/version_3.svg" /></a>
<details>
<summary><b>&nbsp;&nbsp;👏👏👏&nbsp; Released on August 28, 2023.</b></summary>

  - **🔥 Supported BluePrint-based event mode.**
  - **🔥 Supported short-term filters or effects.**
  - 💡 Optimized software performance.
  - 💡 Modified some UI behaviors.
  - 💡 Added some fliters.
  - 💡 Fixed some bugs.
</details>
</div><br>

<div>
<a href="https://github.com/opencodewin/MediaEditor/releases/tag/v0.9.7"><img alt="v0.9.7" src="docs/assets/version_2.svg" /></a>
<details>
<summary><b>&nbsp;&nbsp;👏👏👏&nbsp; Released on May 19, 2023.</b></summary>

  - **🔥 Supported plug-in mode for Enhanced clip editing.**
  - 💡 Added some effects and transitions.
  - 💡 Fixed some known bugs.
</details>
</div><br>

<div>
<a href="https://github.com/opencodewin/MediaEditor/releases/tag/v0.9.6"><img alt="v0.9.6" src="docs/assets/version_1.svg" /></a>
<details>
<summary><b>&nbsp;&nbsp;👏👏👏&nbsp; Released on April 20, 2023.</b></summary>

  - **🔥 Building the basic framework for non-linear editing.**
  - 💡 Added some UI components.

</details>
</div>

## FUTURE WORK
<table width="700">
<tr>
<td width="100">AI Repair</td>
<td width="160">&#9997; Denoising<br/>&#9997; Deband<br/>&#9997; Inpaint<br/>&#9997; Defocusing<br/>&#9997; Deflicker</td>
<td rowspan=3>
<img src="docs/assets/sd.jpg" />
</td>
</tr>
<tr>
<td>AI Enhance</td>
<td>&#9997; Interpolation<br/>&#9997; Super Resolution<br/>&#9997; Face Enhancement</td>
</tr>
<tr>
<td>AIGC</td>
<td>&#9997; Text2Img<br/>&#9997; Img2Img<br/>&#9997; Text2Audio<br/>&#9997; Speech2Text</td>
</tr>
</table>

## FEATURES
* Support complete timeline editing functions, including move, crop, cut, thumbnail preview, scale and delete.
* Support more flexible and easily blueprint system. Blueprint is represented in the form of nodes, which can handle complex functions through nodes and flows.
>>><img src="docs/assets/blueprint.gif" style="width:auto;height:140;" />
* Support about 45+ built-in media filters and 70+ built-in media transitions.
>>><img src="docs/assets/video-transition_new.png" style="width:auto;height:315;"/>
* Support about 10 video and audio analysis tools.
>>><table><tr><td><img src="docs/assets/cie.gif" style="width:auto;height:170;" /></td><td><img src="docs/assets/waveform.gif" style="width:auto;height:170;" /></td><td><img src="docs/assets/spec.gif" style="width:auto;height:170;" /></td></tr></table>
* Support multiple audio and video codecs, including ProRes, H.264, H.265, VP9, etc.
* Support import and edit videos from standard definition to 4K resolution.
* Support magnetic snapping, which can smoothly adjust adjacent clips when arranging them to eliminate gaps, conflicts, and synchronization issues.
* Support frame-by-frame preview mode, including forward playback and reverse playback.
* Support multi-monitor mode, making it easy to preview and process media through external monitors.
* Support video attribute-editing, including cropping, moving, scaling and rotating video frames.
>>><img src="docs/assets/video-filter_new.png" style="width:auto;height:315;" />
* Support audio mixing, including mixer, pan, equalizer, gate, limiter and compressor.
>>><img src="docs/assets/audio-mixing_new.png" style="width:auto;height:345;"/>
* Support curve and keypoint, applied in video filter, video transition, audio filter, audio transition, video attribute and text subtitle.
* Support subtitle editing, including font, position, scale, rotate, oytline width, font attribute, alignment, etc.
* Support customized blueprint nodes, allowing for free expansion of filter and transition effects.
* Support multiple professional export formats, including QuickTime, MKV, MP4, Matroska, etc.

## GETTING STARTED
We provide the following release packages for Windows, Linux and macOS.

| System | Stable / Nightly |
| ------ | ---------------- |
| Windows 10 / 11 | <a href="https://github.com/opencodewin/MediaEditor/releases/download/v0.9.9/mec_SDL2_OpenGL3_win-x86_64-0.9.9.exe"><img src="docs/assets/download.svg"></a> |
| Ubuntu 20.04 | <a href="https://github.com/opencodewin/MediaEditor/releases/download/v0.9.9/MEC_SDL2_OpenGL3-ubuntu2004-x86_64-0.9.9.AppImage"><img src="docs/assets/download.svg"></a> |
| Ubuntu 22.04 | <a href="https://github.com/opencodewin/MediaEditor/releases/download/v0.9.9/MEC_SDL2_OpenGL3-ubuntu2204-x86_64-0.9.9.AppImage"><img src="docs/assets/download.svg"></a> |
| MacOS x86 | <a href="https://github.com/opencodewin/MediaEditor/releases/download/v0.9.9/MEC_SDL2_OpenGL3-MacOS-x86_64-0.9.9.dmg"><img src="docs/assets/download.svg"></a> |
| MacOS ARM | <a href="https://github.com/opencodewin/MediaEditor/releases/download/v0.9.9/MEC_SDL2_OpenGL3-MacOS-arm64-0.9.9.dmg"><img src="docs/assets/download.svg"></a> |

In addition, we also provide tutorials for compiling and installing our software from source code, Please go to [here](docs/dev/How-to-Built.md) 🐧.

Until then, there are three things to note.

- ⚠️⚠️⚠️ Vulkan sdk is necessary, please [download](https://vulkan.lunarg.com/sdk/home) and install it.
- ⚠️⚠️⚠️ For linux, please make the AppImage file executable by the following command:
  ``` sh
  chmod +x MEC_SDL2_OpenGL3-linux-x86_64-x.x.x.AppImage
  ``` 
- ⚠️⚠️⚠️ And if you're using linux distros that use fuse3, and miss libfuse.so.2 to run the AppImage file, you can install it by:
  ``` sh
  sudo apt install libfuse2
  ```

## DEPENDENCIES
Mec relies on some of our other projects. If you are interested in how it's built, you can browse through these projects, which we are constantly updating:

*  imgui (https://github.com/opencodewin/imgui.git)
*  blueprintsdk (https://github.com/opencodewin/blueprintsdk.git)
*  mediacore (https://github.com/opencodewin/MediaCore.git)

## HOW TO CONTRIBUTE
Mec is created by ours and we welcome every contribution. At present, it has achieved quite a lot functions, which also means that it is becoming increasingly large. We don't recommend that you add new functions to this current code. If you really need this functions, please contact us.

## PLUGINS
Mec supports plug-in frameworks, which can expand Filters, Transitions, Effects and AI, according to your own needs. Currently, we offer the following built-in plugins:

### FILTERS AND EFFECTS
| | | | | |
|------------------|-----------------|:--------------|:----------------|:---------------|
| ALM Enhancement  | Audio Equalizer | Audio Gain    | Bilateral Blur  | Binary         |
| Box Blur         | Brightness      | Canny Edge    | CAS Sharpen     | Chroma Key     |
| Color Balance    | Color Curve     | Color Invert  | Contrast        | Crop           |
| Deband           | Deinterlace     | Dilation      | Erosion         | Exposure       |
| Flip             | Gamma           | Gaussian Blur | Guided Filter   | HQDN3D Denoise |
| Hue              | Laplacian Edge  | Lut 3D        | SmartDenoise    | Sobel Edge     |
| USM Sharpen      | Vibrance        | WarpAffine    | WarpPerspective | White Balance  |
| Barrel           | Pincushion      | Jitter        | Kuwahara        | Lighting       |
| PixeLate         | RadicalBlur     | SmudgeBlur    | Soul            | Star           |
| Sway             | WaterRipple     | Bilateral     | Glass           | Emboss         |
| CrossHatch       | Sketch          | Audio Aecho   |||
| | | | | |

**Kuwahara**

<img src="docs/assets/mediaeditor_filter.png" style="width:90%;height:auto;" />

### TRANSITIONS
| | | | | |
|-----------------|----------------|:-----------------|:-----------------|:------------------|
| Alpha           | AudioFade      | LinearBlur       | BookFlip         | Bounce            |
| BowTie          | Burn           | BurnOut          | ButterflyWave    | CannabisLeaf      |
| CircleBlur      | CircleCrop     | ColorPhase       | ColorDistance    | CrazyParametric   |
| Crosshatch      | CrossWarp      | CrossZoom        | **Cube**         | DirectionalScaled |
| DirectionalWarp | Dissolve       | DoomScreen       | Door             | Doorway           |
| Dreamy          | DreamyZoom     | Edge             | Fade             | Flyeye            |
| GlitchDisplace  | GlitchMemories | GridFlip         | **Heart**        | Hexagonalize      |
| KaleidoScope    | Luma           | LuminanceMelt    | Morph            | Mosaic            |
| Move            | MultiplyBlend  | PageCurl         | Perlin           | Pinwheel          |
| Pixelize        | Polar          | PolkaDots        | Radial           | RandomSquares     |
| Rectangle       | Ripple         | Rolls            | RotateScale      | RotateScaleVanish |
| SimpleZoom      | SimpleZoomOut  | Slider           | SquaresWire      | Squeeze           |
| StaticWipe      | StereoViewer   | Swap             | Swirl            | WaterDrop         |
| Wind            | WindowBlinds   | WindowSlice      | Wipe             | ZoomInCircles     |
| | | | | |

**Cube**

<img src="docs/assets/fs1.jpeg" style="width:90%;height:auto;" />

**Heart**

<img src="docs/assets/fs2.jpeg" style="width:90%;height:auto;" />

## LICENSE
Mec is **[LGPLv3 licensed](LICENSE)**. You may use, distribute and copy it under the license terms.

<a href="https://github.com/opencodewin/MediaEditor/graphs/contributors"><img src="docs/assets/built-by-developers.svg" height="25" /></a>
