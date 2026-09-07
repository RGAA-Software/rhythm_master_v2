#include "export_process.h"

#include <SDL3/SDL.h>

#include <array>
#include <stdexcept>

namespace rhythm::exporting::detail {
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
struct ProcessDelete {
    void operator()(SDL_Process* process) const {
        if (!SDL_WaitProcess(process, false, nullptr)) {
            SDL_KillProcess(process, true);
            SDL_WaitProcess(process, true, nullptr);
        }
        SDL_DestroyProcess(process);
    }
};
}  // namespace
class ExportProcess::Impl final {
   public:
    Impl(const std::filesystem::path& executable, const std::filesystem::path& directory) {
        const auto binary_path = executable.u8string(), folder_path = directory.u8string();
        const std::string binary(binary_path.begin(), binary_path.end());
        const std::string folder(folder_path.begin(), folder_path.end());
        std::array<const char*, 4> arguments{binary.c_str(), "--export-job", folder.c_str(),
                                             nullptr};
        Properties properties;
        if (!SDL_SetPointerProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                                    arguments.data()) ||
            !SDL_SetBooleanProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,
                                    true) ||
            !SDL_SetNumberProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
                                   SDL_PROCESS_STDIO_NULL) ||
            !SDL_SetNumberProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
                                   SDL_PROCESS_STDIO_NULL) ||
            !SDL_SetNumberProperty(properties.Handle(), SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
                                   SDL_PROCESS_STDIO_NULL))
            throw std::runtime_error(SDL_GetError());
        // Background requests CREATE_NO_WINDOW on Windows. SDL intentionally
        // hides its exit code; success requires the child's explicit result file.
        process_.reset(SDL_CreateProcessWithProperties(properties.Handle()));
        if (!process_) throw std::runtime_error(SDL_GetError());
    }
    bool Done() { return SDL_WaitProcess(process_.get(), false, nullptr); }

   private:
    // SDL transfers exclusive child/process-resource ownership to this adapter.
    std::unique_ptr<SDL_Process, ProcessDelete> process_{};
};
ExportProcess::ExportProcess(const std::filesystem::path& executable,
                             const std::filesystem::path& directory)
    : impl_(std::make_unique<Impl>(executable, directory)) {}
ExportProcess::~ExportProcess() = default;
bool ExportProcess::Done() { return impl_->Done(); }
}  // namespace rhythm::exporting::detail
