#include <iostream>
#include <string>
#include "MediaParser.h"
#include "MediaReader.h"
#include "MediaEncoder.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

int main(int argc, const char* argv[])
{
    if (argc < 3)
    {
        Log(Error) << "Wrong arguments!" << endl;
        return -1;
    }

    string vidEncCodec = "h264";
    uint32_t outWidth{1920}, outHeight{1080};
    MediaInfo::Ratio outFrameRate = { 25, 1 };
    uint64_t outVidBitRate = 10*1000*1000;
    string audEncCodec = "aac";
    uint32_t outAudChannels = 2;
    uint32_t outSampleRate = 48000;
    uint64_t outAudBitRate = 128*1000;

    MediaParserHolder hParser = CreateMediaParser();
    if (!hParser->Open(argv[1]))
    {
        Log(Error) << "FAILED to open MediaParser by file '" << argv[1] << "'! Error is '" << hParser->GetError() << "'." << endl;
        return -1;
    }
    MediaInfo::InfoHolder hInfo = hParser->GetMediaInfo();

    MediaReader *vidreader{nullptr}, *audreader{nullptr};
    if (hParser->GetBestVideoStreamIndex() >= 0)
    {
        vidreader = CreateMediaReader();
        if (!vidreader->Open(hParser))
        {
            Log(Error) << "FAILED to open video MediaReader! Error is '" << vidreader->GetError() << "'." << endl;
            return -2;
        }
        if (!vidreader->ConfigVideoReader(outWidth, outHeight))
        {
            Log(Error) << "FAILED to configure video MediaReader! Error is '" << vidreader->GetError() << "'." << endl;
            return -3;
        }
        vidreader->Start();
    }
    if (hParser->GetBestAudioStreamIndex() >= 0)
    {
        audreader = CreateMediaReader();
        if (!audreader->Open(hParser))
        {
            Log(Error) << "FAILED to open audio MediaReader! Error is '" << audreader->GetError() << "'." << endl;
            return -4;
        }
        if (!audreader->ConfigAudioReader(outAudChannels, outSampleRate))
        {
            Log(Error) << "FAILED to configure audio MediaReader! Error is '" << audreader->GetError() << "'." << endl;
            return -5;
        }
        audreader->Start();
    }

    int vidStmIdx{-1}, audStmIdx{-1};
    MediaEncoder *mencoder = CreateMediaEncoder();
    if (!mencoder->Open(argv[2]))
    {
        Log(Error) << "FAILED to open MediaEncoder by '" << argv[2] << "'! Error is '" << mencoder->GetError() << "'." << endl;
        return -6;
    }
    string vidEncImgFormat;
    if (vidreader && (vidStmIdx = mencoder->ConfigureVideoStream(vidEncCodec, vidEncImgFormat, outWidth, outHeight, outFrameRate, outVidBitRate)) < 0)
    {
        Log(Error) << "FAILED to configure video encoder! Error is '" << mencoder->GetError() << "'." << endl;
        return -7;
    }
    string audEncSmpFormat;
    if (audreader && (audStmIdx = mencoder->ConfigureAudioStream(audEncCodec, audEncSmpFormat, outAudChannels, outSampleRate, outAudBitRate)) < 0)
    {
        Log(Error) << "FAILED to configure audio encoder! Error is '" << mencoder->GetError() << "'." << endl;
        return -8;
    }
    mencoder->Start();

    bool vidInputEof = vidreader ? false : true;
    bool audInputEof = audreader ? false : true;
    double audpos = 0, vidpos = 0;
    uint32_t vidFrameCount = 0;
    ImGui::ImMat vmat;
    uint32_t pcmbufSize = 8192;
    uint8_t* pcmbuf = new uint8_t[pcmbufSize];
    while (!vidInputEof || !audInputEof)
    {
        if ((!vidInputEof && vidpos <= audpos) || audInputEof)
        {
            bool eof;
            if (!vidreader->ReadVideoFrame(vidpos, vmat, eof) && !eof)
            {
                Log(Error) << "FAILED to read video frame! Error is '" << vidreader->GetError() << "'." << endl;
                break;
            }
            if (!eof)
            {
                vmat.time_stamp = vidpos;
                if (!mencoder->EncodeVideoFrame(vidStmIdx, vmat))
                {
                    Log(Error) << "FAILED to encode video frame! Error is '" << mencoder->GetError() << "'." << endl;
                    break;
                }
            }
            else
            {
                vmat.release();
                if (!mencoder->EncodeVideoFrame(vidStmIdx, vmat))
                {
                    Log(Error) << "FAILED to encode video EOF! Error is '" << mencoder->GetError() << "'." << endl;
                    break;
                }
                vidInputEof = true;
            }
        }
        else
        {
            bool eof;
            uint32_t readSize = pcmbufSize;
            if (!audreader->ReadAudioSamples(pcmbuf, readSize, audpos, eof) && !eof)
            {
                Log(Error) << "FAILED to read audio samples! Error is '" << audreader->GetError() << "'." << endl;
                break;
            }
            if (!eof)
            {
                if (!mencoder->EncodeAudioSamples(audStmIdx, pcmbuf, readSize))
                {
                    Log(Error) << "FAILED to encode audio samples! Error is '" << mencoder->GetError() << "'." << endl;
                    break;
                }
            }
            else
            {
                if (!mencoder->EncodeAudioSamples(audStmIdx, nullptr, 0))
                {
                    Log(Error) << "FAILED to encode audio EOF! Error is '" << mencoder->GetError() << "'." << endl;
                    break;
                }
                audInputEof = true;
            }
        }
    }
    delete [] pcmbuf;
    mencoder->WaitAndFinishEncoding();
    mencoder->Close();
    ReleaseMediaEncoder(&mencoder);

    if (vidreader)
        ReleaseMediaReader(&vidreader);
    if (audreader)
        ReleaseMediaReader(&audreader);

    return 0;
}