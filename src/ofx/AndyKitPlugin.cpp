// AndyKit — OpenFX plugin wrapper around the host-independent engine.
// SPDX-License-Identifier: MIT
#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

#include "core/Engine.h"
#include "core/Presets.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define kPluginName        "AndyKit"
#define kPluginGrouping    "ANDYTHEFILMMAKER"
#define kPluginDescription "Turns edges into points, shapes and lines with temporal pixel memory, contours, dither and glitch. Open source."
#define kPluginIdentifier  "com.andythefilmmaker.andykit"
#ifndef AK_VERSION_MAJOR
#define AK_VERSION_MAJOR 0
#define AK_VERSION_MINOR 2
#endif

#define kParamPresetChoice "akPreset"
#define kParamPresetApply  "akPresetApply"
#define kParamPresetName   "akPresetName"
#define kParamPresetFile   "akPresetFile"
#define kParamPresetSave   "akPresetSave"
#define kParamPresetLoad   "akPresetLoad"
#define kParamTitle        "akTitle"

using namespace OFX;

namespace {

// Entries of the preset menu: built-in demos, then the .andykit files found in the presets
// folder when the plugin was loaded. Frozen once so menu indices stay stable for the session.
struct MenuEntry { std::string label; int builtin; std::string path; };
const std::vector<MenuEntry>& presetMenu() {
  static std::vector<MenuEntry> menu;
  static std::once_flag once;
  std::call_once(once, [] {
    const auto& b = ak::builtinPresets();
    for (size_t i = 0; i < b.size(); ++i) menu.push_back({b[i].name, int(i), ""});
    for (const auto& u : ak::scanPresetFolder(ak::defaultPresetFolder())) menu.push_back({"* " + u.name, -1, u.path});
  });
  return menu;
}

// Analyses of source frames, reused across renders (pixel memory replays earlier frames).
class AnalysisCache {
 public:
  std::shared_ptr<const ak::Analysis> get(double t, int w, int h) {
    std::lock_guard<std::mutex> lk(m_);
    auto it = map_.find(t);
    if (it != map_.end() && it->second.w == w && it->second.h == h) return it->second.a;
    return nullptr;
  }
  void put(double t, int w, int h, std::shared_ptr<const ak::Analysis> a) {
    std::lock_guard<std::mutex> lk(m_);
    if (map_.size() > 160) map_.clear();
    map_[t] = {w, h, std::move(a)};
  }
  void clear() { std::lock_guard<std::mutex> lk(m_); map_.clear(); }

 private:
  struct Entry { int w, h; std::shared_ptr<const ak::Analysis> a; };
  std::mutex m_;
  std::map<double, Entry> map_;
};

// Copies an OFX RGBA float image (bottom-up rows) into a top-down ak::Image.
bool toEngineImage(const Image& src, ak::Image& out) {
  if (src.getPixelDepth() != eBitDepthFloat || src.getPixelComponents() != ePixelComponentRGBA) return false;
  const OfxRectI& b = src.getBounds();
  int w = b.x2 - b.x1, h = b.y2 - b.y1;
  if (w <= 0 || h <= 0) return false;
  out.resize(w, h);
  for (int y = 0; y < h; ++y) {
    const float* row = static_cast<const float*>(src.getPixelAddress(b.x1, b.y2 - 1 - y));
    if (!row) continue;
    float* dst = out.px(0, y);
    for (int i = 0; i < w * 4; ++i) {
      float v = row[i];
      dst[i] = std::isfinite(v) ? v : 0.f;
    }
  }
  return true;
}

bool endsWith(const std::string& s, const std::string& e) { return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0; }

} // namespace

class AndyKitPlugin : public ImageEffect {
 public:
  explicit AndyKitPlugin(OfxImageEffectHandle handle) : ImageEffect(handle) {
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    srcClip_ = getContext() == eContextGenerator ? nullptr : fetchClip(kOfxImageEffectSimpleSourceClipName);
    presetChoice_ = fetchChoiceParam(kParamPresetChoice);
    presetName_ = fetchStringParam(kParamPresetName);
    presetFile_ = fetchStringParam(kParamPresetFile);
    title_ = fetchStringParam(kParamTitle);
    for (const auto& s : ak::paramSpecs()) {
      Param* p = nullptr;
      switch (s.type) {
        case ak::PType::Double: p = doubles_[s.id] = fetchDoubleParam(s.id); break;
        case ak::PType::Int:    p = ints_[s.id] = fetchIntParam(s.id); break;
        case ak::PType::Bool:   p = bools_[s.id] = fetchBooleanParam(s.id); break;
        case ak::PType::Choice: p = choices_[s.id] = fetchChoiceParam(s.id); break;
      }
      groupParams_[s.group].push_back({s.id, p});
    }
    for (const auto& g : groupParams_) updateGroupEnabled(g.first);
  }

  void render(const RenderArguments& args) override {
    if (!srcClip_ || !dstClip_) throwSuiteStatusException(kOfxStatFailed);
    std::unique_ptr<Image> dst(dstClip_->fetchImage(args.time));
    std::unique_ptr<const Image> src(srcClip_->fetchImage(args.time));
    if (!dst || !src) throwSuiteStatusException(kOfxStatFailed);
    if (dst->getPixelDepth() != eBitDepthFloat || dst->getPixelComponents() != ePixelComponentRGBA)
      throwSuiteStatusException(kOfxStatErrUnsupported);

    ak::Image frame;
    if (!toEngineImage(*src, frame)) throwSuiteStatusException(kOfxStatErrUnsupported);
    const OfxRectI sb = src->getBounds();

    ak::Params p = readParams(args.time);
    auto current = analysisAt(args.time, frame);

    // Earlier frames for pixel memory (deterministic: each render replays its own warm-up).
    std::vector<std::shared_ptr<const ak::Analysis>> keep;
    std::vector<ak::HistoryFrame> history;
    double fps = srcClip_->getFrameRate();
    if (fps <= 0) fps = 24;
    if (p.pixelMemory >= 0.5 && p.pixelMemoryWarmup > 0 && !abort()) {
      OfxRangeD range = srcClip_->getFrameRange();
      int warm = int(p.pixelMemoryWarmup);
      for (int k = warm; k >= 1; --k) {
        double t = args.time - k;
        if (t < range.min) continue;
        auto a = cachedOrFetch(t, frame.w, frame.h);
        if (!a) continue;
        keep.push_back(a);
        history.push_back({a.get(), int(std::floor(t)), t / fps});
      }
    }

    ak::RenderInfo info;
    info.frame = int(std::floor(args.time));
    info.seconds = args.time / fps;
    std::string title;
    title_->getValueAtTime(args.time, title);
    info.title = title.empty() ? "ANDYKIT" : title;

    ak::render(frame, *current, history, p, info);

    // Write back the render window (top-down engine rows -> bottom-up OFX rows).
    const OfxRectI& rw = args.renderWindow;
    for (int y = rw.y1; y < rw.y2; ++y) {
      if (abort()) return;
      float* out = static_cast<float*>(dst->getPixelAddress(rw.x1, y));
      if (!out) continue;
      int ey = sb.y2 - 1 - y;
      for (int x = rw.x1; x < rw.x2; ++x, out += 4) {
        int ex = x - sb.x1;
        if (ey < 0 || ey >= frame.h || ex < 0 || ex >= frame.w) { out[0] = out[1] = out[2] = out[3] = 0; continue; }
        const float* q = frame.px(ex, ey);
        out[0] = q[0]; out[1] = q[1]; out[2] = q[2]; out[3] = q[3];
      }
    }
  }

  void getFramesNeeded(const FramesNeededArguments& args, FramesNeededSetter& frames) override {
    int warm = 0;
    bool on = false;
    bools_["pixelMemory"]->getValueAtTime(args.time, on);
    if (on) ints_["pixelMemoryWarmup"]->getValueAtTime(args.time, warm);
    OfxRangeD r{args.time - std::max(0, warm), args.time};
    frames.setFramesNeeded(*srcClip_, r);
  }

  void changedParam(const InstanceChangedArgs& args, const std::string& name) override {
    if (name == kParamPresetApply) {
      int idx = 0;
      presetChoice_->getValue(idx);
      const auto& menu = presetMenu();
      if (idx < 0 || idx >= int(menu.size())) return;
      const MenuEntry& e = menu[size_t(idx)];
      if (e.builtin >= 0) {
        applyParams(ak::presetParams(size_t(e.builtin)), "Apply AndyKit preset");
        presetName_->setValue(e.label);
      } else {
        loadFromFile(e.path);
      }
      return;
    }
    if (name == kParamPresetSave) { saveToFile(args.time); return; }
    if (name == kParamPresetLoad) {
      std::string path;
      presetFile_->getValue(path);
      if (path.empty()) {
        sendMessage(Message::eMessageError, "", "Choose a preset file (.andykit) in \"Preset File\" first.");
        return;
      }
      loadFromFile(path);
      return;
    }
    if (const ak::ParamSpec* s = ak::findSpec(name)) {
      const ak::ParamSpec* en = ak::groupEnable(s->group);
      if (en && name == en->id) updateGroupEnabled(s->group);
    }
  }

  void changedClip(const InstanceChangedArgs&, const std::string&) override { cache_.clear(); }

 private:
  ak::Params readParams(double t) {
    ak::Params p;
    for (auto& kv : doubles_) { double v; kv.second->getValueAtTime(t, v); p.set(kv.first, v); }
    for (auto& kv : ints_)    { int v;    kv.second->getValueAtTime(t, v); p.set(kv.first, v); }
    for (auto& kv : bools_)   { bool v;   kv.second->getValueAtTime(t, v); p.set(kv.first, v ? 1 : 0); }
    for (auto& kv : choices_) { int v;    kv.second->getValueAtTime(t, v); p.set(kv.first, v); }
    return p;
  }

  void applyParams(const ak::Params& p, const char* undoName) {
    beginEditBlock(undoName);
    for (const auto& s : ak::paramSpecs()) {
      double v = ak::paramValue(p, s.id);
      switch (s.type) {
        case ak::PType::Double: doubles_[s.id]->setValue(v); break;
        case ak::PType::Int:    ints_[s.id]->setValue(int(std::lround(v))); break;
        case ak::PType::Bool:   bools_[s.id]->setValue(v >= 0.5); break;
        case ak::PType::Choice: choices_[s.id]->setValue(int(std::lround(v))); break;
      }
    }
    endEditBlock();
    for (const auto& g : groupParams_) updateGroupEnabled(g.first);
  }

  // Greys out a group's controls when its "Enable" checkbox is off.
  void updateGroupEnabled(const std::string& group) {
    const ak::ParamSpec* en = ak::groupEnable(group);
    if (!en) return;
    bool on = true;
    bools_[en->id]->getValue(on);
    for (auto& pp : groupParams_[group])
      if (pp.first != en->id) pp.second->setEnabled(on);
  }

  void loadFromFile(const std::string& path) {
    ak::Params p;
    std::string name, err;
    if (!ak::loadPresetFile(path, p, &name, &err)) {
      sendMessage(Message::eMessageError, "", "AndyKit could not load the preset:\n" + err);
      return;
    }
    applyParams(p, "Load AndyKit preset");
    if (!name.empty()) presetName_->setValue(name);
    presetFile_->setValue(path);
  }

  void saveToFile(double time) {
    std::string name, path, err;
    presetName_->getValue(name);
    presetFile_->getValue(path);
    if (name.empty()) name = "My Preset";
    if (path.empty()) path = ak::presetFileName(ak::defaultPresetFolder(), name);
    else if (endsWith(path, "/") || endsWith(path, "\\")) path = ak::presetFileName(path, name);
    else if (!endsWith(path, ak::kPresetExtension)) path += ak::kPresetExtension;
    if (!ak::savePresetFile(path, readParams(time), name, &err)) {
      sendMessage(Message::eMessageError, "", "AndyKit could not save the preset:\n" + err);
      return;
    }
    presetFile_->setValue(path);
    sendMessage(Message::eMessageMessage, "", "Preset \"" + name + "\" saved to:\n" + path);
  }

  std::shared_ptr<const ak::Analysis> analysisAt(double t, const ak::Image& img) {
    if (auto a = cache_.get(t, img.w, img.h)) return a;
    auto a = std::make_shared<const ak::Analysis>(ak::analyze(img));
    cache_.put(t, img.w, img.h, a);
    return a;
  }

  std::shared_ptr<const ak::Analysis> cachedOrFetch(double t, int w, int h) {
    if (auto a = cache_.get(t, w, h)) return a;
    std::unique_ptr<const Image> im(srcClip_->fetchImage(t));
    if (!im) return nullptr;
    ak::Image tmp;
    if (!toEngineImage(*im, tmp) || tmp.w != w || tmp.h != h) return nullptr;
    auto a = std::make_shared<const ak::Analysis>(ak::analyze(tmp));
    cache_.put(t, w, h, a);
    return a;
  }

  Clip* dstClip_ = nullptr;
  Clip* srcClip_ = nullptr;
  ChoiceParam* presetChoice_ = nullptr;
  StringParam* presetName_ = nullptr;
  StringParam* presetFile_ = nullptr;
  StringParam* title_ = nullptr;
  std::map<std::string, DoubleParam*> doubles_;
  std::map<std::string, IntParam*> ints_;
  std::map<std::string, BooleanParam*> bools_;
  std::map<std::string, ChoiceParam*> choices_;
  std::map<std::string, std::vector<std::pair<std::string, Param*>>> groupParams_;
  AnalysisCache cache_;
};

mDeclarePluginFactory(AndyKitPluginFactory, {}, {});

void AndyKitPluginFactory::describe(ImageEffectDescriptor& desc) {
  desc.setLabels(kPluginName, kPluginName, kPluginName);
  desc.setPluginGrouping(kPluginGrouping);
  desc.setPluginDescription(kPluginDescription);
  desc.addSupportedContext(eContextFilter);
  desc.addSupportedContext(eContextGeneral);
  desc.addSupportedBitDepth(eBitDepthFloat);
  desc.setSingleInstance(false);
  desc.setHostFrameThreading(false);
  desc.setSupportsMultiResolution(false);
  desc.setSupportsTiles(false);
  desc.setTemporalClipAccess(true);
  desc.setRenderTwiceAlways(false);
  desc.setSupportsMultipleClipPARs(false);
  desc.setRenderThreadSafety(eRenderInstanceSafe);
}

void AndyKitPluginFactory::describeInContext(ImageEffectDescriptor& desc, ContextEnum) {
  ClipDescriptor* src = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
  src->addSupportedComponent(ePixelComponentRGBA);
  src->setTemporalClipAccess(true);
  src->setSupportsTiles(false);
  src->setIsMask(false);

  ClipDescriptor* dst = desc.defineClip(kOfxImageEffectOutputClipName);
  dst->addSupportedComponent(ePixelComponentRGBA);
  dst->setSupportsTiles(false);

  PageParamDescriptor* page = desc.definePageParam("Controls");

  // ---- Presets ----
  const std::string folder = ak::defaultPresetFolder();
  GroupParamDescriptor* presetGroup = desc.defineGroupParam("grpPresets");
  presetGroup->setLabels("Presets", "Presets", "Presets");
  presetGroup->setOpen(true);
  page->addChild(*presetGroup);

  ChoiceParamDescriptor* pc = desc.defineChoiceParam(kParamPresetChoice);
  pc->setLabels("Preset", "Preset", "Preset");
  pc->setHint("Demo looks to start from, then your presets (*) found in " + folder +
              " when Resolve started. Pick one and press Apply Preset.");
  for (const auto& e : presetMenu()) pc->appendOption(e.label);
  pc->setDefault(0);
  pc->setAnimates(false);
  pc->setParent(*presetGroup);
  page->addChild(*pc);

  PushButtonParamDescriptor* pb = desc.definePushButtonParam(kParamPresetApply);
  pb->setLabels("Apply Preset", "Apply Preset", "Apply Preset");
  pb->setParent(*presetGroup);
  page->addChild(*pb);

  StringParamDescriptor* pn = desc.defineStringParam(kParamPresetName);
  pn->setLabels("Preset Name", "Preset Name", "Preset Name");
  pn->setHint("Name stored inside the preset file and shown in the menu.");
  pn->setDefault("My Preset");
  pn->setStringType(eStringTypeSingleLine);
  pn->setAnimates(false);
  pn->setParent(*presetGroup);
  page->addChild(*pn);

  StringParamDescriptor* pf = desc.defineStringParam(kParamPresetFile);
  pf->setLabels("Preset File", "Preset File", "Preset File");
  pf->setHint("A .andykit file anywhere on your computer (e.g. your #ASSETS folder). "
              "Leave empty to save into " + folder + ", whose presets also appear in the menu after restarting Resolve. "
              "You can also type a folder ending with / to save there using the preset name.");
  pf->setDefault("");
  pf->setStringType(eStringTypeFilePath);
  pf->setFilePathExists(false);
  pf->setAnimates(false);
  pf->setParent(*presetGroup);
  page->addChild(*pf);

  PushButtonParamDescriptor* ps = desc.definePushButtonParam(kParamPresetSave);
  ps->setLabels("Save Preset to File", "Save Preset to File", "Save Preset to File");
  ps->setHint("Writes every current setting to the Preset File.");
  ps->setParent(*presetGroup);
  page->addChild(*ps);

  PushButtonParamDescriptor* pl = desc.definePushButtonParam(kParamPresetLoad);
  pl->setLabels("Load Preset from File", "Load Preset from File", "Load Preset from File");
  pl->setHint("Reads the Preset File and applies it.");
  pl->setParent(*presetGroup);
  page->addChild(*pl);

  // ---- effect parameters, grouped; each group starts with its Enable checkbox ----
  std::map<std::string, GroupParamDescriptor*> groups;
  for (const auto& s : ak::paramSpecs()) {
    GroupParamDescriptor*& g = groups[s.group];
    if (!g) {
      std::string gid = std::string("grp") + s.group;
      gid.erase(std::remove(gid.begin(), gid.end(), ' '), gid.end());
      g = desc.defineGroupParam(gid);
      g->setLabels(s.group, s.group, s.group);
      g->setOpen(std::string(s.group) == "General" || std::string(s.group) == "Points");
      page->addChild(*g);
      if (std::string(s.group) == "General") { // frame title lives with the frame/labels toggles
        StringParamDescriptor* title = desc.defineStringParam(kParamTitle);
        title->setLabels("Frame Title", "Frame Title", "Frame Title");
        title->setHint("Text shown in the artwork frame when Labels is on.");
        title->setDefault("ANDYKIT");
        title->setStringType(eStringTypeSingleLine);
        title->setAnimates(false);
        title->setParent(*g);
        page->addChild(*title);
      }
    }
    ParamDescriptor* pd = nullptr;
    switch (s.type) {
      case ak::PType::Double: {
        DoubleParamDescriptor* d = desc.defineDoubleParam(s.id);
        d->setDefault(s.def);
        d->setRange(s.min, s.max);
        d->setDisplayRange(s.min, s.max);
        d->setIncrement((s.max - s.min) <= 10 ? 0.05 : 1.0);
        d->setDigits((s.max - s.min) <= 10 ? 2 : 0);
        pd = d;
        break;
      }
      case ak::PType::Int: {
        IntParamDescriptor* d = desc.defineIntParam(s.id);
        d->setDefault(int(s.def));
        d->setRange(int(s.min), int(s.max));
        d->setDisplayRange(int(s.min), int(s.max));
        pd = d;
        break;
      }
      case ak::PType::Bool: {
        BooleanParamDescriptor* d = desc.defineBooleanParam(s.id);
        d->setDefault(s.def >= 0.5);
        d->setAnimates(true);
        pd = d;
        break;
      }
      case ak::PType::Choice: {
        ChoiceParamDescriptor* d = desc.defineChoiceParam(s.id);
        for (const auto& o : *s.choices) d->appendOption(o);
        d->setDefault(int(s.def));
        pd = d;
        break;
      }
    }
    pd->setLabels(s.label, s.label, s.label);
    pd->setParent(*g);
    page->addChild(*pd);
  }
}

ImageEffect* AndyKitPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum) {
  return new AndyKitPlugin(handle);
}

namespace OFX {
namespace Plugin {
void getPluginIDs(PluginFactoryArray& ids) {
  static AndyKitPluginFactory p(kPluginIdentifier, AK_VERSION_MAJOR, AK_VERSION_MINOR);
  ids.push_back(&p);
}
} // namespace Plugin
} // namespace OFX
