// ak_render — renders AndyKit on still images or image sequences without a host.
// Used for development and CI checks.
//
//   ak_render <input> <output.png> [--preset "Andy Lines"] [--frames N] [--fps 25] [key=value ...]
//
// With --frames N the same input is rendered as N frames of a still (motion, memory and
// rhythm evolve over time) and written as output_0000.png ... output_NNNN.png.
// SPDX-License-Identifier: MIT
#include "core/Engine.h"
#include "core/Presets.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static float toLinearish(unsigned char v) { return v / 255.f; } // the engine works on display-referred values

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <input> <output.png> [--preset name] [--frames N] [--fps F] [key=value ...]\n", argv[0]);
    std::fprintf(stderr, "presets:");
    for (auto& p : ak::builtinPresets()) std::fprintf(stderr, " \"%s\"", p.name.c_str());
    std::fprintf(stderr, "\n");
    return 1;
  }
  std::string in = argv[1], out = argv[2];
  int frames = 1;
  double fps = 25;
  ak::Params params;
  std::vector<std::pair<std::string, std::string>> overrides;
  for (int i = 3; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--preset" && i + 1 < argc) {
      std::string name = argv[++i];
      bool found = false;
      for (size_t k = 0; k < ak::builtinPresets().size(); ++k)
        if (ak::builtinPresets()[k].name == name) { params = ak::presetParams(k); found = true; }
      if (!found) { std::fprintf(stderr, "unknown preset: %s\n", name.c_str()); return 1; }
    } else if (a == "--frames" && i + 1 < argc) frames = std::max(1, std::atoi(argv[++i]));
    else if (a == "--fps" && i + 1 < argc) fps = std::atof(argv[++i]);
    else if (auto eq = a.find('='); eq != std::string::npos) overrides.push_back({a.substr(0, eq), a.substr(eq + 1)});
    else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 1; }
  }
  for (auto& kv : overrides)
    if (!ak::setFromText(params, kv.first, kv.second)) { std::fprintf(stderr, "bad parameter: %s=%s\n", kv.first.c_str(), kv.second.c_str()); return 1; }

  int w, h, n;
  unsigned char* data = stbi_load(in.c_str(), &w, &h, &n, 4);
  if (!data) { std::fprintf(stderr, "cannot read %s\n", in.c_str()); return 1; }
  ak::Image src;
  src.resize(w, h);
  for (size_t i = 0; i < size_t(w) * h * 4; ++i) src.d[i] = toLinearish(data[i]);
  stbi_image_free(data);

  ak::Analysis analysis = ak::analyze(src);
  std::vector<ak::HistoryFrame> history;
  std::vector<unsigned char> buf(size_t(w) * h * 4);
  for (int f = 0; f < frames; ++f) {
    ak::Image img = src;
    ak::RenderInfo info;
    info.frame = f;
    info.seconds = f / fps;
    auto t0 = std::chrono::steady_clock::now();
    ak::render(img, analysis, history, params, info);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    for (size_t i = 0; i < buf.size(); ++i) buf[i] = (unsigned char)std::lround(std::min(1.f, std::max(0.f, img.d[i])) * 255);
    std::string path = out;
    if (frames > 1) {
      char suffix[32];
      std::snprintf(suffix, sizeof suffix, "_%04d.png", f);
      auto dot = path.rfind(".png");
      path = (dot == std::string::npos ? path : path.substr(0, dot)) + suffix;
    }
    if (!stbi_write_png(path.c_str(), w, h, 4, buf.data(), w * 4)) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
    std::printf("%s  %dx%d  %.1f ms\n", path.c_str(), w, h, ms);
    history.push_back({&analysis, f, info.seconds});
  }
  return 0;
}
