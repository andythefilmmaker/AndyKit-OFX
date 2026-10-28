// Test host clip implementation. Derived from the OpenFX HostSupport demo.
// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause
#include <algorithm>
#include <cmath>
#include <cstring>

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

namespace MyHost {

  MyImage::MyImage(MyClipInstance& clip, OfxTime time, bool output)
    : OFX::Host::ImageEffect::Image(clip), _data(nullptr) {
    const SourceFrames& s = SourceFrames::get();
    size_t n = size_t(s.width) * s.height * 4;
    _data = new float[n];
    if (output) std::fill(_data, _data + n, 0.f);
    else {
      // the "video": the still drifts horizontally a little every frame
      int shift = int(std::lround(time * 2));
      for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x) {
          int sx = std::clamp(x - shift, 0, s.width - 1);
          std::memcpy(&_data[(size_t(y) * s.width + x) * 4], &s.rgba[(size_t(y) * s.width + sx) * 4], 4 * sizeof(float));
        }
    }
    setDoubleProperty(kOfxImageEffectPropRenderScale, 1.0, 0);
    setDoubleProperty(kOfxImageEffectPropRenderScale, 1.0, 1);
    setPointerProperty(kOfxImagePropData, _data);
    setIntProperty(kOfxImagePropBounds, 0, 0);
    setIntProperty(kOfxImagePropBounds, 0, 1);
    setIntProperty(kOfxImagePropBounds, s.width, 2);
    setIntProperty(kOfxImagePropBounds, s.height, 3);
    setIntProperty(kOfxImagePropRegionOfDefinition, 0, 0);
    setIntProperty(kOfxImagePropRegionOfDefinition, 0, 1);
    setIntProperty(kOfxImagePropRegionOfDefinition, s.width, 2);
    setIntProperty(kOfxImagePropRegionOfDefinition, s.height, 3);
    setIntProperty(kOfxImagePropRowBytes, int(s.width * 4 * sizeof(float)));
    setStringProperty(kOfxImageEffectPropPixelDepth, kOfxBitDepthFloat);
    setStringProperty(kOfxImageEffectPropComponents, kOfxImageComponentRGBA);
    setStringProperty(kOfxImageEffectPropPreMultiplication, kOfxImageUnPreMultiplied);
  }

  float* MyImage::pixel(int x, int y) const {
    const SourceFrames& s = SourceFrames::get();
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return nullptr;
    return _data + (size_t(y) * s.width + x) * 4;
  }

  MyImage::~MyImage() { delete[] _data; }

  MyClipInstance::MyClipInstance(MyEffectInstance* effect, OFX::Host::ImageEffect::ClipDescriptor* desc)
    : OFX::Host::ImageEffect::ClipInstance(effect, *desc), _effect(effect), _name(desc->getName()), _outputImage(nullptr) {}

  MyClipInstance::~MyClipInstance() { if (_outputImage) _outputImage->releaseReference(); }

  const std::string& MyClipInstance::getUnmappedBitDepth() const { static const std::string v(kOfxBitDepthFloat); return v; }
  const std::string& MyClipInstance::getUnmappedComponents() const { static const std::string v(kOfxImageComponentRGBA); return v; }
  const std::string& MyClipInstance::getPremult() const { static const std::string v(kOfxImageUnPreMultiplied); return v; }
  double MyClipInstance::getAspectRatio() const { return 1.0; }
  double MyClipInstance::getFrameRate() const { return SourceFrames::get().fps; }
  void MyClipInstance::getFrameRange(double& a, double& b) const { a = 0; b = SourceFrames::get().lastFrame; }
  const std::string& MyClipInstance::getFieldOrder() const { static const std::string v(kOfxImageFieldNone); return v; }
  bool MyClipInstance::getConnected() const { return true; }
  double MyClipInstance::getUnmappedFrameRate() const { return SourceFrames::get().fps; }
  void MyClipInstance::getUnmappedFrameRange(double& a, double& b) const { a = 0; b = SourceFrames::get().lastFrame; }
  bool MyClipInstance::getContinuousSamples() const { return false; }

  OfxRectD MyClipInstance::getRegionOfDefinition(OfxTime) const {
    OfxRectD v;
    v.x1 = v.y1 = 0;
    v.x2 = SourceFrames::get().width;
    v.y2 = SourceFrames::get().height;
    return v;
  }

  OFX::Host::ImageEffect::Image* MyClipInstance::getImage(OfxTime time, const OfxRectD*) {
    if (_name == "Output") {
      if (!_outputImage) _outputImage = new MyImage(*this, 0, true);
      _outputImage->addReference();
      return _outputImage;
    }
    ++fetches;
    return new MyImage(*this, time, false);
  }
}
