// SPDX-License-Identifier: MIT
#include "Params.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ak {

const std::vector<ParamSpec>& paramSpecs() {
  static const std::vector<ParamSpec> specs = {
#define AK_X(id, label, group, type, def, mn, mx, choices) {#id, label, group, PType::type, double(def), double(mn), double(mx), choices},
    AK_PARAM_LIST(AK_X)
#undef AK_X
  };
  return specs;
}

const ParamSpec* findSpec(const std::string& id) {
  for (const auto& s : paramSpecs())
    if (id == s.id) return &s;
  return nullptr;
}

const ParamSpec* groupEnable(const std::string& group) {
  for (const auto& s : paramSpecs())
    if (group == s.group && s.type == PType::Bool && std::string(s.label).rfind("Enable ", 0) == 0) return &s;
  return nullptr;
}

bool Params::set(const std::string& key, double v) {
#define AK_X(id, label, group, type, def, mn, mx, choices) \
  if (key == #id) { id = std::clamp(v, double(mn), double(mx)); return true; }
  AK_PARAM_LIST(AK_X)
#undef AK_X
  return false;
}

static std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == ' ' || c == '-' || c == '_'; }), s.end());
  return s;
}

bool setFromText(Params& p, const std::string& key, const std::string& value) {
  const ParamSpec* s = findSpec(key);
  if (!s) return false;
  std::string v = lower(value);
  if (s->type == PType::Choice) {
    for (size_t i = 0; i < s->choices->size(); ++i)
      if (lower((*s->choices)[i]) == v) return p.set(key, double(i));
  }
  if (v == "true" || v == "on" || v == "yes") return p.set(key, 1);
  if (v == "false" || v == "off" || v == "no") return p.set(key, 0);
  char* end = nullptr;
  double d = std::strtod(value.c_str(), &end);
  if (end == value.c_str()) return false;
  return p.set(key, d);
}

} // namespace ak
