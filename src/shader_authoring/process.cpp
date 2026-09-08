#include "process.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

namespace rhythm::shader_authoring::detail {
namespace {
class Properties final {
   public:
    Properties() {
        if (!handle_) throw std::runtime_error(SDL_GetError());
    }
    ~Properties() { SDL_DestroyProperties(handle_); }
    Properties(const Properties&) = delete;
    Properties& operator=(const Properties&) = delete;
    SDL_PropertiesID Handle() const { return handle_; }

   private:
    SDL_PropertiesID handle_ = SDL_CreateProperties();
};
struct CloseStream {
    void operator()(SDL_IOStream* stream) const { SDL_CloseIO(stream); }
};
struct CloseProcess {
    void operator()(SDL_Process* process) const {
        if (!SDL_WaitProcess(process, false, nullptr)) {
            SDL_KillProcess(process, true);
            SDL_WaitProcess(process, true, nullptr);
        }
        SDL_DestroyProcess(process);
    }
};
}  // namespace
void RunCompiler(const std::vector<std::string>& arguments, const std::filesystem::path& log,
                 std::stop_token stop) {
    const auto path = log.u8string();
    const std::string encoded(path.begin(), path.end());
    // Exclusive resources returned by the native API never leave this adapter.
    std::unique_ptr<SDL_IOStream, CloseStream> stream(SDL_IOFromFile(encoded.c_str(), "wb"));
    if (!stream) throw std::runtime_error(SDL_GetError());
    std::vector<const char*> argv;
    for (const auto& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    Properties properties;
    if (!SDL_SetPointerProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                                argv.data()) ||
        !SDL_SetBooleanProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,
                                true) ||
        !SDL_SetNumberProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
                               SDL_PROCESS_STDIO_NULL) ||
        !SDL_SetNumberProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
                               SDL_PROCESS_STDIO_REDIRECT) ||
        !SDL_SetPointerProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_STDOUT_POINTER,
                                stream.get()) ||
        !SDL_SetBooleanProperty(properties.Handle(),
                                SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true))
        throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Process, CloseProcess> process(
            SDL_CreateProcessWithProperties(properties.Handle()));
    if (!process) throw std::runtime_error(SDL_GetError());
    stream.reset();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!SDL_WaitProcess(process.get(), false, nullptr)) {
        if (stop.stop_requested()) throw std::runtime_error("shader.cancelled");
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("shader.timeout");
        if (std::filesystem::file_size(log) > 256 * 1024)
            throw std::runtime_error("shader.diagnostic_limit");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (stop.stop_requested()) throw std::runtime_error("shader.cancelled");
    // Background mode suppresses native exit status. The caller requires a fresh
    // complete artifact in its unique workspace and validates it before import.
}
}  // namespace rhythm::shader_authoring::detail
