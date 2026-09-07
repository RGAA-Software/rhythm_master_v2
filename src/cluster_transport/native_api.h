// Native SDK types are restricted to this private adapter implementation.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <msquic.h>

#include <memory>
#include <stdexcept>
#include <utility>

namespace rhythm::transport::detail {
inline void Check(QUIC_STATUS status) {
    if (QUIC_FAILED(status)) throw std::runtime_error("transport.native_operation");
}
class Api final {
   public:
    Api() { Check(MsQuicOpen2(&table_)); }
    ~Api() {
        if (table_) MsQuicClose(table_);
    }
    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;
    const QUIC_API_TABLE& Table() const { return *table_; }

   private:
    // Exclusive SDK table allocation, acquired/freed by this synchronous RAII boundary.
    const QUIC_API_TABLE* table_ = nullptr;
};
enum class HandleKind { kRegistration, kConfiguration, kListener, kConnection, kStream };
class NativeHandle final {
   public:
    NativeHandle() = default;
    NativeHandle(std::shared_ptr<Api> api, HandleKind kind) : api_(std::move(api)), kind_(kind) {}
    ~NativeHandle() { Reset(); }
    NativeHandle(NativeHandle&& other) noexcept
        : api_(std::move(other.api_)),
          kind_(other.kind_),
          handle_(std::exchange(other.handle_, nullptr)) {}
    NativeHandle& operator=(NativeHandle&& other) noexcept {
        if (this != &other) {
            Reset();
            api_ = std::move(other.api_);
            kind_ = other.kind_;
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    NativeHandle(const NativeHandle&) = delete;
    NativeHandle& operator=(const NativeHandle&) = delete;
    void Reset() {
        if (!handle_) return;
        switch (kind_) {
            case HandleKind::kRegistration:
                api_->Table().RegistrationClose(handle_);
                break;
            case HandleKind::kConfiguration:
                api_->Table().ConfigurationClose(handle_);
                break;
            case HandleKind::kListener:
                api_->Table().ListenerClose(handle_);
                break;
            case HandleKind::kConnection:
                api_->Table().ConnectionClose(handle_);
                break;
            case HandleKind::kStream:
                api_->Table().StreamClose(handle_);
                break;
        }
        handle_ = nullptr;
    }
    HQUIC Get() const { return handle_; }
    HQUIC* Output() { return &handle_; }
    void Adopt(HQUIC handle) { handle_ = handle; }

   private:
    std::shared_ptr<Api> api_{};
    HandleKind kind_ = HandleKind::kConnection;
    // C API handle ownership never escapes the adapter; close quiesces its callbacks.
    HQUIC handle_ = nullptr;
};
}  // namespace rhythm::transport::detail
