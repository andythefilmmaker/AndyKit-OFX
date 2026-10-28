// SPDX-License-Identifier: MIT
#pragma once
#include "Params.h"
#include <string>
#include <utility>
#include <vector>

namespace lw {

struct Preset {
  std::string name;
  std::vector<std::pair<std::string, std::string>> values; // param id -> value (number, bool or choice label)
};

const std::vector<Preset>& builtinPresets();
Params presetParams(size_t index); // defaults + the preset's values

} // namespace lw
