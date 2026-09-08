#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "rhythm/shader_authoring/compiler.h"

// Native bridge for Python content tooling; uses the same compiler path as Studio.
#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    using namespace rhythm;
    try {
        if (argc != 6)
            throw std::invalid_argument(
                    "shader_author_tool compiler includes varying expression-file asset-directory");
        const std::filesystem::path source(argv[4]);
        const auto size = std::filesystem::file_size(source);
        if (size > image_shader::kMaximumSourceBytes)
            throw std::length_error("shader.source_limit");
        std::string expression(std::size_t(size), '\0');
        std::ifstream file(source, std::ios::binary);
        file.read(expression.data(), std::streamsize(expression.size()));
        if (!file) throw std::runtime_error("shader.read");
        shader_authoring::Compiler compiler;
        if (!compiler.Start({{argv[1], argv[2], argv[3]}, argv[5], expression}))
            throw std::runtime_error("shader.worker_unavailable");
        for (;;) {
            if (const auto result = compiler.Take()) {
                if (!result->asset_) throw std::runtime_error(result->error_);
                std::cout << "{\"sha256\":\"" << result->asset_->id_.sha256_
                          << "\",\"bytes\":" << result->asset_->bytes_ << ",\"media_type\":\""
                          << result->asset_->media_type_ << "\"}\n";
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
