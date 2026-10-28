// AndyKit — parameter table shared by the engine, the OFX plugin and the CLI.
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>

namespace ak {

enum class PType { Double, Int, Bool, Choice };

// Choice lists (index order is part of the preset format: append, never reorder)
inline const std::vector<std::string> kColorModes   = {"Source", "Palette", "Mono", "Duotone", "Hybrid"};
inline const std::vector<std::string> kPalettes     = {"Solar", "Signal", "Cobalt", "Mono", "Ember", "Acid", "Forest", "Cyan"};
inline const std::vector<std::string> kBlendModes   = {"Normal", "Screen", "Add", "Overlay", "Soft Light", "Hard Light",
                                                       "Multiply", "Darken", "Lighten", "Difference", "Color Dodge", "Luminosity"};
inline const std::vector<std::string> kMotionModes  = {"None", "Glitch", "Drift", "Pulse", "Orbit"};
inline const std::vector<std::string> kPrimitives   = {"Bracket", "Square", "Circle", "Cross", "Diamond", "Mixed"};
inline const std::vector<std::string> kConnModes    = {"Contour", "Nearest", "Radial", "Orthogonal", "None"};
inline const std::vector<std::string> kFocusModes   = {"Emphasize", "Isolate", "Off"};
inline const std::vector<std::string> kMemoryStyles = {"Feedback", "Glyphs", "Archive"};
inline const std::vector<std::string> kContourStyles= {"Topographic", "Flow"};
inline const std::vector<std::string> kDitherAlgos  = {"Atkinson", "Floyd-Steinberg", "Bayer"};

// X-macro: id, label, group, type, default, min, max, choices (nullptr if none)
#define AK_PARAM_LIST(X) \
  /* General */ \
  X(seed,               "Seed",                 "General",      Int,    426,  1,   9999, nullptr) \
  X(colorMode,          "Color Mode",           "General",      Choice, 0,    0,   4,    &kColorModes) \
  X(palette,            "Palette",              "General",      Choice, 0,    0,   7,    &kPalettes) \
  X(blendMode,          "Overlay Blend",        "General",      Choice, 1,    0,   11,   &kBlendModes) \
  X(baseOpacity,        "Source Opacity",       "General",      Double, 78,   0,   100,  nullptr) \
  X(overlayOpacity,     "Overlay Opacity",      "General",      Double, 74,   0,   100,  nullptr) \
  X(artworkFrame,       "Artwork Frame",        "General",      Bool,   1,    0,   1,    nullptr) \
  X(labels,             "Labels",               "General",      Bool,   0,    0,   1,    nullptr) \
  /* Motion */ \
  X(motionEnable,       "Enable Motion",        "Motion",       Bool,   1,    0,   1,    nullptr) \
  X(motionMode,         "Motion",               "Motion",       Choice, 1,    0,   4,    &kMotionModes) \
  X(motionAmount,       "Motion Amount",        "Motion",       Double, 52,   0,   100,  nullptr) \
  X(motionSpeed,        "Motion Speed",         "Motion",       Double, 54,   0,   100,  nullptr) \
  /* Points & shapes */ \
  X(pointsEnable,       "Enable Points",        "Points",       Bool,   1,    0,   1,    nullptr) \
  X(density,            "Density",              "Points",       Double, 54,   0,   100,  nullptr) \
  X(threshold,          "Edge Threshold",       "Points",       Double, 24,   0,   100,  nullptr) \
  X(pointRadius,        "Point Radius",         "Points",       Double, 1.45, 0,   6,    nullptr) \
  X(nodeShapes,         "Node Shapes",          "Points",       Bool,   1,    0,   1,    nullptr) \
  X(primitive,          "Shape",                "Points",       Choice, 0,    0,   5,    &kPrimitives) \
  X(shapeMin,           "Shape Min",            "Points",       Double, 6,    1,   80,   nullptr) \
  X(shapeMax,           "Shape Max",            "Points",       Double, 34,   2,   160,  nullptr) \
  X(featureSizing,      "Feature Sizing",       "Points",       Double, 76,   0,   100,  nullptr) \
  X(sizeVariation,      "Size Variation",       "Points",       Double, 38,   0,   100,  nullptr) \
  X(sizeHierarchy,      "Size Hierarchy",       "Points",       Double, 74,   0,   100,  nullptr) \
  X(shapeStroke,        "Shape Stroke",         "Points",       Double, 0.8,  0.1, 5,    nullptr) \
  X(circles,            "Halo Circles",         "Points",       Bool,   0,    0,   1,    nullptr) \
  /* Connections */ \
  X(connectionsEnable,  "Enable Connections",   "Connections",  Bool,   1,    0,   1,    nullptr) \
  X(connectionMode,     "Connection Mode",      "Connections",  Choice, 0,    0,   4,    &kConnModes) \
  X(connection,         "Connection",           "Connections",  Double, 72,   0,   200,  nullptr) \
  X(edgeConfidence,     "Edge Confidence",      "Connections",  Double, 28,   0,   100,  nullptr) \
  X(lineWeight,         "Line Weight",          "Connections",  Double, 0.75, 0.1, 5,    nullptr) \
  X(network,            "Hub Network",          "Connections",  Bool,   0,    0,   1,    nullptr) \
  /* Subject */ \
  X(subjectEnable,      "Enable Subject Focus", "Subject",      Bool,   1,    0,   1,    nullptr) \
  X(objectFocus,        "Subject Focus",        "Subject",      Choice, 0,    0,   2,    &kFocusModes) \
  X(objectStrength,     "Focus Strength",       "Subject",      Double, 72,   0,   100,  nullptr) \
  X(objectConfidence,   "Focus Tightness",      "Subject",      Double, 45,   0,   100,  nullptr) \
  /* Pixel memory */ \
  X(pixelMemory,        "Enable Pixel Memory",  "Pixel Memory", Bool,   1,    0,   1,    nullptr) \
  X(pixelMemoryStyle,   "Memory Style",         "Pixel Memory", Choice, 0,    0,   2,    &kMemoryStyles) \
  X(pixelMemoryCellSize,"Memory Cell Size",     "Pixel Memory", Double, 6,    2,   32,   nullptr) \
  X(pixelMemoryThreshold,"Memory Threshold",    "Pixel Memory", Double, 46,   0,   100,  nullptr) \
  X(pixelMemoryFeedback,"Memory Feedback",      "Pixel Memory", Double, 92,   0,   99,   nullptr) \
  X(pixelMemoryWindows, "Memory Windows",       "Pixel Memory", Int,    4,    0,   12,   nullptr) \
  X(pixelMemoryEntropy, "Memory Entropy",       "Pixel Memory", Double, 52,   0,   100,  nullptr) \
  X(pixelMemoryOpacity, "Memory Opacity",       "Pixel Memory", Double, 78,   0,   100,  nullptr) \
  X(pixelMemoryPalette, "Memory Palette",       "Pixel Memory", Choice, 1,    0,   7,    &kPalettes) \
  X(pixelMemoryWarmup,  "Memory Warm-up Frames","Pixel Memory", Int,    24,   0,   96,   nullptr) \
  /* Texture */ \
  X(textureEnable,      "Enable Texture",       "Texture",      Bool,   1,    0,   1,    nullptr) \
  X(scanlines,          "Scanlines",            "Texture",      Double, 18,   0,   100,  nullptr) \
  X(grain,              "Grain",                "Texture",      Double, 10,   0,   100,  nullptr) \
  /* Contour field */ \
  X(contourField,       "Enable Contours",      "Contours",     Bool,   0,    0,   1,    nullptr) \
  X(contourStyle,       "Contour Style",        "Contours",     Choice, 0,    0,   1,    &kContourStyles) \
  X(contourLevels,      "Contour Levels",       "Contours",     Int,    14,   2,   40,   nullptr) \
  X(contourDetail,      "Contour Detail",       "Contours",     Double, 55,   0,   100,  nullptr) \
  X(contourOpacity,     "Contour Opacity",      "Contours",     Double, 80,   0,   100,  nullptr) \
  X(contourDrift,       "Contour Drift",        "Contours",     Double, 12,   0,   100,  nullptr) \
  /* Grid */ \
  X(grid,               "Enable Grid",          "Grid",         Bool,   0,    0,   1,    nullptr) \
  X(gridModules,        "Grid Modules",         "Grid",         Int,    12,   2,   48,   nullptr) \
  X(gridOpacity,        "Grid Opacity",         "Grid",         Double, 12,   0,   100,  nullptr) \
  /* Dither */ \
  X(ditherPrint,        "Enable Dither",        "Dither",       Bool,   0,    0,   1,    nullptr) \
  X(ditherAlgorithm,    "Dither Algorithm",     "Dither",       Choice, 0,    0,   2,    &kDitherAlgos) \
  X(ditherCellSize,     "Dither Cell Size",     "Dither",       Double, 6,    2,   24,   nullptr) \
  X(ditherThreshold,    "Dither Threshold",     "Dither",       Double, 50,   0,   100,  nullptr) \
  X(ditherContrast,     "Dither Contrast",      "Dither",       Double, 68,   0,   100,  nullptr) \
  X(ditherPalette,      "Dither Palette",       "Dither",       Choice, 2,    0,   7,    &kPalettes) \
  X(ditherInvert,       "Dither Invert",        "Dither",       Bool,   0,    0,   1,    nullptr) \
  X(ditherOpacity,      "Dither Opacity",       "Dither",       Double, 100,  0,   100,  nullptr) \
  X(ditherBlend,        "Dither Blend",         "Dither",       Choice, 0,    0,   11,   &kBlendModes) \
  X(ditherMotion,       "Dither Motion",        "Dither",       Double, 0,    0,   100,  nullptr) \
  /* Rhythm (replaces audio reactivity: OFX effects get no audio) */ \
  X(rhythm,             "Enable Beat Pulse",    "Rhythm",       Bool,   1,    0,   1,    nullptr) \
  X(bpm,                "BPM",                  "Rhythm",       Double, 120,  40,  200,  nullptr) \
  X(beatOffset,         "Beat Offset",          "Rhythm",       Double, 0,    0,   100,  nullptr) \
  X(syncStrength,       "Pulse Strength",       "Rhythm",       Double, 72,   0,   100,  nullptr) \
  X(bassGain,           "Shape Pulse",          "Rhythm",       Double, 88,   0,   150,  nullptr) \
  X(midGain,            "Line Pulse",           "Rhythm",       Double, 62,   0,   150,  nullptr) \
  X(highGain,           "Glitch Pulse",         "Rhythm",       Double, 54,   0,   150,  nullptr)

struct Params {
#define AK_X(id, label, group, type, def, mn, mx, choices) double id = def;
  AK_PARAM_LIST(AK_X)
#undef AK_X
  bool set(const std::string& key, double v);
};

struct ParamSpec {
  const char* id;
  const char* label;
  const char* group;
  PType type;
  double def, min, max;
  const std::vector<std::string>* choices;
};

const std::vector<ParamSpec>& paramSpecs();
const ParamSpec* findSpec(const std::string& id);
// The "Enable ..." checkbox that switches a whole group on/off (nullptr if the group has none).
const ParamSpec* groupEnable(const std::string& group);
// Sets a value from text: numbers, true/false, or a choice label (case-insensitive).
bool setFromText(Params& p, const std::string& key, const std::string& value);

} // namespace ak
