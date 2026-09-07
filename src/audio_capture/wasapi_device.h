#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
// Windows/COM types are private to this device adapter.
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <optional>

namespace rhythm::audio::detail {
struct PcmChunk {
    std::array<float, 8192> samples_{};
    std::uint32_t frames_ = 0;
    bool discontinuity_ = false;
};
class ComApartment final {
   public:
    ComApartment();
    ~ComApartment();
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
};
class WasapiDevice final {
   public:
    WasapiDevice();
    ~WasapiDevice();
    WasapiDevice(const WasapiDevice&) = delete;
    WasapiDevice& operator=(const WasapiDevice&) = delete;
    std::optional<PcmChunk> Read();
    std::uint32_t SampleRate() const { return sample_rate_; }

   private:
    ComApartment apartment_{};
    Microsoft::WRL::ComPtr<IAudioClient> client_{};
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_{};
    std::uint32_t sample_rate_ = 48000;
    bool started_ = false;
};
}  // namespace rhythm::audio::detail
