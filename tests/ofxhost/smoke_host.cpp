// Minimal OpenFX host used to smoke-test the AndyKit bundle without an editing app.
//   smoke_host <plugin-dir> <input.png> <out-prefix> [--frames N] [--start S] [--preset index] [param=value ...]
// Derived from the OpenFX HostSupport demo.
// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxPixels.h"
#include "ofxhBinary.h"
#include "ofxhPropertySuite.h"
#include "ofxhClip.h"
#include "ofxhParam.h"
#include "ofxhMemory.h"
#include "ofxhImageEffect.h"
#include "ofxhPluginAPICache.h"
#include "ofxhPluginCache.h"
#include "ofxhHost.h"
#include "ofxhImageEffectAPI.h"

#include "hostDemoHostDescriptor.h"
#include "hostDemoEffectInstance.h"
#include "hostDemoClipInstance.h"
#include "hostDemoParamInstance.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static int fail(const std::string& msg) { std::cerr << "FAIL: " << msg << std::endl; return 1; }

int main(int argc, char** argv) {
  if (argc < 4) { std::cerr << "usage: smoke_host <plugin-dir> <input.png> <out-prefix> [--frames N] [--preset i] [k=v ...]\n"; return 2; }
  std::string pluginDir = argv[1], input = argv[2], prefix = argv[3];
  int frames = 3, preset = -1, start = 0;
  for (int i = 4; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
    else if (a == "--preset" && i + 1 < argc) preset = std::atoi(argv[++i]);
    else if (a == "--start" && i + 1 < argc) start = std::atoi(argv[++i]);
    else if (auto eq = a.find('='); eq != std::string::npos) MyHost::paramOverrides()[a.substr(0, eq)] = a.substr(eq + 1);
  }
  if (preset >= 0) MyHost::paramOverrides()["akPreset"] = std::to_string(preset);

  int w, h, n;
  unsigned char* data = stbi_load(input.c_str(), &w, &h, &n, 4);
  if (!data) return fail("cannot read " + input);
  auto& S = MyHost::SourceFrames::get();
  S.width = w; S.height = h; S.lastFrame = std::max(start + frames - 1, 1);
  S.rgba.resize(size_t(w) * h * 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w * 4; ++x) S.rgba[size_t(h - 1 - y) * w * 4 + x] = data[size_t(y) * w * 4 + x] / 255.f;
  stbi_image_free(data);

  OFX::Host::PluginCache::getPluginCache()->setCacheVersion("akSmokeV1");
  OFX::Host::PluginCache::getPluginCache()->addFileToPath(pluginDir);
  MyHost::Host host;
  OFX::Host::ImageEffect::PluginCache cache(host);
  cache.registerInCache(*OFX::Host::PluginCache::getPluginCache());
  OFX::Host::PluginCache::getPluginCache()->scanPluginFiles();

  OFX::Host::ImageEffect::ImageEffectPlugin* plugin = cache.getPluginById("com.andythefilmmaker.andykit");
  if (!plugin) return fail("plugin com.andythefilmmaker.andykit not found in " + pluginDir);
  std::cout << "loaded: " << plugin->getDescriptor().getLabel() << " v" << plugin->getVersionMajor() << "." << plugin->getVersionMinor() << std::endl;

  std::unique_ptr<OFX::Host::ImageEffect::Instance> inst(plugin->createInstance(kOfxImageEffectContextFilter, nullptr));
  if (!inst) return fail("createInstance");
  OfxStatus st = inst->createInstanceAction();
  if (st != kOfxStatOK && st != kOfxStatReplyDefault) return fail("createInstanceAction");
  if (!inst->getClipPreferences()) return fail("getClipPreferences");

  OfxPointD scale{1.0, 1.0};
  if (preset >= 0) { // press "Apply Preset" like a user would
    inst->beginInstanceChangedAction(kOfxChangeUserEdited);
    st = inst->paramInstanceChangedAction("akPresetApply", kOfxChangeUserEdited, 0, scale);
    inst->endInstanceChangedAction(kOfxChangeUserEdited);
    if (st != kOfxStatOK && st != kOfxStatReplyDefault) return fail("apply preset");
    std::cout << "preset " << preset << " applied" << std::endl;
  }

  OfxRectI rw{0, 0, w, h};
  OfxRectD roi{0, 0, double(w), double(h)};
  st = inst->beginRenderAction(start, start + frames - 1, 1.0, false, scale, true, false);
  if (st != kOfxStatOK && st != kOfxStatReplyDefault) return fail("beginRenderAction");
  auto* out = dynamic_cast<MyHost::MyClipInstance*>(inst->getClip("Output"));
  auto* src = dynamic_cast<MyHost::MyClipInstance*>(inst->getClip("Source"));
  if (!out || !src) return fail("clips");
  std::vector<unsigned char> buf(size_t(w) * h * 4);
  for (int t = start; t < start + frames; ++t) {
    std::map<OFX::Host::ImageEffect::ClipInstance*, OfxRectD> rois;
    inst->getRegionOfInterestAction(t, scale, roi, rois);
    OFX::Host::ImageEffect::RangeMap needed;
    inst->getFrameNeededAction(t, needed);
    int before = src->fetches;
    st = inst->renderAction(t, kOfxImageFieldBoth, rw, scale, true, false, false);
    if (st != kOfxStatOK) return fail("renderAction at frame " + std::to_string(t) + " status " + std::to_string(st));
    MyHost::MyImage* img = out->getOutputImage();
    double sum = 0;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const float* p = img->pixel(x, y);
        for (int c = 0; c < 4; ++c) {
          float v = std::min(1.f, std::max(0.f, p[c]));
          if (c < 3) sum += v;
          buf[(size_t(h - 1 - y) * w + x) * 4 + c] = (unsigned char)std::lround(v * 255);
        }
      }
    std::ostringstream name;
    name << prefix << "_" << t << ".png";
    stbi_write_png(name.str().c_str(), w, h, 4, buf.data(), w * 4);
    std::cout << "frame " << t << ": mean " << sum / (double(w) * h * 3) << ", source fetches " << (src->fetches - before)
              << ", frames needed " << (needed.empty() ? 0 : needed.begin()->second.size()) << " -> " << name.str() << std::endl;
    if (!(sum > 0)) return fail("empty output");
  }
  inst->endRenderAction(start, start + frames - 1, 1.0, false, scale, true, false);
  inst.reset();
  OFX::Host::PluginCache::clearPluginCache();
  std::cout << "OK" << std::endl;
  return 0;
}
