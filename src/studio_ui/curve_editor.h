#pragma once
#include <map>
#include <string>

#include "rhythm/parameters/curve.h"

namespace rhythm::studio {
struct CurveEdit {
    bool changed_ = false;
    bool committed_ = false;
};
CurveEdit DrawCurveEditor(parameters::Curve& curve, const std::string& id,
                          const std::map<std::string, std::string>& text);
}  // namespace rhythm::studio
