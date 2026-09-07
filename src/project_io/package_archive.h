#pragma once

#include <map>
#include <string>
#include <string_view>

namespace rhythm::project::detail {
using PackageEntries = std::map<std::string, std::string>;
std::string WriteArchive(const PackageEntries& entries);
PackageEntries ReadArchive(std::string_view bytes);
}  // namespace rhythm::project::detail
