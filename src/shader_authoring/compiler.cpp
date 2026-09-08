#include "rhythm/shader_authoring/compiler.h"

#include <picosha2.h>

#include <atomic>
#include <chrono>
#include <fstream>

#include "process.h"
#include "rhythm/image_shader/resources.h"

namespace rhythm::shader_authoring {
namespace {
std::string Utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
std::string Read(const std::filesystem::path& path, std::size_t maximum) {
    const auto size = std::filesystem::file_size(path);
    if (size > maximum) throw std::length_error("shader.file_limit");
    std::string bytes(std::size_t(size), '\0');
    std::ifstream stream(path, std::ios::binary);
    stream.read(bytes.data(), std::streamsize(bytes.size()));
    if (!stream) throw std::runtime_error("shader.read");
    return bytes;
}
void Write(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), std::streamsize(bytes.size()));
    stream.close();
    if (!stream) throw std::runtime_error("shader.write");
}
class Workspace final {
   public:
    explicit Workspace(const std::filesystem::path& assets) {
        static std::atomic<std::uint64_t> sequence{0};
        const auto root = std::filesystem::absolute(assets).parent_path() / ".shader-jobs";
        std::filesystem::create_directories(root);
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto name =
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                    "-" + std::to_string(sequence.fetch_add(1));
            const auto candidate = root / name;
            if (std::filesystem::create_directory(candidate)) {
                path_ = candidate;
                return;
            }
        }
        throw std::runtime_error("shader.workspace");
    }
    ~Workspace() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;
    const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_{};
};
Result Compile(const Request& request, std::stop_token stop) {
    Result result;
    try {
        if (const auto diagnostic = image_shader::ValidateExpression(request.expression_)) {
            result.diagnostic_ = diagnostic;
            result.error_ = diagnostic->code_;
            return result;
        }
        if (stop.stop_requested()) throw std::runtime_error("shader.cancelled");
        Workspace workspace(request.assets_);
        const auto source = workspace.Path() / "expression.sc";
        Write(source, image_shader::FragmentSource(request.expression_));
        image_shader::Program program;
        program.expression_ = request.expression_;
        const auto compiler = Read(request.tools_.compiler_, 64 * 1024 * 1024);
        program.compiler_sha256_ = picosha2::hash256_hex_string(compiler.begin(), compiler.end());
        for (std::size_t index = 0; index < 2; ++index) {
            const auto artifact = workspace.Path() / (index ? "android.bin" : "windows.bin");
            const auto log = workspace.Path() / "diagnostics.txt";
            detail::RunCompiler(
                    {Utf8(request.tools_.compiler_), "-f", Utf8(source), "-o", Utf8(artifact),
                     "--type", "fragment", "--platform", index ? "android" : "windows", "-p",
                     index ? "300_es" : "s_5_0", "-i", Utf8(request.tools_.includes_),
                     "--varyingdef", Utf8(request.tools_.varying_)},
                    log, stop);
            const auto diagnostics = Read(log, 256 * 1024);
            try {
                const auto bytes = Read(artifact, image_shader::kMaximumArtifactBytes);
                program.artifacts_[index] = {bytes.begin(), bytes.end()};
                image_shader::ValidateArtifact(program.artifacts_[index],
                                               image_shader::Target(index));
            } catch (const std::exception&) {
                throw std::runtime_error(std::string(index ? "Android: " : "Windows: ") +
                                         (diagnostics.empty() ? "shader.compile_failed"
                                                              : diagnostics.substr(0, 16384)));
            }
        }
        const auto bytes = image_shader::Encode(program);
        const auto bundle = workspace.Path() / "program.rmshader";
        Write(bundle, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        result.asset_ = assets::Store(request.assets_)
                                .Import(bundle, std::string(image_shader::kMediaType),
                                        image_shader::kMaximumProgramBytes, stop);
    } catch (const std::exception& error) {
        result.error_ = error.what();
    }
    return result;
}
}  // namespace
Compiler::~Compiler() {
    Cancel();
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool Compiler::Start(Request request) {
    if (Busy()) return false;
    if (request.expression_.size() > image_shader::kMaximumSourceBytes)
        throw std::length_error("shader.source_limit");
    cancellation_ = {};
    auto task = std::make_shared<std::packaged_task<Result()>>(
            [request = std::move(request), stop = cancellation_.get_token()] {
                return Compile(request, stop);
            });
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    return true;
}
void Compiler::Cancel() { cancellation_.request_stop(); }
std::optional<Result> Compiler::Take() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return {};
    return pending_.get();
}
}  // namespace rhythm::shader_authoring
