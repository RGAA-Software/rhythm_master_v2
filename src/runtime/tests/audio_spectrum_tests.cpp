#include <iostream>
#include <stdexcept>

#include "audio_spectrum.h"
#include "rhythm/graph/registry.h"

int main() {
    using namespace rhythm;
    try {
        const auto require = [](bool value) {
            if (!value) throw std::runtime_error("spectrum.geometry");
        };
        graph::Registry registry;
        auto node = registry.MakeNode(1, "texture.spectrum");
        std::array<float, 63> bands;
        bands.fill(0.25f);
        for (const auto layout : {0.0, 1.0}) {
            for (const auto count : {8.0, 63.0, 256.0}) {
                node.properties_["spectrum_layout"] = layout;
                node.properties_["bar_count"] = count;
                render::DrawList list;
                list.width_ = 1280;
                list.height_ = 720;
                runtime::detail::DrawSpectrum(node, bands, {}, list);
                require(list.vertices_.size() == static_cast<std::size_t>(count * 4));
                require(list.indices_.size() == static_cast<std::size_t>(count * 6) &&
                        list.commands_.size() == 1);
                for (const auto& vertex : list.vertices_)
                    require(std::isfinite(vertex.x_) && std::isfinite(vertex.y_) &&
                            vertex.x_ >= 0 && vertex.x_ <= 1280 && vertex.y_ >= 0 &&
                            vertex.y_ <= 720);
                if (layout == 0) require(std::abs(list.vertices_[0].y_ - 396) < 0.001);
                for (const auto index : list.indices_) require(index < list.vertices_.size());
            }
        }
        render::DrawList empty;
        bands.fill(0);
        runtime::detail::DrawSpectrum(node, bands, {}, empty);
        require(empty.vertices_.empty() && empty.commands_.empty());
        std::cout << "Spectrum geometry passed: linear/radial bounds, canonical resampling, "
                     "batched draw and silence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
