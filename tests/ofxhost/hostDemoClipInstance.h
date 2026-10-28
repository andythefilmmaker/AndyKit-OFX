// Test host clip: float RGBA images loaded from a still, slightly animated over time.
// Derived from the OpenFX HostSupport demo.
// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef HOST_DEMO_CLIP_INSTANCE_H
#define HOST_DEMO_CLIP_INSTANCE_H

#include <string>
#include <vector>

namespace MyHost {

  // Source frames shared by all clips of the test host.
  struct SourceFrames {
    int width = 0, height = 0;
    std::vector<float> rgba; // bottom-up rows, as OFX expects
    int lastFrame = 47;
    double fps = 25;
    static SourceFrames& get() { static SourceFrames s; return s; }
  };

  class MyClipInstance;

  class MyImage : public OFX::Host::ImageEffect::Image {
  protected:
    float* _data;
  public:
    explicit MyImage(MyClipInstance& clip, OfxTime t, bool output);
    float* pixel(int x, int y) const;
    ~MyImage();
  };

  class MyClipInstance : public OFX::Host::ImageEffect::ClipInstance {
  protected:
    MyEffectInstance* _effect;
    std::string _name;
    MyImage* _outputImage;
  public:
    int fetches = 0;
    MyClipInstance(MyEffectInstance* effect, OFX::Host::ImageEffect::ClipDescriptor* desc);
    virtual ~MyClipInstance();
    MyImage* getOutputImage() { return _outputImage; }
    const std::string& getUnmappedBitDepth() const;
    virtual const std::string& getUnmappedComponents() const;
    virtual const std::string& getPremult() const;
    virtual double getAspectRatio() const;
    virtual double getFrameRate() const;
    virtual void getFrameRange(double& startFrame, double& endFrame) const;
    virtual const std::string& getFieldOrder() const;
    virtual bool getConnected() const;
    virtual double getUnmappedFrameRate() const;
    virtual void getUnmappedFrameRange(double& unmappedStartFrame, double& unmappedEndFrame) const;
    virtual bool getContinuousSamples() const;
    virtual OFX::Host::ImageEffect::Image* getImage(OfxTime time, const OfxRectD* optionalBounds);
#ifdef OFX_SUPPORTS_OPENGLRENDER
    virtual OFX::Host::ImageEffect::Texture* loadTexture(OfxTime, const char*, const OfxRectD*) { return NULL; }
#endif
    virtual OfxRectD getRegionOfDefinition(OfxTime time) const;
  };
}

#endif
