// AndyKit — host-independent rendering engine.
// SPDX-License-Identifier: MIT
#include "Engine.h"
#include "Font5x7.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_map>

namespace ak {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

inline float clampf(float v, float a = 0.f, float b = 1.f) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Deterministic hash in [0,1) from two integers and the seed.
inline float hash01(int64_t a, int64_t b, int seed) {
  uint32_t h = uint32_t(a) * 374761393u ^ uint32_t(b) * 668265263u ^ uint32_t(seed) * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return float(h) / 4294967296.f;
}

struct Col { float r, g, b; };

// Palettes (sRGB 0..1). Index order matches kPalettes.
const std::vector<Col>& palette(int idx) {
  static const std::vector<std::vector<Col>> P = [] {
    auto hex = [](uint32_t v) { return Col{((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; };
    std::vector<std::vector<uint32_t>> raw = {
      {0xff4d1a, 0xff9a1f, 0xffd23f, 0xfff3c4, 0xff2e63}, // Solar
      {0x00ff9c, 0x00d1ff, 0xff2bd6, 0xf4f4f0},           // Signal
      {0x081a7a, 0x1f4fff, 0x7fa7ff, 0xeef2ff},           // Cobalt
      {0x1a1a1a, 0x7a7a7a, 0xbdbdbd, 0xffffff},           // Mono
      {0x2b0a05, 0x8a1c0b, 0xff5a1f, 0xffd1a1},           // Ember
      {0xc8ff00, 0x00ffd5, 0xff00a8, 0xffffff},           // Acid
      {0x0b2416, 0x1f5c3a, 0x6fae6b, 0xe4f0d0},           // Forest
      {0x02232b, 0x00799a, 0x3fd6f2, 0xe6fbff}};          // Cyan
    std::vector<std::vector<Col>> out;
    for (auto& p : raw) { std::vector<Col> c; for (auto v : p) c.push_back(hex(v)); out.push_back(c); }
    return out;
  }();
  return P[std::clamp(idx, 0, int(P.size()) - 1)];
}

template <class F> void parallelRows(int h, F f) {
  unsigned n = std::max(1u, std::thread::hardware_concurrency());
  if (h < 64 || n == 1) { f(0, h); return; }
  n = std::min<unsigned>(n, 16);
  std::vector<std::thread> ts;
  int chunk = (h + int(n) - 1) / int(n);
  for (int y0 = 0; y0 < h; y0 += chunk) ts.emplace_back(f, y0, std::min(h, y0 + chunk));
  for (auto& t : ts) t.join();
}

// ---------- drawing on premultiplied RGBA layers ----------
inline void plot(Image& im, int x, int y, const Col& c, float a) {
  if (a <= 0.f || x < 0 || y < 0 || x >= im.w || y >= im.h) return;
  float* p = im.px(x, y);
  a = std::min(a, 1.f);
  p[0] = c.r * a + p[0] * (1 - a);
  p[1] = c.g * a + p[1] * (1 - a);
  p[2] = c.b * a + p[2] * (1 - a);
  p[3] = a + p[3] * (1 - a);
}

// Anti-aliased segment; walks the major axis so long lines stay cheap.
void segment(Image& im, float x0, float y0, float x1, float y1, float width, const Col& c, float alpha) {
  if (alpha <= 0.003f) return;
  float hw = std::max(0.5f, width * 0.5f);
  float dim = std::min(1.f, width); // hairlines get dimmer instead of thinner
  float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy;
  auto cover = [&](int x, int y) {
    float px = x + 0.5f, py = y + 0.5f;
    float t = L2 > 0 ? clampf(((px - x0) * dx + (py - y0) * dy) / L2) : 0.f;
    float ex = x0 + t * dx - px, ey = y0 + t * dy - py;
    float cov = clampf(hw + 0.5f - std::sqrt(ex * ex + ey * ey));
    if (cov > 0) plot(im, x, y, c, alpha * cov * dim);
  };
  int r = int(std::ceil(hw + 1));
  if (std::fabs(dx) >= std::fabs(dy)) {
    if (x1 < x0) { std::swap(x0, x1); std::swap(y0, y1); dx = -dx; dy = -dy; }
    int xa = std::max(0, int(std::floor(x0 - hw - 1))), xb = std::min(im.w - 1, int(std::ceil(x1 + hw + 1)));
    for (int x = xa; x <= xb; ++x) {
      float t = dx != 0 ? clampf((x + 0.5f - x0) / dx) : 0.f;
      int yc = int(std::floor(y0 + t * dy));
      for (int y = yc - r; y <= yc + r; ++y) cover(x, y);
    }
  } else {
    if (y1 < y0) { std::swap(x0, x1); std::swap(y0, y1); dx = -dx; dy = -dy; }
    int ya = std::max(0, int(std::floor(y0 - hw - 1))), yb = std::min(im.h - 1, int(std::ceil(y1 + hw + 1)));
    for (int y = ya; y <= yb; ++y) {
      float t = dy != 0 ? clampf((y + 0.5f - y0) / dy) : 0.f;
      int xc = int(std::floor(x0 + t * dx));
      for (int x = xc - r; x <= xc + r; ++x) cover(x, y);
    }
  }
}

void ring(Image& im, float cx, float cy, float rad, float width, const Col& c, float alpha) {
  if (alpha <= 0.003f || rad <= 0) return;
  float hw = std::max(0.5f, width * 0.5f), dim = std::min(1.f, width);
  int xa = std::max(0, int(cx - rad - hw - 2)), xb = std::min(im.w - 1, int(cx + rad + hw + 2));
  int ya = std::max(0, int(cy - rad - hw - 2)), yb = std::min(im.h - 1, int(cy + rad + hw + 2));
  float inner = std::max(0.f, rad - hw - 1.5f);
  for (int y = ya; y <= yb; ++y) {
    float py = y + 0.5f - cy;
    for (int x = xa; x <= xb; ++x) {
      float px = x + 0.5f - cx, d = std::sqrt(px * px + py * py);
      if (d < inner) { // skip the hollow inside quickly
        int jump = int(cx + std::sqrt(std::max(0.f, inner * inner - py * py)) - 1);
        if (jump > x) { x = jump; continue; }
      }
      float cov = clampf(hw + 0.5f - std::fabs(d - rad));
      if (cov > 0) plot(im, x, y, c, alpha * cov * dim);
    }
  }
}

void disc(Image& im, float cx, float cy, float rad, const Col& c, float alpha) {
  if (alpha <= 0.003f || rad <= 0) return;
  int xa = std::max(0, int(cx - rad - 2)), xb = std::min(im.w - 1, int(cx + rad + 2));
  int ya = std::max(0, int(cy - rad - 2)), yb = std::min(im.h - 1, int(cy + rad + 2));
  for (int y = ya; y <= yb; ++y)
    for (int x = xa; x <= xb; ++x) {
      float px = x + 0.5f - cx, py = y + 0.5f - cy;
      float cov = clampf(rad + 0.5f - std::sqrt(px * px + py * py));
      if (cov > 0) plot(im, x, y, c, alpha * cov);
    }
}

void text(Image& im, float x, float y, const std::string& s, float scale, const Col& c, float alpha) {
  int k = std::max(1, int(std::round(scale)));
  float cx = x;
  for (char ch : s) {
    const uint8_t* g = glyph5x7(ch);
    for (int row = 0; row < 7; ++row)
      for (int col = 0; col < 5; ++col)
        if (g[row] & (0x10 >> col))
          for (int yy = 0; yy < k; ++yy)
            for (int xx = 0; xx < k; ++xx) plot(im, int(cx) + col * k + xx, int(y) + row * k + yy, c, alpha);
    cx += 6 * k;
  }
}
inline float textWidth(const std::string& s, float scale) { return s.size() * 6.f * std::max(1, int(std::round(scale))); }

// ---------- blending ----------
inline float lumOf(float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

inline float blendCh(int mode, float b, float s) {
  switch (mode) {
    case 0: return s;                                   // Normal
    case 1: return b + s - b * s;                       // Screen
    case 2: return b + s;                               // Add
    case 3: { float c = clampf(b); return c <= 0.5f ? 2 * c * s : 1 - 2 * (1 - c) * (1 - s); } // Overlay
    case 4: { float c = clampf(b);                      // Soft light (W3C)
      if (s <= 0.5f) return c - (1 - 2 * s) * c * (1 - c);
      float d = c <= 0.25f ? ((16 * c - 12) * c + 4) * c : std::sqrt(c);
      return c + (2 * s - 1) * (d - c); }
    case 5: { float c = clampf(b); return s <= 0.5f ? 2 * c * s : 1 - 2 * (1 - c) * (1 - s); } // Hard light
    case 6: return b * s;                               // Multiply
    case 7: return std::min(b, s);                      // Darken
    case 8: return std::max(b, s);                      // Lighten
    case 9: return std::fabs(b - s);                    // Difference
    case 10: return s >= 1.f ? std::max(b, 1.f) : std::min(1.f, clampf(b) / (1 - s)); // Color dodge
    default: return s;
  }
}

inline void blendPixel(float* bp, float sr, float sg, float sb, float a, int mode) {
  if (a <= 0) return;
  float rr, rg, rb;
  if (mode == 11) { // Luminosity: backdrop hue/sat with source luminance
    float d = lumOf(sr, sg, sb) - lumOf(bp[0], bp[1], bp[2]);
    rr = bp[0] + d; rg = bp[1] + d; rb = bp[2] + d;
    float l = lumOf(rr, rg, rb), n = std::min({rr, rg, rb}), x = std::max({rr, rg, rb});
    if (n < 0) { rr = l + (rr - l) * l / (l - n + 1e-6f); rg = l + (rg - l) * l / (l - n + 1e-6f); rb = l + (rb - l) * l / (l - n + 1e-6f); }
    if (x > 1 && l < 1) { rr = l + (rr - l) * (1 - l) / (x - l); rg = l + (rg - l) * (1 - l) / (x - l); rb = l + (rb - l) * (1 - l) / (x - l); }
  } else {
    rr = blendCh(mode, bp[0], sr); rg = blendCh(mode, bp[1], sg); rb = blendCh(mode, bp[2], sb);
  }
  bp[0] = bp[0] * (1 - a) + rr * a;
  bp[1] = bp[1] * (1 - a) + rg * a;
  bp[2] = bp[2] * (1 - a) + rb * a;
}

// Composites a premultiplied layer onto img.
void composite(Image& img, const Image& layer, int mode, float opacity) {
  if (opacity <= 0) return;
  parallelRows(img.h, [&](int y0, int y1) {
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < img.w; ++x) {
        const float* s = layer.px(x, y);
        if (s[3] <= 1e-4f) continue;
        float inv = 1.f / s[3];
        blendPixel(img.px(x, y), clampf(s[0] * inv), clampf(s[1] * inv), clampf(s[2] * inv), s[3] * opacity, mode);
      }
  });
}

// ---------- per-frame state ----------
struct Energy { float bass = 0, mid = 0, high = 0; };

Energy rhythm(const Params& p, double sec) {
  Energy e;
  if (p.rhythm < 0.5) return e;
  double ph = std::fmod(sec * p.bpm / 60.0 + p.beatOffset / 100.0, 1.0);
  float pulse = float(std::exp(-ph * 7.0)), k = float(p.syncStrength / 100.0);
  e.bass = pulse * 0.8f * float(p.bassGain / 100) * k;
  e.mid = pulse * 0.45f * float(p.midGain / 100) * k;
  e.high = pulse * 0.3f * float(p.highGain / 100) * k;
  return e;
}

struct Pt {
  float x, y, m, lum, gx, gy, r, g, b, rnd;
  int id;
  Col col;
  float fw = 1, fa = 1, score = 0, px = 0, py = 0, size = 0, pulse = 1;
};

struct Focus { float x, y, sx, sy; };

struct Ctx {
  Image& img;
  Image ov;
  const Analysis& A;
  const Params& p;
  const RenderInfo& info;
  int W, H, seed;
  float unit, kx, ky, cellPx = 1;
  Energy E;
  Focus F{};
  std::vector<Pt> pts;
  Ctx(Image& i, const Analysis& a, const Params& pp, const RenderInfo& in)
      : img(i), A(a), p(pp), info(in), W(i.w), H(i.h), seed(int(pp.seed)) {
    unit = std::min(W, H) / 720.f;
    kx = float(W) / A.w;
    ky = float(H) / A.h;
  }
};

// ---------- points, focus, colour ----------
void buildPoints(Ctx& c) {
  const Analysis& A = c.A;
  const Params& p = c.p;
  int step = std::max(2, int(std::lround(lerpf(16, 3, float(p.density / 100)))));
  float thr = A.meanMag + float(p.threshold / 100) * (A.maxMag - A.meanMag) * 0.5f;
  c.cellPx = step * c.kx;
  for (int cy = 0; cy < A.h; cy += step)
    for (int cx = 0; cx < A.w; cx += step) {
      int best = -1;
      float bm = thr;
      for (int y = cy; y < std::min(cy + step, A.h); ++y)
        for (int x = cx; x < std::min(cx + step, A.w); ++x) {
          int i = y * A.w + x;
          if (A.mag[i] > bm) { bm = A.mag[i]; best = i; }
        }
      if (best < 0) continue;
      int x = best % A.w, y = best / A.w, id = (cy / step) * 4096 + cx / step;
      Pt q{};
      q.x = (x + 0.5f) * c.kx; q.y = (y + 0.5f) * c.ky; q.m = bm / A.maxMag; q.lum = A.lum[best];
      q.gx = A.gx[best]; q.gy = A.gy[best]; q.r = A.r[best]; q.g = A.g[best]; q.b = A.b[best];
      q.id = id; q.rnd = hash01(id, 7, c.seed);
      c.pts.push_back(q);
    }
}

void computeFocus(Ctx& c) {
  double sw = 0, mx = 0, my = 0;
  for (auto& q : c.pts) { double w = q.m * q.m; sw += w; mx += q.x * w; my += q.y * w; }
  if (sw <= 0) { c.F = {c.W / 2.f, c.H / 2.f, c.W / 4.f, c.H / 4.f}; return; }
  mx /= sw; my /= sw;
  double vx = 0, vy = 0;
  for (auto& q : c.pts) { double w = q.m * q.m; vx += w * (q.x - mx) * (q.x - mx); vy += w * (q.y - my) * (q.y - my); }
  c.F = {float(mx), float(my), float(std::sqrt(vx / sw)) + 1, float(std::sqrt(vy / sw)) + 1};
}

void preparePoints(Ctx& c) {
  const Params& p = c.p;
  int mode = int(p.colorMode), motion = p.motionEnable >= 0.5 ? int(p.motionMode) : 0;
  int focus = p.subjectEnable >= 0.5 ? int(p.objectFocus) : 2;
  const auto& P = palette(int(p.palette));
  float mo = float(p.motionAmount / 100), sp = float(p.motionSpeed / 100);
  float str = float(p.objectStrength / 100), kf = lerpf(2.2f, 1.0f, float(p.objectConfidence / 100));
  float t = float(c.info.seconds);
  int64_t tick = int64_t(std::floor(t / lerpf(0.5f, 0.06f, sp)));
  for (auto& q : c.pts) {
    float mx = std::max({q.r, q.g, q.b, 1.f}), f = std::min(3.f, std::max(1.f, 220.f / mx));
    Col src{std::min(255.f, q.r * f) / 255, std::min(255.f, q.g * f) / 255, std::min(255.f, q.b * f) / 255};
    const Col& pc = P[std::min<size_t>(P.size() - 1, size_t(q.lum / 256 * P.size()))];
    float l = q.lum / 255;
    Col duo{lerpf(P.front().r, P.back().r, l), lerpf(P.front().g, P.back().g, l), lerpf(P.front().b, P.back().b, l)};
    switch (mode) {
      case 0: q.col = src; break;
      case 1: q.col = pc; break;
      case 2: q.col = {0.94f, 0.94f, 0.94f}; break;
      case 3: q.col = duo; break;
      default: q.col = {(src.r + pc.r) / 2, (src.g + pc.g) / 2, (src.b + pc.b) / 2};
    }
    float fw = 1;
    if (focus != 2) {
      float dx = (q.x - c.F.x) / (c.F.sx * kf + 1), dy = (q.y - c.F.y) / (c.F.sy * kf + 1);
      fw = clampf(1.5f - std::sqrt(dx * dx + dy * dy));
    }
    q.fw = fw;
    q.fa = focus == 0 ? 1 - (1 - fw) * str * 0.6f : focus == 1 ? 1 - (1 - fw) * str : 1.f;
    q.score = q.m * (0.4f + 0.6f * fw);
    float ox = 0, oy = 0;
    q.pulse = 1;
    if (motion == 2) { // Drift
      float w = t * 1.5f * (0.2f + sp);
      ox = std::sin(w + q.rnd * kTau) * mo * 6 * c.unit;
      oy = std::cos(w * 1.3f + q.rnd * kTau) * mo * 6 * c.unit;
    } else if (motion == 1) { // Glitch
      if (hash01(tick + q.id, 61, c.seed) < mo * 0.05f + c.E.high * 0.05f) ox = (hash01(tick + q.id, 62, c.seed) - 0.5f) * mo * 50 * c.unit;
    } else if (motion == 3) { // Pulse
      q.pulse = 1 + 0.3f * mo * std::sin(t * kTau * (0.25f + sp * 1.5f) + q.rnd * kTau);
    } else if (motion == 4) { // Orbit around the focus
      float dx = q.x - c.F.x, dy = q.y - c.F.y, d = std::sqrt(dx * dx + dy * dy);
      float maxd = std::sqrt(float(c.W) * c.W + float(c.H) * c.H) * 0.5f;
      float a = mo * 0.22f * std::sin(t * (0.3f + sp * 1.6f) + q.rnd * 0.6f) * (1.f - 0.6f * d / maxd);
      float ca = std::cos(a), sa = std::sin(a);
      ox = c.F.x + dx * ca - dy * sa - q.x;
      oy = c.F.y + dx * sa + dy * ca - q.y;
    }
    q.px = q.x + ox;
    q.py = q.y + oy;
  }
}

Col mixCol(const Col& a, const Col& b) { return {(a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2}; }

// ---------- connections ----------
void drawConnections(Ctx& c) {
  const Params& p = c.p;
  int mode = int(p.connectionMode);
  if (p.connectionsEnable < 0.5 || mode == 4 || p.connection <= 0 || c.pts.size() < 2) return;
  float conn = float(p.connection / 100);
  float R = c.cellPx * lerpf(1.3f, 3.6f, conn);
  int maxL = p.connection > 133 ? 4 : p.connection > 66 ? 3 : p.connection > 33 ? 2 : 1;
  std::unordered_map<int64_t, std::vector<int>> grid;
  auto key = [](int i, int j) { return int64_t(i) * 1000003 + j; };
  for (int i = 0; i < int(c.pts.size()); ++i) grid[key(int(c.pts[i].x / R), int(c.pts[i].y / R))].push_back(i);
  float need = 0.3f + 0.55f * float(p.edgeConfidence / 100), baseA = 0.32f + 0.5f * c.E.mid;
  float lw = float(p.lineWeight) * c.unit;
  std::vector<std::pair<int, float>> cand;
  for (int i = 0; i < int(c.pts.size()); ++i) {
    const Pt& a = c.pts[i];
    int gx = int(a.x / R), gy = int(a.y / R);
    float gl = std::sqrt(a.gx * a.gx + a.gy * a.gy) + 1e-6f, nx = a.gx / gl, ny = a.gy / gl;
    cand.clear();
    for (int di = -1; di <= 1; ++di)
      for (int dj = -1; dj <= 1; ++dj) {
        auto it = grid.find(key(gx + di, gy + dj));
        if (it == grid.end()) continue;
        for (int j : it->second) {
          if (j <= i) continue;
          const Pt& b = c.pts[j];
          float dx = b.x - a.x, dy = b.y - a.y, d = std::sqrt(dx * dx + dy * dy);
          if (d > R || d < 1) continue;
          if (mode == 0) {
            float along = 1 - std::fabs((dx * nx + dy * ny) / d), ld = std::fabs(a.lum - b.lum) / 255;
            if (along < need || ld > 0.3f) continue;
            cand.push_back({j, d - along * R * 0.3f});
          } else cand.push_back({j, d});
        }
      }
    std::sort(cand.begin(), cand.end(), [](auto& u, auto& v) { return u.second < v.second; });
    for (int k = 0; k < std::min<int>(maxL, int(cand.size())); ++k) {
      const Pt& b = c.pts[cand[k].first];
      float al = baseA * std::min(a.fa, b.fa);
      Col col = mixCol(a.col, b.col);
      if (mode == 3) { // Orthogonal: L-shaped path
        segment(c.ov, a.px, a.py, b.px, a.py, lw, col, al);
        segment(c.ov, b.px, a.py, b.px, b.py, lw, col, al);
      } else segment(c.ov, a.px, a.py, b.px, b.py, lw, col, al);
    }
  }
  if (mode == 2) { // Radial: spokes from the subject focus to the strongest points
    std::vector<const Pt*> s;
    for (auto& q : c.pts) s.push_back(&q);
    std::sort(s.begin(), s.end(), [](auto a, auto b) { return a->score > b->score; });
    size_t n = std::min(s.size(), size_t(std::max(4.f, s.size() * 0.12f * conn)));
    for (size_t k = 0; k < n; ++k)
      segment(c.ov, c.F.x, c.F.y, s[k]->px, s[k]->py, lw * 0.8f, s[k]->col, baseA * 0.55f * s[k]->fa);
  }
}

// ---------- nodes ----------
void primitive(Image& im, int mode, float x, float y, float s, float w, const Col& col, float a) {
  float h = s / 2;
  switch (mode) {
    case 0: { float k = s * 0.28f;
      segment(im, x - h, y - h + k, x - h, y - h, w, col, a); segment(im, x - h, y - h, x - h + k, y - h, w, col, a);
      segment(im, x + h - k, y - h, x + h, y - h, w, col, a); segment(im, x + h, y - h, x + h, y - h + k, w, col, a);
      segment(im, x + h, y + h - k, x + h, y + h, w, col, a); segment(im, x + h, y + h, x + h - k, y + h, w, col, a);
      segment(im, x - h + k, y + h, x - h, y + h, w, col, a); segment(im, x - h, y + h, x - h, y + h - k, w, col, a);
      break; }
    case 2: ring(im, x, y, h, w, col, a); break;
    case 3: segment(im, x - h, y, x + h, y, w, col, a); segment(im, x, y - h, x, y + h, w, col, a); break;
    case 4: segment(im, x, y - h, x + h, y, w, col, a); segment(im, x + h, y, x, y + h, w, col, a);
            segment(im, x, y + h, x - h, y, w, col, a); segment(im, x - h, y, x, y - h, w, col, a); break;
    default: segment(im, x - h, y - h, x + h, y - h, w, col, a); segment(im, x + h, y - h, x + h, y + h, w, col, a);
             segment(im, x + h, y + h, x - h, y + h, w, col, a); segment(im, x - h, y + h, x - h, y - h, w, col, a);
  }
}

void drawNodes(Ctx& c) {
  const Params& p = c.p;
  if (p.pointsEnable < 0.5) return;
  float pr = float(p.pointRadius) * c.unit * (1 + c.E.bass * 0.35f);
  if (pr > 0)
    for (auto& q : c.pts) if (q.fa >= 0.03f) disc(c.ov, q.px, q.py, pr * q.pulse, q.col, q.fa * 0.95f);
  if (p.nodeShapes < 0.5 && p.circles < 0.5 && p.labels < 0.5 && p.network < 0.5) return;
  std::vector<Pt*> ranked;
  for (auto& q : c.pts) ranked.push_back(&q);
  std::sort(ranked.begin(), ranked.end(), [](Pt* a, Pt* b) { return a->score > b->score || (a->score == b->score && a->id < b->id); });
  if (ranked.empty()) return;
  float fs = float(p.featureSizing / 100), hi = float(p.sizeHierarchy / 100), sv = float(p.sizeVariation / 100);
  float str = float(p.objectStrength / 100);
  int n = std::min<int>(int(ranked.size()), std::max(1, int(std::lround(ranked.size() * lerpf(0.06f, 0.3f, fs)))));
  float smax = float(std::max(p.shapeMax, p.shapeMin));
  for (int k = 0; k < n; ++k) {
    Pt* q = ranked[k];
    float rank = float(k) / n, base = fs * q->m + (1 - fs) * 0.5f, hier = std::pow(1 - rank, 1 + hi * 3);
    float mix = base * (1 - hi) + hier * hi + (q->rnd - 0.5f) * sv;
    float sz = (float(p.shapeMin) + (smax - float(p.shapeMin)) * clampf(mix)) * c.unit;
    if (p.subjectEnable >= 0.5 && p.objectFocus == 0) sz *= 1 + q->fw * str * 0.35f;
    q->size = sz * (1 + c.E.bass * 0.35f) * q->pulse;
  }
  float sw = float(p.shapeStroke) * c.unit;
  int prim = int(p.primitive);
  if (p.nodeShapes >= 0.5)
    for (int k = 0; k < n; ++k) {
      Pt* q = ranked[k];
      if (q->fa < 0.03f) continue;
      int m = prim;
      if (prim == 5) { // Mixed: pick by edge direction and a stable random
        float ang = std::atan2(q->gy, q->gx);
        int bucket = int(std::floor((ang + kPi) / kTau * 4)) & 3;
        m = q->rnd < 0.2f ? 2 : (bucket == 0 ? 0 : bucket == 1 ? 1 : bucket == 2 ? 4 : 3);
      }
      primitive(c.ov, m, q->px, q->py, q->size, sw, q->col, q->fa);
    }
  if (p.circles >= 0.5)
    for (int k = 0; k < (n + 3) / 4; ++k) ring(c.ov, ranked[k]->px, ranked[k]->py, ranked[k]->size * 1.6f, sw, ranked[k]->col, ranked[k]->fa * 0.5f);
  if (p.network >= 0.5) {
    int hub = std::min(n, 40);
    float lim = std::min(c.W, c.H) * 0.28f, lw = std::max(0.5f, float(p.lineWeight) * 0.6f) * c.unit;
    for (int i = 0; i < hub; ++i)
      for (int j = i + 1; j < hub; ++j) {
        Pt *a = ranked[i], *b = ranked[j];
        if (std::hypot(a->x - b->x, a->y - b->y) > lim) continue;
        segment(c.ov, a->px, a->py, b->px, b->py, lw, a->col, 0.16f * std::min(a->fa, b->fa));
      }
  }
  if (p.labels >= 0.5) {
    float s = std::max(1.f, 1.3f * c.unit);
    for (int k = 0; k < std::min(n, 14); ++k) {
      Pt* q = ranked[k];
      std::string t = std::to_string(int(q->x)) + "." + std::to_string(int(q->y));
      text(c.ov, q->px + q->size / 2 + 4 * c.unit, q->py - 3.5f * s, t, s, q->col, 0.85f * q->fa);
    }
  }
}

// ---------- contour field ----------
std::vector<float> blurred(const Analysis& A, int r) {
  std::vector<float> t1(A.lum.size()), t2(A.lum.size());
  if (r <= 0) { for (size_t i = 0; i < t2.size(); ++i) t2[i] = A.lum[i] / 255; return t2; }
  for (int y = 0; y < A.h; ++y)
    for (int x = 0; x < A.w; ++x) {
      float s = 0; int n = 0;
      for (int k = -r; k <= r; ++k) { int xx = std::clamp(x + k, 0, A.w - 1); s += A.lum[y * A.w + xx]; ++n; }
      t1[y * A.w + x] = s / n / 255;
    }
  for (int y = 0; y < A.h; ++y)
    for (int x = 0; x < A.w; ++x) {
      float s = 0; int n = 0;
      for (int k = -r; k <= r; ++k) { int yy = std::clamp(y + k, 0, A.h - 1); s += t1[yy * A.w + x]; ++n; }
      t2[y * A.w + x] = s / n;
    }
  return t2;
}

void drawContours(Ctx& c) {
  const Params& p = c.p;
  const Analysis& A = c.A;
  const auto& P = palette(int(p.palette));
  auto L = blurred(A, int(std::lround(lerpf(4, 0, float(p.contourDetail / 100)))));
  float alpha = float(p.contourOpacity / 100), lw = std::max(0.6f, float(p.lineWeight) * 0.8f) * c.unit;
  float drift = float(std::fmod(c.info.seconds * p.contourDrift * 0.01, 1.0));
  int levels = int(p.contourLevels);
  if (int(p.contourStyle) == 0) { // Topographic: marching squares
    for (int lv = 1; lv <= levels; ++lv) {
      float iso = std::fmod(float(lv) / (levels + 1) + drift / (levels + 1), 1.f);
      const Col& col = P[lv % P.size()];
      for (int y = 0; y < A.h - 1; ++y)
        for (int x = 0; x < A.w - 1; ++x) {
          float v[4] = {L[y * A.w + x], L[y * A.w + x + 1], L[(y + 1) * A.w + x + 1], L[(y + 1) * A.w + x]};
          int code = (v[0] > iso) | (v[1] > iso) << 1 | (v[2] > iso) << 2 | (v[3] > iso) << 3;
          if (code == 0 || code == 15) continue;
          // corners: 0 TL, 1 TR, 2 BR, 3 BL; edges e between corner e and e+1
          float ex[4], ey[4];
          const float cxs[4] = {0, 1, 1, 0}, cys[4] = {0, 0, 1, 1};
          bool has[4];
          for (int e = 0; e < 4; ++e) {
            int a = e, b = (e + 1) & 3;
            has[e] = (v[a] > iso) != (v[b] > iso);
            if (has[e]) {
              float t = (iso - v[a]) / (v[b] - v[a]);
              ex[e] = (x + cxs[a] + (cxs[b] - cxs[a]) * t + 0.5f) * c.kx;
              ey[e] = (y + cys[a] + (cys[b] - cys[a]) * t + 0.5f) * c.ky;
            }
          }
          int es[4], ne = 0;
          for (int e = 0; e < 4; ++e) if (has[e]) es[ne++] = e;
          if (ne == 2) segment(c.ov, ex[es[0]], ey[es[0]], ex[es[1]], ey[es[1]], lw, col, alpha);
          else if (ne == 4) {
            segment(c.ov, ex[0], ey[0], ex[1], ey[1], lw, col, alpha);
            segment(c.ov, ex[2], ey[2], ex[3], ey[3], lw, col, alpha);
          }
        }
    }
  } else { // Flow: streamlines along the isophotes
    float spacing = lerpf(12, 4, float(p.contourDetail / 100));
    std::vector<uint8_t> occ(size_t((A.w / 2 + 1) * (A.h / 2 + 1)), 0);
    auto occIdx = [&](float x, float y) { return size_t(int(y) / 2 * (A.w / 2 + 1) + int(x) / 2); };
    auto sample = [&](float x, float y) {
      int xi = std::clamp(int(x), 0, A.w - 1), yi = std::clamp(int(y), 0, A.h - 1);
      return L[yi * A.w + xi];
    };
    int maxLines = levels * 90, lines = 0, sid = 0;
    float off = drift * spacing;
    std::vector<std::pair<float, float>> path, back;
    for (float sy = 3 + off; sy < A.h - 3 && lines < maxLines; sy += spacing)
      for (float sx = 3 + off; sx < A.w - 3 && lines < maxLines; sx += spacing) {
        float x0 = sx + (hash01(sid, 1, c.seed) - 0.5f) * spacing * 0.6f;
        float y0 = sy + (hash01(sid++, 2, c.seed) - 0.5f) * spacing * 0.6f;
        if (occ[occIdx(x0, y0)]) continue;
        path.clear(); back.clear();
        std::vector<size_t> touched;
        for (int dir = -1; dir <= 1; dir += 2) {
          auto& out = dir < 0 ? back : path;
          float x = x0, y = y0, px = 0, py = 0;
          for (int s = 0; s < 60; ++s) {
            if (x < 2 || y < 2 || x > A.w - 3 || y > A.h - 3) break;
            size_t oi = occIdx(x, y);
            if (occ[oi] && s > 0) break;
            float gx = sample(x + 1.5f, y) - sample(x - 1.5f, y), gy = sample(x, y + 1.5f) - sample(x, y - 1.5f);
            float g = std::sqrt(gx * gx + gy * gy);
            if (g < 0.008f) break;
            float tx = -gy / g * dir, ty = gx / g * dir;
            if (s > 0 && tx * px + ty * py < 0) { tx = -tx; ty = -ty; }
            if (s > 0) { tx = px * 0.35f + tx * 0.65f; ty = py * 0.35f + ty * 0.65f; }
            out.push_back({x, y});
            touched.push_back(oi);
            px = tx; py = ty;
            x += tx * 1.2f; y += ty * 1.2f;
          }
        }
        std::reverse(back.begin(), back.end());
        if (!path.empty()) back.insert(back.end(), path.begin() + 1, path.end());
        if (back.size() < 6) continue;
        for (auto oi : touched) occ[oi] = 1;
        ++lines;
        float tone = sample(x0, y0);
        const Col& col = P[std::min<size_t>(P.size() - 1, size_t(tone * P.size()))];
        for (size_t k = 1; k < back.size(); ++k)
          segment(c.ov, (back[k - 1].first + 0.5f) * c.kx, (back[k - 1].second + 0.5f) * c.ky,
                  (back[k].first + 0.5f) * c.kx, (back[k].second + 0.5f) * c.ky, lw, col, alpha);
      }
  }
}

void drawGrid(Ctx& c) {
  int n = std::max(2, int(c.p.gridModules));
  float cell = float(c.W) / n, a = float(c.p.gridOpacity / 100);
  Col w{1, 1, 1};
  for (float x = 0; x <= c.W + 0.5f; x += cell) segment(c.ov, x, 0, x, float(c.H), c.unit, w, a);
  for (float y = 0; y <= c.H + 0.5f; y += cell) segment(c.ov, 0, y, float(c.W), y, c.unit, w, a);
}

// ---------- dither ----------
void drawDither(Ctx& c) {
  const Params& p = c.p;
  float cell = std::max(2.f, float(p.ditherCellSize) * c.unit);
  int gw = int(std::ceil(c.W / cell)), gh = int(std::ceil(c.H / cell));
  float jx = 0, jy = 0;
  if (p.ditherMotion > 0) {
    int64_t tick = int64_t(c.info.seconds * 12);
    jx = (hash01(tick, 3, c.seed) - 0.5f) * cell * float(p.ditherMotion / 100);
    jy = (hash01(tick, 4, c.seed) - 0.5f) * cell * float(p.ditherMotion / 100);
  }
  std::vector<float> L(size_t(gw) * gh);
  float ct = 1 + (float(p.ditherContrast) - 50) / 50 * 1.5f, off = (50 - float(p.ditherThreshold)) * 2.55f;
  for (int y = 0; y < gh; ++y)
    for (int x = 0; x < gw; ++x) {
      float s = 0; int n = 0;
      for (int k = 0; k < 4; ++k) {
        int sx = std::clamp(int((x + 0.25f + 0.5f * (k & 1)) * cell + jx), 0, c.W - 1);
        int sy = std::clamp(int((y + 0.25f + 0.5f * (k >> 1)) * cell + jy), 0, c.H - 1);
        const float* q = c.img.px(sx, sy);
        s += lumOf(clampf(q[0]), clampf(q[1]), clampf(q[2])); ++n;
      }
      L[size_t(y) * gw + x] = (s / n * 255 - 128) * ct + 128 + off;
    }
  static const int bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  int algo = int(p.ditherAlgorithm);
  std::vector<uint8_t> on(L.size());
  for (int y = 0; y < gh; ++y)
    for (int x = 0; x < gw; ++x) {
      size_t i = size_t(y) * gw + x;
      float v = L[i];
      if (algo == 2) { on[i] = v > (bayer[(y & 3) * 4 + (x & 3)] + 0.5f) * 16; continue; }
      bool o = v > 128;
      on[i] = o;
      float e = v - (o ? 255 : 0);
      auto add = [&](int dx, int dy, float w) {
        int xx = x + dx, yy = y + dy;
        if (xx >= 0 && xx < gw && yy < gh) L[size_t(yy) * gw + xx] += e * w;
      };
      if (algo == 0) { add(1, 0, 0.125f); add(2, 0, 0.125f); add(-1, 1, 0.125f); add(0, 1, 0.125f); add(1, 1, 0.125f); add(0, 2, 0.125f); }
      else { add(1, 0, 7 / 16.f); add(-1, 1, 3 / 16.f); add(0, 1, 5 / 16.f); add(1, 1, 1 / 16.f); }
    }
  const auto& P = palette(int(p.ditherPalette));
  Col dark = P.front(), light = P.back();
  if (p.ditherInvert >= 0.5) std::swap(dark, light);
  int mode = int(p.ditherBlend);
  float a = float(p.ditherOpacity / 100);
  parallelRows(c.H, [&](int y0, int y1) {
    for (int y = y0; y < y1; ++y) {
      int gy = std::min(gh - 1, int(y / cell));
      for (int x = 0; x < c.W; ++x) {
        int gx = std::min(gw - 1, int(x / cell));
        const Col& col = on[size_t(gy) * gw + gx] ? light : dark;
        blendPixel(c.img.px(x, y), col.r, col.g, col.b, a, mode);
      }
    }
  });
}

// ---------- pixel memory (temporal feedback) ----------
struct Rect { int x, y, w, h; };

struct Memory {
  int w = 0, h = 0;
  float cell = 1;
  std::vector<float> buf, nxt, row;
};

std::vector<Rect> memoryWindows(const Params& p, const Memory& M, double sec, int seed) {
  std::vector<Rect> out;
  int n = int(p.pixelMemoryWindows);
  if (n <= 0) { out.push_back({0, 0, M.w, M.h}); return out; }
  int64_t ep = int64_t(std::floor(sec / lerpf(9.f, 0.9f, float(p.motionSpeed / 100))));
  for (int k = 0; k < n; ++k) {
    int w = std::max(2, int(lerpf(0.12f, 0.42f, hash01(ep * 31 + k, 1, seed)) * M.w));
    int h = std::max(2, int(lerpf(0.08f, 0.35f, hash01(ep * 31 + k, 2, seed)) * M.h));
    out.push_back({int(hash01(ep * 31 + k, 3, seed) * std::max(1, M.w - w)), int(hash01(ep * 31 + k, 4, seed) * std::max(1, M.h - h)), w, h});
  }
  return out;
}

void memoryStep(Memory& M, const Analysis& A, int frame, double sec, const Params& p, const Energy& E, int seed) {
  int w = M.w, h = M.h, style = int(p.pixelMemoryStyle);
  float fb = float(p.pixelMemoryFeedback / 100), ent = float(p.pixelMemoryEntropy / 100);
  int sy = 0, sx = 0;
  if (style == 2) { // Archive: horizontal smear in a direction that changes per window epoch
    int64_t ep = int64_t(std::floor(sec / lerpf(9.f, 0.9f, float(p.motionSpeed / 100))));
    sx = hash01(frame, 12, seed) < ent * 0.8f ? (hash01(ep, 5, seed) < 0.5f ? -1 : 1) : 0;
  } else {
    sy = hash01(frame, 11, seed) < ent * 0.6f ? 1 : 0;
    sx = hash01(frame, 12, seed) < ent * 0.25f ? (hash01(frame, 13, seed) < 0.5f ? -1 : 1) : 0;
  }
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      size_t o = (size_t(y) * w + x) * 3;
      int X = x - sx, Y = y - sy;
      if (X >= 0 && X < w && Y >= 0 && Y < h) {
        size_t q = (size_t(Y) * w + X) * 3;
        M.nxt[o] = M.buf[q] * fb; M.nxt[o + 1] = M.buf[q + 1] * fb; M.nxt[o + 2] = M.buf[q + 2] * fb;
      } else M.nxt[o] = M.nxt[o + 1] = M.nxt[o + 2] = 0;
    }
  if (hash01(frame, 14, seed) < ent * 0.35f) { // row tearing
    int y0 = int(hash01(frame, 15, seed) * h), hh = 1 + int(hash01(frame, 16, seed) * h * 0.08f);
    int k = int((hash01(frame, 17, seed) - 0.5f) * w * 0.2f);
    for (int y = y0; y < std::min(h, y0 + hh); ++y) {
      std::memcpy(M.row.data(), &M.nxt[size_t(y) * w * 3], sizeof(float) * w * 3);
      for (int x = 0; x < w; ++x) {
        int X = ((x - k) % w + w) % w;
        for (int ch = 0; ch < 3; ++ch) M.nxt[(size_t(y) * w + x) * 3 + ch] = M.row[size_t(X) * 3 + ch];
      }
    }
  }
  float thr = float(p.pixelMemoryThreshold) * 2.55f * (1 - E.bass * 0.25f);
  const auto& P = palette(int(p.pixelMemoryPalette));
  for (const Rect& r : memoryWindows(p, M, sec, seed))
    for (int y = r.y; y < std::min(h, r.y + r.h); ++y)
      for (int x = r.x; x < std::min(w, r.x + r.w); ++x) {
        int ax = std::min(A.w - 1, int((x + 0.5f) / w * A.w)), ay = std::min(A.h - 1, int((y + 0.5f) / h * A.h));
        float lv = A.lum[ay * A.w + ax];
        if (lv <= thr) continue;
        Col cc;
        if (style == 2) { float g = lv / 255; cc = {g, g, g}; }
        else cc = P[std::min<size_t>(P.size() - 1, size_t((lv - thr) / (256 - thr) * P.size()))];
        size_t o = (size_t(y) * w + x) * 3;
        M.nxt[o] = std::max(M.nxt[o], cc.r); M.nxt[o + 1] = std::max(M.nxt[o + 1], cc.g); M.nxt[o + 2] = std::max(M.nxt[o + 2], cc.b);
      }
  std::swap(M.buf, M.nxt);
}

void drawMemory(Ctx& c, const std::vector<HistoryFrame>& history) {
  const Params& p = c.p;
  Memory M;
  M.cell = std::max(2.f, float(p.pixelMemoryCellSize) * c.unit);
  M.w = int(std::ceil(c.W / M.cell)); M.h = int(std::ceil(c.H / M.cell));
  M.buf.assign(size_t(M.w) * M.h * 3, 0.f); M.nxt = M.buf; M.row.assign(size_t(M.w) * 3, 0.f);
  // Replays the warm-up frames, then the current one: same result whatever order frames are rendered in.
  int warm = int(p.pixelMemoryWarmup);
  int start = std::max(0, int(history.size()) - warm);
  for (size_t i = size_t(start); i < history.size(); ++i)
    memoryStep(M, *history[i].analysis, history[i].frame, history[i].seconds, p, rhythm(p, history[i].seconds), c.seed);
  memoryStep(M, c.A, c.info.frame, c.info.seconds, p, c.E, c.seed);

  auto wins = memoryWindows(p, M, c.info.seconds, c.seed);
  std::vector<uint8_t> vis(size_t(M.w) * M.h, 0);
  for (auto& r : wins)
    for (int y = r.y; y < std::min(M.h, r.y + r.h); ++y)
      for (int x = r.x; x < std::min(M.w, r.x + r.w); ++x) vis[size_t(y) * M.w + x] = 1;
  float a = float(p.pixelMemoryOpacity / 100);
  bool glyphs = int(p.pixelMemoryStyle) == 1;
  parallelRows(c.H, [&](int y0, int y1) {
    for (int y = y0; y < y1; ++y) {
      int cy = std::min(M.h - 1, int(y / M.cell));
      float ly = (y - cy * M.cell) / M.cell;
      for (int x = 0; x < c.W; ++x) {
        int cx = std::min(M.w - 1, int(x / M.cell));
        size_t ci = size_t(cy) * M.w + cx;
        if (!vis[ci]) continue;
        const float* v = &M.buf[ci * 3];
        if (v[0] + v[1] + v[2] < 0.01f) continue;
        if (glyphs) {
          float lx = (x - cx * M.cell) / M.cell;
          bool mark = (hash01(int64_t(ci), 9, c.seed) < 0.5f) ? (std::fabs(lx - 0.5f) < 0.13f || std::fabs(ly - 0.5f) < 0.13f)
                                                               : (std::fabs(ly - 0.5f) < 0.16f && lx > 0.15f && lx < 0.85f);
          if (!mark) continue;
        }
        blendPixel(c.img.px(x, y), v[0], v[1], v[2], a, 1);
      }
    }
  });
  const auto& P = palette(int(p.pixelMemoryPalette));
  if (p.pixelMemoryWindows > 0)
    for (auto& r : wins) {
      float x0 = r.x * M.cell, y0 = r.y * M.cell, x1 = (r.x + r.w) * M.cell, y1 = (r.y + r.h) * M.cell;
      Col col = P.back();
      segment(c.img, x0, y0, x1, y0, c.unit, col, 0.35f); segment(c.img, x1, y0, x1, y1, c.unit, col, 0.35f);
      segment(c.img, x1, y1, x0, y1, c.unit, col, 0.35f); segment(c.img, x0, y1, x0, y0, c.unit, col, 0.35f);
    }
}

// ---------- post ----------
void glitchSlices(Ctx& c) {
  const Params& p = c.p;
  if (p.motionEnable < 0.5 || int(p.motionMode) != 1) return;
  float mo = float(p.motionAmount / 100), sp = float(p.motionSpeed / 100);
  int64_t tick = int64_t(std::floor(c.info.seconds / lerpf(0.42f, 0.045f, sp)));
  if (hash01(tick, 21, c.seed) > mo * 0.5f + c.E.high * 0.5f) return;
  int n = 1 + int(hash01(tick, 22, c.seed) * 4);
  std::vector<float> row(size_t(c.W) * 4);
  for (int k = 0; k < n; ++k) {
    int y0 = int(hash01(tick, 30 + k, c.seed) * c.H);
    int hh = std::max(1, int((2 + hash01(tick, 40 + k, c.seed) * 42) * c.unit));
    int dx = int((hash01(tick, 50 + k, c.seed) - 0.5f) * 2 * mo * 90 * c.unit);
    for (int y = y0; y < std::min(c.H, y0 + hh); ++y) {
      std::memcpy(row.data(), c.img.px(0, y), sizeof(float) * 4 * c.W);
      for (int x = 0; x < c.W; ++x) {
        int sx = x - dx;
        if (sx < 0 || sx >= c.W) continue;
        std::memcpy(c.img.px(x, y), &row[size_t(sx) * 4], sizeof(float) * 4);
      }
    }
  }
  if (hash01(tick, 23, c.seed) < mo * 0.4f) { // faint horizontal ghost
    int dx = int((hash01(tick, 24, c.seed) - 0.5f) * 8 * c.unit);
    if (dx != 0)
      parallelRows(c.H, [&](int y0, int y1) {
        std::vector<float> r(size_t(c.W) * 4);
        for (int y = y0; y < y1; ++y) {
          std::memcpy(r.data(), c.img.px(0, y), sizeof(float) * 4 * c.W);
          for (int x = 0; x < c.W; ++x) {
            int sx = x - dx;
            if (sx < 0 || sx >= c.W) continue;
            const float* s = &r[size_t(sx) * 4];
            blendPixel(c.img.px(x, y), clampf(s[0]), clampf(s[1]), clampf(s[2]), 0.25f, 1);
          }
        }
      });
  }
}

void textures(Ctx& c) {
  const Params& p = c.p;
  if (p.textureEnable < 0.5) return;
  float scan = float(p.scanlines / 100) * 0.7f, grain = float(p.grain / 100) * 0.6f;
  if (scan <= 0 && grain <= 0) return;
  int period = std::max(2, int(std::lround(3 * c.unit))), dark = std::max(1, period / 3);
  parallelRows(c.H, [&](int y0, int y1) {
    for (int y = y0; y < y1; ++y) {
      bool line = scan > 0 && (y % period) < dark;
      for (int x = 0; x < c.W; ++x) {
        float* q = c.img.px(x, y);
        if (line) { q[0] *= 1 - scan; q[1] *= 1 - scan; q[2] *= 1 - scan; }
        if (grain > 0) {
          float n = hash01(int64_t(y) * 8192 + x, c.info.frame, c.seed);
          blendPixel(q, n, n, n, grain, 3);
        }
      }
    }
  });
}

void drawFrame(Ctx& c) {
  if (c.p.artworkFrame < 0.5) return;
  int m = int(std::lround(0.035f * std::min(c.W, c.H)));
  float tk = m * 0.5f;
  for (int y = 0; y < c.H; ++y)
    for (int x = 0; x < c.W; ++x)
      if (x < m || y < m || x >= c.W - m || y >= c.H - m) { float* q = c.img.px(x, y); q[0] = q[1] = q[2] = 0; }
      else if (x == m) x = c.W - m - 1; // skip the inside
  Col w{1, 1, 1};
  float lw = std::max(1.f, 0.8f * c.unit), a = 0.55f;
  float L = float(m), R = float(c.W - m), T = float(m), B = float(c.H - m);
  segment(c.img, L, T, R, T, lw, w, a); segment(c.img, R, T, R, B, lw, w, a);
  segment(c.img, R, B, L, B, lw, w, a); segment(c.img, L, B, L, T, lw, w, a);
  segment(c.img, L, T - tk, L, T, lw, w, a); segment(c.img, L - tk, T, L, T, lw, w, a);
  segment(c.img, R, T - tk, R, T, lw, w, a); segment(c.img, R + tk, T, R, T, lw, w, a);
  segment(c.img, L, B + tk, L, B, lw, w, a); segment(c.img, L - tk, B, L, B, lw, w, a);
  segment(c.img, R, B + tk, R, B, lw, w, a); segment(c.img, R + tk, B, R, B, lw, w, a);
  float cx = c.W / 2.f, cy = c.H / 2.f, t6 = tk * 0.6f;
  segment(c.img, cx, T - t6, cx, T, lw, w, a); segment(c.img, cx, B, cx, B + t6, lw, w, a);
  segment(c.img, L - t6, cy, L, cy, lw, w, a); segment(c.img, R, cy, R + t6, cy, lw, w, a);
  if (c.p.labels >= 0.5) {
    float s = std::max(1.f, m * 0.32f / 7);
    float ty = c.H - m / 2.f - 3.5f * s;
    text(c.img, L, ty, c.info.title, s, w, 0.7f);
    char buf[64];
    std::snprintf(buf, sizeof buf, "SEED %d * %.2fS", c.seed, c.info.seconds);
    std::string r = buf;
    text(c.img, R - textWidth(r, s), ty, r, s, w, 0.7f);
  }
}

} // namespace

// ---------- public API ----------
Analysis analyze(const Image& src, int targetW) {
  Analysis A;
  A.w = std::max(8, std::min(targetW, src.w));
  A.h = std::max(8, int(std::lround(double(A.w) * src.h / std::max(1, src.w))));
  size_t n = size_t(A.w) * A.h;
  A.lum.assign(n, 0); A.r.assign(n, 0); A.g.assign(n, 0); A.b.assign(n, 0);
  A.mag.assign(n, 0); A.gx.assign(n, 0); A.gy.assign(n, 0);
  for (int y = 0; y < A.h; ++y) {
    int sy0 = int(double(y) * src.h / A.h), sy1 = std::max(sy0 + 1, int(double(y + 1) * src.h / A.h));
    for (int x = 0; x < A.w; ++x) {
      int sx0 = int(double(x) * src.w / A.w), sx1 = std::max(sx0 + 1, int(double(x + 1) * src.w / A.w));
      int stepx = std::max(1, (sx1 - sx0) / 4), stepy = std::max(1, (sy1 - sy0) / 4);
      float r = 0, g = 0, b = 0; int k = 0;
      for (int yy = sy0; yy < std::min(sy1, src.h); yy += stepy)
        for (int xx = sx0; xx < std::min(sx1, src.w); xx += stepx) {
          const float* q = src.px(xx, yy);
          r += clampf(q[0]); g += clampf(q[1]); b += clampf(q[2]); ++k;
        }
      k = std::max(k, 1);
      size_t i = size_t(y) * A.w + x;
      A.r[i] = r / k * 255; A.g[i] = g / k * 255; A.b[i] = b / k * 255;
      A.lum[i] = 0.299f * A.r[i] + 0.587f * A.g[i] + 0.114f * A.b[i];
    }
  }
  double sum = 0;
  float mx = 0;
  for (int y = 1; y < A.h - 1; ++y)
    for (int x = 1; x < A.w - 1; ++x) {
      int i = y * A.w + x;
      const auto& l = A.lum;
      float gx = (l[i - A.w + 1] + 2 * l[i + 1] + l[i + A.w + 1]) - (l[i - A.w - 1] + 2 * l[i - 1] + l[i + A.w - 1]);
      float gy = (l[i + A.w - 1] + 2 * l[i + A.w] + l[i + A.w + 1]) - (l[i - A.w - 1] + 2 * l[i - A.w] + l[i - A.w + 1]);
      float m = std::sqrt(gx * gx + gy * gy);
      A.gx[i] = gx; A.gy[i] = gy; A.mag[i] = m;
      sum += m; mx = std::max(mx, m);
    }
  A.maxMag = mx > 0 ? mx : 1;
  A.meanMag = float(sum / n);
  return A;
}

void render(Image& img, const Analysis& current, const std::vector<HistoryFrame>& history, const Params& p, const RenderInfo& info) {
  if (img.w < 8 || img.h < 8) return;
  Ctx c(img, current, p, info);
  c.E = rhythm(p, info.seconds);

  // Source over black at the chosen opacity
  float base = float(p.baseOpacity / 100);
  if (base < 1)
    parallelRows(c.H, [&](int y0, int y1) {
      for (int y = y0; y < y1; ++y)
        for (int x = 0; x < c.W; ++x) { float* q = img.px(x, y); q[0] *= base; q[1] *= base; q[2] *= base; }
    });

  if (p.ditherPrint >= 0.5) drawDither(c);

  buildPoints(c);
  computeFocus(c);
  preparePoints(c);

  c.ov.resize(c.W, c.H);
  if (p.contourField >= 0.5) drawContours(c);
  if (p.grid >= 0.5) drawGrid(c);
  drawConnections(c);
  drawNodes(c);
  composite(img, c.ov, int(p.blendMode), float(p.overlayOpacity / 100));
  c.ov = Image(); // free early

  if (p.pixelMemory >= 0.5) drawMemory(c, history);
  glitchSlices(c);
  textures(c);
  drawFrame(c);
}

} // namespace ak
