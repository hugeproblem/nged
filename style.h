#pragma once
#include "gmath.h"
#include <stdint.h>

namespace nged {
using gmath::Vec2;
struct UIStyle
{
  uint32_t windowBackgroundColor    = 0x333333ff;
  uint32_t nodeDefaultColor         = 0xddddddff;
  float    nodeStrokeWidth          = 0.f;
  uint32_t nodeStrokeColor          = 0x000000ff;
  float    nodePinRadius            = 3.4f;
  float    linkStrokeWidth          = 2.f;
  uint32_t linkDefaultColor         = 0xddddddff;
  float    linkSelectedWidth        = 4.f;
  uint32_t linkSelectedColor        = 0xffff00ff;
  uint32_t arrowDefaultColor        = 0xff0000ff;
  uint32_t arrowSelectedColor       = 0xffff00ff;
  uint32_t nodeLabelColor           = 0xeeeeeeff;
  uint32_t selectionBoxBackground   = 0x33691E88;
  uint32_t deselectionBoxBackground = 0x600D1E88;
  float    routerRadius             = 6.f;
  Vec2     commentBoxMargin         = {8.f, 8.f};
  uint32_t commentColor             = 0x4caf50ff;
  uint32_t commentBackground        = 0x004d4066;
  uint32_t groupBoxBackground       = 0x44444466;
  float    bigFontSize              = 24;
  float    normalFontSize           = 18;
  float    smallFontSize            = 14;
  float    commandPaletteWidthRatio = 0.75f; // width / parent window width
  float    groupboxHeaderHeight     = 16.f;

public:
  static UIStyle& instance();
  void save();
  void load();
};

} // namespace nged
