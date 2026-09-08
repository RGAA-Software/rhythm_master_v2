#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>

#include "rhythm/parameters/curve.h"

namespace rhythm::studio {
struct CurveEdit {
    bool changed_ = false;
    bool committed_ = false;
};
// UI-thread selection, plot drag and bulk-edit state. The caller owns the
// document transaction; Draw publishes values and a single completion signal.
class CurveEditor final {
   public:
    CurveEdit Draw(parameters::Curve& curve, const std::string& id,
                   const std::map<std::string, std::string>& text);
    void Reset();

   private:
    CurveEdit DrawPlot(parameters::Curve& curve);
    bool DrawBatch(parameters::Curve& curve, const std::map<std::string, std::string>& text);
    std::string id_{};
    std::optional<parameters::Curve> previous_{};
    std::set<std::size_t> selected_{};
    std::optional<std::size_t> dragged_{};
    int handle_ = 0;
    double time_min_ = 0;
    double time_max_ = 1;
    double value_min_ = 0;
    double value_max_ = 1;
    bool drag_changed_ = false;
    parameters::CurveTransform transform_{};
    std::string error_{};
};
}  // namespace rhythm::studio
