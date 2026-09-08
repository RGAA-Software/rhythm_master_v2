#include "catalog_preview.h"

#include "rhythm/render/layout.h"

namespace rhythm::studio {
void CatalogPreview::Select(std::filesystem::path package) {
    if (package == selected_) return;
    Close();
    selected_ = std::move(package);
}
void CatalogPreview::Close() {
    loader_.Cancel();
    loader_.Take();
    requested_.clear();
    selected_.clear();
    session_ = {};
    texture_ = {};
    extent_ = {};
    failed_ = false;
}
void CatalogPreview::Retry() {
    session_ = {};
    texture_ = {};
    failed_ = false;
}
void CatalogPreview::Update(render::Renderer& renderer, double seconds,
                            const runtime::ExternalInputs& inputs) {
    if (auto loaded = loader_.Take()) {
        if (!selected_.empty() && requested_ == selected_) {
            if (loaded->package_)
                session_.LoadPrepared(std::move(*loaded->package_));
            else
                failed_ = true;
        }
        requested_.clear();
    }
    if (selected_.empty() || failed_) return;
    if (!session_.Ready() && !loader_.Busy() && loader_.StartFile(selected_))
        requested_ = selected_;
    if (!session_.Ready()) return;
    const auto fit = render::AspectFit(session_.Canvas(), {0, 0, 256, 144});
    extent_ = {static_cast<std::uint16_t>(fit.width_), static_cast<std::uint16_t>(fit.height_)};
    const auto output = session_.Tick(seconds, false, extent_, renderer, inputs);
    texture_ = output.final_;
    failed_ = output.budget_.has_value();
}
}  // namespace rhythm::studio
