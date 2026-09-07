// Private candidate C API ownership adapter; not an installed project API.
#pragma once
#include <msquic.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
namespace rhythm::quic_probe {
inline void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
inline void Check(QUIC_STATUS status, const std::string& operation) {
    Require(QUIC_SUCCEEDED(status), operation + ":" + std::to_string(status));
}
class Api final {
   public:
    Api() { Check(MsQuicOpen2(&table_), "api.open"); }
    ~Api() {
        if (table_) MsQuicClose(table_);
    }
    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;
    const QUIC_API_TABLE& Table() const { return *table_; }

   private:
    // Native function table exclusively acquired/released by this RAII adapter.
    const QUIC_API_TABLE* table_ = nullptr;
};
enum class Kind { kRegistration, kConfiguration, kListener, kConnection, kStream };
class Handle final {
   public:
    Handle(std::shared_ptr<Api> api, Kind kind) : api_(std::move(api)), kind_(kind) {}
    ~Handle() { Reset(); }
    void Reset() {
        if (!handle_) return;
        const auto& api = api_->Table();
        switch (kind_) {
            case Kind::kRegistration:
                api.RegistrationClose(handle_);
                break;
            case Kind::kConfiguration:
                api.ConfigurationClose(handle_);
                break;
            case Kind::kListener:
                api.ListenerClose(handle_);
                break;
            case Kind::kConnection:
                api.ConnectionClose(handle_);
                break;
            case Kind::kStream:
                api.StreamClose(handle_);
                break;
        }
        handle_ = nullptr;
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HQUIC Get() const { return handle_; }
    HQUIC* Output() { return &handle_; }
    void Adopt(HQUIC handle) { handle_ = handle; }

   private:
    std::shared_ptr<Api> api_{};
    Kind kind_ = Kind::kRegistration;
    // C API handle ownership stays inside this adapter; close quiesces callbacks.
    HQUIC handle_ = nullptr;
};
}  // namespace rhythm::quic_probe
