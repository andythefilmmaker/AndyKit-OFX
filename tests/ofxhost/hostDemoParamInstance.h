// Test host params: values start at the plugin's defaults and can be overridden by name.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef HOST_DEMO_PARAM_INSTANCE_H
#define HOST_DEMO_PARAM_INSTANCE_H

#include <map>
#include <string>

namespace MyHost {

  // name -> textual value, applied when the param instance is created
  inline std::map<std::string, std::string>& paramOverrides() { static std::map<std::string, std::string> m; return m; }

  class MyPushbuttonInstance : public OFX::Host::Param::PushbuttonInstance {
  public:
    MyPushbuttonInstance(MyEffectInstance* e, const std::string& name, OFX::Host::Param::Descriptor& d)
      : OFX::Host::Param::PushbuttonInstance(d, e) {}
  };

  class MyIntegerInstance : public OFX::Host::Param::IntegerInstance {
    int _v;
  public:
    MyIntegerInstance(MyEffectInstance* e, const std::string& name, OFX::Host::Param::Descriptor& d)
      : OFX::Host::Param::IntegerInstance(d, e), _v(d.getProperties().getIntProperty(kOfxParamPropDefault)) {
      auto it = paramOverrides().find(name); if (it != paramOverrides().end()) _v = std::stoi(it->second);
    }
    OfxStatus get(int& v) { v = _v; return kOfxStatOK; }
    OfxStatus get(OfxTime, int& v) { v = _v; return kOfxStatOK; }
    OfxStatus set(int v) { _v = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, int v) { _v = v; return kOfxStatOK; }
  };

  class MyChoiceInstance : public OFX::Host::Param::ChoiceInstance {
    int _v;
  public:
    MyChoiceInstance(MyEffectInstance* e, const std::string& name, OFX::Host::Param::Descriptor& d)
      : OFX::Host::Param::ChoiceInstance(d, e), _v(d.getProperties().getIntProperty(kOfxParamPropDefault)) {
      auto it = paramOverrides().find(name); if (it != paramOverrides().end()) _v = std::stoi(it->second);
    }
    OfxStatus get(int& v) { v = _v; return kOfxStatOK; }
    OfxStatus get(OfxTime, int& v) { v = _v; return kOfxStatOK; }
    OfxStatus set(int v) { _v = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, int v) { _v = v; return kOfxStatOK; }
  };

  class MyDoubleInstance : public OFX::Host::Param::DoubleInstance {
    double _v;
  public:
    MyDoubleInstance(MyEffectInstance* e, const std::string& name, OFX::Host::Param::Descriptor& d)
      : OFX::Host::Param::DoubleInstance(d, e), _v(d.getProperties().getDoubleProperty(kOfxParamPropDefault)) {
      auto it = paramOverrides().find(name); if (it != paramOverrides().end()) _v = std::stod(it->second);
    }
    OfxStatus get(double& v) { v = _v; return kOfxStatOK; }
    OfxStatus get(OfxTime, double& v) { v = _v; return kOfxStatOK; }
    OfxStatus set(double v) { _v = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, double v) { _v = v; return kOfxStatOK; }
    OfxStatus derive(OfxTime, double& v) { v = 0; return kOfxStatOK; }
    OfxStatus integrate(OfxTime a, OfxTime b, double& v) { v = _v * (b - a); return kOfxStatOK; }
  };

  class MyBooleanInstance : public OFX::Host::Param::BooleanInstance {
    bool _v;
  public:
    MyBooleanInstance(MyEffectInstance* e, const std::string& name, OFX::Host::Param::Descriptor& d)
      : OFX::Host::Param::BooleanInstance(d, e), _v(d.getProperties().getIntProperty(kOfxParamPropDefault) != 0) {
      auto it = paramOverrides().find(name); if (it != paramOverrides().end()) _v = it->second == "1" || it->second == "true";
    }
    OfxStatus get(bool& v) { v = _v; return kOfxStatOK; }
    OfxStatus get(OfxTime, bool& v) { v = _v; return kOfxStatOK; }
    OfxStatus set(bool v) { _v = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, bool v) { _v = v; return kOfxStatOK; }
  };

  class MyStringInstance : public OFX::Host::Param::StringInstance {
    std::string _v;
  public:
    MyStringInstance(MyEffectInstance* e, const std::string& name, OFX::Host::Param::Descriptor& d)
      : OFX::Host::Param::StringInstance(d, e), _v(d.getProperties().getStringProperty(kOfxParamPropDefault)) {
      auto it = paramOverrides().find(name); if (it != paramOverrides().end()) _v = it->second;
    }
    OfxStatus get(std::string& v) { v = _v; return kOfxStatOK; }
    OfxStatus get(OfxTime, std::string& v) { v = _v; return kOfxStatOK; }
    OfxStatus set(const char* v) { _v = v; return kOfxStatOK; }
    OfxStatus set(OfxTime, const char* v) { _v = v; return kOfxStatOK; }
  };
}

#endif
