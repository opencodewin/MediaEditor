#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "immat.h"
#include "MediaInfo.h"
#include "Logger.h"

#define ENCOPT__PROFILE             "Profile"
#define ENCOPT__PRESET              "Preset"
#define ENCOPT__GOP_SIZE            "GopSize"
#define ENCOPT__ASPECT_RATIO        "AspectRatio"
#define ENCOPT__MAX_B_FRAMES        "MaxBFrames"
#define ENCOPT__COLOR_RANGE         "ColorRange"
#define ENCOPT__COLOR_SPACE         "ColorSpace"
#define ENCOPT__COLOR_PRIMARIES     "ColorPrimaries"
#define ENCOPT__COLOR_TRC           "ColorTransferCharacteristic"
#define ENCOPT__CHROMA_LOCATION     "ChromaLocation"

struct MediaEncoder
{
    virtual bool Open(const std::string& url) = 0;
    virtual bool Close() = 0;
    virtual bool ConfigureVideoStream(const std::string& codecName,
            std::string& imageFormat, uint32_t width, uint32_t height,
            const MediaInfo::Ratio& frameRate, uint64_t bitRate,
            std::unordered_map<std::string, std::string>* extraOpts = nullptr) = 0;
    virtual bool ConfigureAudioStream(const std::string& codecName,
            std::string& sampleFormat, uint32_t channels, uint32_t sampleRate, uint64_t bitRate) = 0;
    virtual bool Start() = 0;
    virtual bool FinishEncoding() = 0;
    virtual bool EncodeVideoFrame(ImGui::ImMat vmat, bool wait = true) = 0;
    virtual bool EncodeAudioSamples(uint8_t* buf, uint32_t size, bool wait = true) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;

    virtual std::string GetError() const = 0;
};

MediaEncoder* CreateMediaEncoder();
void ReleaseMediaEncoder(MediaEncoder** mencoder);

Logger::ALogger* GetMediaEncoderLogger();