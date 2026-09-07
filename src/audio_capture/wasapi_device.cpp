#include "wasapi_device.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace rhythm::audio::detail {
namespace {
void Check(HRESULT result) {
    if (FAILED(result)) throw std::runtime_error("audio.device_unavailable");
}
// A borrowed WASAPI packet exists only in Read's synchronous copy boundary.
// ReleaseBuffer is paired here, including exceptions and silent/discontinuous packets.
class PacketLease final {
   public:
    explicit PacketLease(Microsoft::WRL::ComPtr<IAudioCaptureClient> capture)
        : capture_(std::move(capture)) {
        Check(capture_->GetBuffer(&bytes_, &frames_, &flags_, nullptr, nullptr));
        acquired_ = true;
    }
    ~PacketLease() {
        if (acquired_) capture_->ReleaseBuffer(frames_);
    }
    PacketLease(const PacketLease&) = delete;
    PacketLease& operator=(const PacketLease&) = delete;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_{};
    // Device-owned bytes, valid until this local lease's destructor only.
    BYTE* bytes_ = nullptr;
    UINT32 frames_ = 0;
    DWORD flags_ = 0;
    bool acquired_ = false;
};
}  // namespace
ComApartment::ComApartment() { Check(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
ComApartment::~ComApartment() { CoUninitialize(); }
WasapiDevice::WasapiDevice() {
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           IID_PPV_ARGS(enumerator.GetAddressOf())));
    Microsoft::WRL::ComPtr<IMMDevice> device;
    Check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.GetAddressOf()));
    Check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(client_.GetAddressOf())));
    // Ask the shared audio engine for canonical stereo float PCM. Windows owns
    // device-format conversion; this adapter does not decode media files.
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = 2;
    format.nSamplesPerSec = sample_rate_;
    format.wBitsPerSample = 32;
    format.nBlockAlign = 8;
    format.nAvgBytesPerSec = sample_rate_ * 8;
    const DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    Check(client_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 1'000'000, 0, &format, nullptr));
    Check(client_->GetService(IID_PPV_ARGS(capture_.GetAddressOf())));
    Check(client_->Start());
    started_ = true;
}
WasapiDevice::~WasapiDevice() {
    if (started_) client_->Stop();
}
std::optional<PcmChunk> WasapiDevice::Read() {
    UINT32 available = 0;
    Check(capture_->GetNextPacketSize(&available));
    if (!available) return {};
    PcmChunk chunk;
    {
        const PacketLease packet(capture_);
        chunk.discontinuity_ = (packet.flags_ & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0;
        if (packet.frames_ > 4096) {
            chunk.discontinuity_ = true;
            return chunk;
        }
        chunk.frames_ = packet.frames_;
        if (!(packet.flags_ & AUDCLNT_BUFFERFLAGS_SILENT)) {
            if (!packet.bytes_) throw std::runtime_error("audio.invalid_packet");
            std::memcpy(chunk.samples_.data(), packet.bytes_,
                        static_cast<std::size_t>(chunk.frames_) * 8);
            for (std::size_t index = 0; index < chunk.frames_ * 2; ++index)
                chunk.samples_[index] = std::isfinite(chunk.samples_[index])
                                                ? std::clamp(chunk.samples_[index], -1.0f, 1.0f)
                                                : 0;
        }
    }
    return chunk;
}
}  // namespace rhythm::audio::detail
