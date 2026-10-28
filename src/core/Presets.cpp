// Built-in demo looks (examples to start from). Values are applied over the defaults;
// choices are referenced by label.
// SPDX-License-Identifier: MIT
#include "Presets.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ak {

const std::vector<Preset>& builtinPresets() {
  static const std::vector<Preset> presets = {
    {"Default", {}},
    {"Demo - Lines", {{"contourField", "false"}, {"contourStyle", "topographic"}, {"contourLevels", "14"}, {"contourDetail", "55"}, {"contourOpacity", "80"}, {"contourDrift", "12"}, {"objectFocus", "emphasize"}, {"objectStrength", "72"}, {"objectConfidence", "45"}, {"artworkFrame", "true"}, {"density", "54"}, {"threshold", "24"}, {"pointRadius", "1.45"}, {"shapeMin", "6"}, {"shapeMax", "34"}, {"featureSizing", "76"}, {"sizeVariation", "38"}, {"sizeHierarchy", "74"}, {"shapeStroke", "0.8"}, {"connection", "72"}, {"edgeConfidence", "28"}, {"lineWeight", "0.75"}, {"scanlines", "18"}, {"grain", "10"}, {"baseOpacity", "78"}, {"overlayOpacity", "74"}, {"motionSpeed", "54"}, {"gridModules", "12"}, {"gridOpacity", "12"}, {"ditherCellSize", "6"}, {"ditherThreshold", "50"}, {"ditherContrast", "68"}, {"ditherOpacity", "100"}, {"ditherMotion", "0"}, {"pixelMemoryCellSize", "6"}, {"pixelMemoryThreshold", "46"}, {"pixelMemoryFeedback", "92"}, {"pixelMemoryWindows", "4"}, {"pixelMemoryEntropy", "52"}, {"pixelMemoryOpacity", "78"}, {"nodeShapes", "true"}, {"network", "false"}, {"circles", "false"}, {"labels", "false"}, {"grid", "false"}, {"ditherPrint", "false"}, {"ditherInvert", "false"}, {"pixelMemory", "true"}, {"ditherAlgorithm", "atkinson"}, {"ditherPalette", "cobalt"}, {"ditherBlend", "Normal"}, {"pixelMemoryStyle", "feedback"}, {"pixelMemoryPalette", "signal"}, {"palette", "solar"}, {"colorMode", "source"}, {"blendMode", "screen"}, {"primitive", "bracket"}, {"connectionMode", "contour"}, {"motionMode", "glitch"}, {"seed", "426"}, {"motionAmount", "52"}, {"bpm", "120"}, {"beatOffset", "0"}, {"syncStrength", "72"}, {"bassGain", "88"}, {"midGain", "62"}, {"highGain", "54"}}},
    {"Demo - Lines Gray", {{"contourField", "false"}, {"contourStyle", "flow"}, {"contourLevels", "16"}, {"contourDetail", "78"}, {"contourOpacity", "80"}, {"contourDrift", "25"}, {"objectFocus", "emphasize"}, {"objectStrength", "72"}, {"objectConfidence", "45"}, {"artworkFrame", "true"}, {"density", "69"}, {"threshold", "43"}, {"pointRadius", "0.54"}, {"shapeMin", "19"}, {"shapeMax", "36"}, {"featureSizing", "37"}, {"sizeVariation", "65"}, {"sizeHierarchy", "98"}, {"shapeStroke", "0.85"}, {"connection", "38"}, {"edgeConfidence", "10"}, {"lineWeight", "0.98"}, {"scanlines", "78"}, {"grain", "61"}, {"baseOpacity", "71"}, {"overlayOpacity", "58"}, {"motionSpeed", "10"}, {"gridModules", "12"}, {"gridOpacity", "18"}, {"ditherCellSize", "12.75"}, {"ditherThreshold", "57"}, {"ditherContrast", "74"}, {"ditherOpacity", "100"}, {"ditherMotion", "0"}, {"pixelMemoryCellSize", "13"}, {"pixelMemoryThreshold", "41"}, {"pixelMemoryFeedback", "73"}, {"pixelMemoryWindows", "8"}, {"pixelMemoryEntropy", "77"}, {"pixelMemoryOpacity", "86"}, {"nodeShapes", "true"}, {"network", "false"}, {"circles", "true"}, {"labels", "true"}, {"grid", "false"}, {"ditherPrint", "false"}, {"ditherInvert", "false"}, {"pixelMemory", "true"}, {"ditherAlgorithm", "atkinson"}, {"ditherPalette", "cyan"}, {"ditherBlend", "screen"}, {"pixelMemoryStyle", "archive"}, {"pixelMemoryPalette", "mono"}, {"palette", "mono"}, {"colorMode", "duotone"}, {"blendMode", "darken"}, {"primitive", "square"}, {"connectionMode", "contour"}, {"motionMode", "orbit"}, {"seed", "439"}, {"motionAmount", "73"}, {"bpm", "120"}, {"beatOffset", "0"}, {"syncStrength", "72"}, {"bassGain", "88"}, {"midGain", "62"}, {"highGain", "54"}}},
    {"Demo - Circles", {{"contourField", "true"}, {"contourStyle", "flow"}, {"contourLevels", "16"}, {"contourDetail", "71"}, {"contourOpacity", "80"}, {"contourDrift", "17"}, {"objectFocus", "emphasize"}, {"objectStrength", "72"}, {"objectConfidence", "45"}, {"artworkFrame", "true"}, {"density", "88"}, {"threshold", "36"}, {"pointRadius", "2.58"}, {"shapeMin", "10"}, {"shapeMax", "86"}, {"featureSizing", "36"}, {"sizeVariation", "63"}, {"sizeHierarchy", "74"}, {"shapeStroke", "1.39"}, {"connection", "152"}, {"edgeConfidence", "36"}, {"lineWeight", "1.82"}, {"scanlines", "8"}, {"grain", "36"}, {"baseOpacity", "77"}, {"overlayOpacity", "82"}, {"motionSpeed", "28"}, {"gridModules", "12"}, {"gridOpacity", "9"}, {"ditherCellSize", "23"}, {"ditherThreshold", "63"}, {"ditherContrast", "81"}, {"ditherOpacity", "100"}, {"ditherMotion", "0"}, {"pixelMemoryCellSize", "16.5"}, {"pixelMemoryThreshold", "45"}, {"pixelMemoryFeedback", "32"}, {"pixelMemoryWindows", "5"}, {"pixelMemoryEntropy", "99"}, {"pixelMemoryOpacity", "88"}, {"nodeShapes", "true"}, {"network", "true"}, {"circles", "true"}, {"labels", "true"}, {"grid", "false"}, {"ditherPrint", "false"}, {"ditherInvert", "false"}, {"pixelMemory", "false"}, {"ditherAlgorithm", "atkinson"}, {"ditherPalette", "signal"}, {"ditherBlend", "Normal"}, {"pixelMemoryStyle", "glyphs"}, {"pixelMemoryPalette", "mono"}, {"palette", "solar"}, {"colorMode", "hybrid"}, {"blendMode", "Normal"}, {"primitive", "diamond"}, {"connectionMode", "radial"}, {"motionMode", "pulse"}, {"seed", "289"}, {"motionAmount", "10"}, {"bpm", "120"}, {"beatOffset", "0"}, {"syncStrength", "72"}, {"bassGain", "88"}, {"midGain", "62"}, {"highGain", "54"}}},
    {"Demo - Dither", {{"contourField", "false"}, {"contourStyle", "flow"}, {"contourLevels", "9"}, {"contourDetail", "62"}, {"contourOpacity", "80"}, {"contourDrift", "30"}, {"objectFocus", "emphasize"}, {"objectStrength", "72"}, {"objectConfidence", "45"}, {"artworkFrame", "true"}, {"density", "66"}, {"threshold", "42"}, {"pointRadius", "3.74"}, {"shapeMin", "7"}, {"shapeMax", "34"}, {"featureSizing", "97"}, {"sizeVariation", "98"}, {"sizeHierarchy", "62"}, {"shapeStroke", "1.5"}, {"connection", "29"}, {"edgeConfidence", "53"}, {"lineWeight", "2.02"}, {"scanlines", "73"}, {"grain", "50"}, {"baseOpacity", "60"}, {"overlayOpacity", "44"}, {"motionSpeed", "88"}, {"gridModules", "12"}, {"gridOpacity", "15"}, {"ditherCellSize", "3"}, {"ditherThreshold", "48"}, {"ditherContrast", "45"}, {"ditherOpacity", "75"}, {"ditherMotion", "29"}, {"pixelMemoryCellSize", "6"}, {"pixelMemoryThreshold", "56"}, {"pixelMemoryFeedback", "79"}, {"pixelMemoryWindows", "4"}, {"pixelMemoryEntropy", "93"}, {"pixelMemoryOpacity", "88"}, {"nodeShapes", "true"}, {"network", "true"}, {"circles", "false"}, {"labels", "true"}, {"grid", "false"}, {"ditherPrint", "true"}, {"ditherInvert", "false"}, {"pixelMemory", "false"}, {"ditherAlgorithm", "Floyd-Steinberg"}, {"ditherPalette", "forest"}, {"ditherBlend", "overlay"}, {"pixelMemoryStyle", "feedback"}, {"pixelMemoryPalette", "cobalt"}, {"palette", "mono"}, {"colorMode", "source"}, {"blendMode", "luminosity"}, {"primitive", "mixed"}, {"connectionMode", "orthogonal"}, {"motionMode", "glitch"}, {"seed", "481"}, {"motionAmount", "63"}, {"bpm", "120"}, {"beatOffset", "0"}, {"syncStrength", "72"}, {"bassGain", "88"}, {"midGain", "62"}, {"highGain", "54"}}},
  };
  return presets;
}

Params presetParams(size_t index) {
  Params p;
  const auto& list = builtinPresets();
  if (index < list.size())
    for (const auto& kv : list[index].values) setFromText(p, kv.first, kv.second);
  return p;
}

// ---------- user preset files ----------
namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::string valueText(const ParamSpec& s, double v) {
  if (s.type == PType::Choice) {
    size_t i = size_t(std::max(0.0, v) + 0.5);
    return i < s.choices->size() ? (*s.choices)[i] : std::to_string(i);
  }
  if (s.type == PType::Bool) return v >= 0.5 ? "true" : "false";
  if (s.type == PType::Int) return std::to_string(long(v >= 0 ? v + 0.5 : v - 0.5));
  std::ostringstream o;
  o << v;
  return o.str();
}

double paramValue(const Params& p, const std::string& id) {
#define AK_X(pid, label, group, type, def, mn, mx, choices) if (id == #pid) return p.pid;
  AK_PARAM_LIST(AK_X)
#undef AK_X
  return 0;
}

std::string presetToText(const Params& p, const std::string& name) {
  std::ostringstream o;
  o << "# AndyKit preset - one parameter per line, edit freely.\n";
  o << "# Missing parameters use their default value; unknown lines are ignored.\n";
  o << "format = andykit-preset\nversion = 1\nname = " << name << "\n";
  std::string group;
  for (const auto& s : paramSpecs()) {
    if (group != s.group) { group = s.group; o << "\n# " << group << "\n"; }
    o << s.id << " = " << valueText(s, paramValue(p, s.id)) << "\n";
  }
  return o.str();
}

bool presetFromText(const std::string& text, Params& out, std::string* name, std::string* error) {
  Params p;
  std::istringstream in(text);
  std::string line, nm;
  int read = 0;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
    if (k == "name") { nm = v; continue; }
    if (k == "format") {
      if (v != "andykit-preset") { if (error) *error = "not an AndyKit preset"; return false; }
      continue;
    }
    if (k == "version") continue;
    if (setFromText(p, k, v)) ++read;
  }
  if (read == 0) { if (error) *error = "no AndyKit parameters found"; return false; }
  out = p;
  if (name) *name = nm;
  return true;
}

bool savePresetFile(const std::string& path, const Params& p, const std::string& name, std::string* error) {
  try {
    fs::path fp = fs::u8path(path);
    if (fp.has_parent_path()) fs::create_directories(fp.parent_path());
    std::ofstream f(fp, std::ios::binary);
    if (!f) { if (error) *error = "cannot write " + path; return false; }
    f << presetToText(p, name);
    return bool(f);
  } catch (const std::exception& e) {
    if (error) *error = e.what();
    return false;
  }
}

bool loadPresetFile(const std::string& path, Params& out, std::string* name, std::string* error) {
  try {
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) { if (error) *error = "cannot open " + path; return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    return presetFromText(ss.str(), out, name, error);
  } catch (const std::exception& e) {
    if (error) *error = e.what();
    return false;
  }
}

std::string defaultPresetFolder() {
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");
#else
  const char* home = std::getenv("HOME");
#endif
  fs::path base = home ? fs::u8path(home) : fs::current_path();
  return (base / "Documents" / "AndyKit Presets").u8string();
}

std::string presetFileName(const std::string& folder, const std::string& name) {
  std::string clean;
  for (char ch : name) clean += (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ? '-' : ch;
  clean = trim(clean);
  if (clean.empty()) clean = "My Preset";
  return (fs::u8path(folder) / fs::u8path(clean + kPresetExtension)).u8string();
}

std::vector<UserPreset> scanPresetFolder(const std::string& dir) {
  std::vector<UserPreset> out;
  try {
    fs::path d = fs::u8path(dir);
    if (!fs::is_directory(d)) return out;
    for (const auto& e : fs::recursive_directory_iterator(d, fs::directory_options::skip_permission_denied)) {
      if (!e.is_regular_file() || e.path().extension() != kPresetExtension) continue;
      Params p;
      std::string nm;
      if (!loadPresetFile(e.path().u8string(), p, &nm, nullptr)) continue;
      if (nm.empty()) nm = e.path().stem().u8string();
      out.push_back({nm, e.path().u8string()});
      if (out.size() >= 200) break;
    }
  } catch (...) {}
  std::sort(out.begin(), out.end(), [](const UserPreset& a, const UserPreset& b) { return a.name < b.name; });
  return out;
}

} // namespace ak
