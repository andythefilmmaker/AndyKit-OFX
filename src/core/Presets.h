// SPDX-License-Identifier: MIT
#pragma once
#include "Params.h"
#include <string>
#include <utility>
#include <vector>

namespace ak {

struct Preset {
  std::string name;
  std::vector<std::pair<std::string, std::string>> values; // param id -> value (number, bool or choice label)
};

// Built-in demo looks.
const std::vector<Preset>& builtinPresets();
Params presetParams(size_t index); // defaults + the preset's values

// ---- user presets: plain-text files (.andykit) that can live in any folder ----
inline constexpr const char* kPresetExtension = ".andykit";

struct UserPreset {
  std::string name;
  std::string path;
};

std::string valueText(const ParamSpec& s, double v);
double paramValue(const Params& p, const std::string& id);
std::string presetToText(const Params& p, const std::string& name);
bool presetFromText(const std::string& text, Params& out, std::string* name, std::string* error);
bool savePresetFile(const std::string& path, const Params& p, const std::string& name, std::string* error);
bool loadPresetFile(const std::string& path, Params& out, std::string* name, std::string* error);

// <Documents>/AndyKit Presets — presets saved here show up in the plugin's preset menu.
std::string defaultPresetFolder();
std::string presetFileName(const std::string& folder, const std::string& name);
std::vector<UserPreset> scanPresetFolder(const std::string& dir);

} // namespace ak
