// AndyKit — host-independent rendering engine.
// SPDX-License-Identifier: MIT
#pragma once
#include "Params.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ak {

// RGBA float image, rows top-to-bottom, 4 floats per pixel.
struct Image {
  int w = 0, h = 0;
  std::vector<float> d;
  void resize(int W, int H) { w = W; h = H; d.assign(size_t(W) * H * 4, 0.f); }
  float* px(int x, int y) { return &d[(size_t(y) * w + x) * 4]; }
  const float* px(int x, int y) const { return &d[(size_t(y) * w + x) * 4]; }
};

// Low-resolution analysis of one frame (luminance, colour, Sobel gradient).
struct Analysis {
  int w = 0, h = 0;
  std::vector<float> lum, r, g, b, mag, gx, gy; // lum/rgb in 0..255
  float maxMag = 1.f, meanMag = 0.f;
};

// Builds the analysis of a frame. targetW is the analysis width (height follows aspect).
Analysis analyze(const Image& src, int targetW = 360);

struct HistoryFrame {
  const Analysis* analysis;
  int frame;      // frame number of that analysis
  double seconds; // time of that frame
};

struct RenderInfo {
  double seconds = 0;          // current time in seconds
  int frame = 0;               // current frame number
  std::string title = "ANDYKIT";
};

// Renders the effect in place on `img` (which holds the source frame).
// `history` holds earlier frames (oldest first) for temporal layers; may be empty.
void render(Image& img, const Analysis& current, const std::vector<HistoryFrame>& history,
            const Params& p, const RenderInfo& info);

} // namespace ak
