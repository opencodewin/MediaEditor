MediaCore README
--------------------

MediaCore is a LGPL multimedia framework designed for media(video/audio/text) editing. It can read and decode video/audio streams to generate raw image/pcm data represented as ImGui::ImMat instances, which can be further processed as matrix data easily. MediaCore also contains classes that implement audio/video clip, track, overlap and multi-track reader concepts, which can be used in a non-linear editing software.

MediaCore depends on FFmpeg and [opencodewin/imgui](https://github.com/opencodewin/imgui).

MediaCore is required by [opencodewin/MediaEditor](https://github.com/opencodewin/MediaEditor).