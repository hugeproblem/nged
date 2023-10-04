#include "parminspector.h"
#include <imgui.h>
#include <imgui_stdlib.h>

extern "C" {
#include <stdint.h>
#include <lua.h>
#include <lauxlib.h>
}

#include <sol/sol.hpp>
#include <type_traits>
#include <utility>

#ifdef DEBUG
#include <stdio.h>
#define WARN(...) fprintf(stderr, __VA_ARGS__)
#define INFO(...) fprintf(stdout, __VA_ARGS__)
#else
#define WARN(...) /*nothing*/
#define INFO(...) /*nothing*/
#endif

namespace parmscript {

hashmap<string, FieldInspector> ParmSetInspector::inspectorOverrides_;

static std::string parmlabel(Parm const& parm)
{
  return parm.label() + "##" + parm.path();
}

static bool boolFieldInspector(Parm& parm)
{
  auto label = parmlabel(parm);
  bool v = parm.as<bool>();
  if (ImGui::Checkbox(label.c_str(), &v)) {
    parm.set<bool>(v);
    return true;
  }
  return false;
}

template <class T>
struct ImGuiDataTypeTrait {};
template <> struct ImGuiDataTypeTrait<int32_t> { static constexpr int value = ImGuiDataType_S32; };
template <> struct ImGuiDataTypeTrait<uint32_t> { static constexpr int value = ImGuiDataType_U32; };
template <> struct ImGuiDataTypeTrait<int64_t> { static constexpr int value = ImGuiDataType_S64; };
template <> struct ImGuiDataTypeTrait<uint64_t> { static constexpr int value = ImGuiDataType_U64; };
template <> struct ImGuiDataTypeTrait<float> { static constexpr int value = ImGuiDataType_Float; };
template <> struct ImGuiDataTypeTrait<double> { static constexpr int value = ImGuiDataType_Double; };

template <class valuetype, class elemtype, int defaultmin=0, int defaultmax=1>
bool scalarFieldInspector(Parm& parm)
{
  constexpr int numcomponent = sizeof(valuetype) / sizeof(elemtype);
  constexpr int scalartype = ImGuiDataTypeTrait<elemtype>::value;
  bool imdirty = false;
  auto v = parm.as<valuetype>();
  auto ui = parm.getMeta<string>("ui", "drag");
  auto min = parm.getMeta<elemtype>("min", elemtype(defaultmin));
  auto max = parm.getMeta<elemtype>("max", elemtype(defaultmax));
  auto speed = parm.getMeta<elemtype>("speed", 1);
  auto label = parmlabel(parm);
  void* pdata = &v;
  if (ui == "drag") {
    imdirty = ImGui::DragScalarN(label.c_str(), scalartype, pdata, numcomponent, speed, &min, &max);
  } else if (ui == "slider") {
    imdirty = ImGui::SliderScalarN(label.c_str(), scalartype, pdata, numcomponent, &min, &max);
  } else {
    imdirty = ImGui::InputScalarN(label.c_str(), scalartype, pdata, numcomponent);
  }
  if (imdirty)
    parm.set<valuetype>(v);
  return imdirty;
}

bool stringFieldInspector(Parm& parm)
{
  bool imdirty = false;
  auto* v = parm.getPtr<std::string>();
  auto label = parmlabel(parm);
  bool multiline = parm.getMeta<bool>("multiline", false);
  if (multiline)
    imdirty = ImGui::InputTextMultiline(label.c_str(), v, ImVec2(0,0), ImGuiInputTextFlags_EnterReturnsTrue);
  else
    imdirty = ImGui::InputText(label.c_str(), v, ImGuiInputTextFlags_EnterReturnsTrue);
  return imdirty;
}

bool colorFieldInspector(Parm& parm)
{
  bool imdirty = false;
  auto label = parmlabel(parm);
  auto v = parm.as<Parm::color>();
  bool alpha = parm.getMeta<bool>("alpha", false);
  bool hsv = parm.getMeta<bool>("hsv", false);
  bool hdr = parm.getMeta<bool>("hdr", false);
  bool wheel = parm.getMeta<bool>("wheel", false);
  bool picker = parm.getMeta<bool>("picker", false);

  uint32_t flags = 0;
  if (alpha)
    flags |= ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf;
  else
    flags |= ImGuiColorEditFlags_NoAlpha;

  if (hsv)
    flags |= ImGuiColorEditFlags_DisplayHSV;
  else
    flags |= ImGuiColorEditFlags_DisplayRGB;

  if (hdr)
    flags |= ImGuiColorEditFlags_Float;
  else
    flags |= ImGuiColorEditFlags_Uint8;

  if (wheel)
    flags |= ImGuiColorEditFlags_PickerHueWheel;

  if (alpha) {
    if (picker)
      imdirty = ImGui::ColorPicker4(label.c_str(), &v.r, flags);
    else
      imdirty = ImGui::ColorEdit4(label.c_str(), &v.r, flags);
  } else {
    if (picker)
      imdirty = ImGui::ColorPicker3(label.c_str(), &v.r, flags);
    else
      imdirty = ImGui::ColorEdit3(label.c_str(), &v.r, flags);
  }
  if (imdirty)
    parm.set<Parm::color>(v);
  return imdirty;
}

bool ParmSetInspector::inspect(Parm& parm, hashset<string>& modified, lua_State* L, ParmFonts* fonts)
{
  bool imdirty = false;
  bool displayChildren = true;
  bool fontPushed = false;
  bool itemWidthPushed = false;
  auto label = parmlabel(parm);

  auto disablewhen = parm.getMeta<string>("disablewhen", "");
  if (fonts) {
    auto font = parm.getMeta<string>("font", "regular");
    if (font == "regular" && fonts->regular) {
      ImGui::PushFont(fonts->regular);
      fontPushed = true;
    } else if (font == "mono" && fonts->mono) {
      ImGui::PushFont(fonts->mono);
      fontPushed = true;
    }
  }
  if (int widthmeta = 0; parm.tryGetMeta<int>("width", &widthmeta)) {
    ImGui::PushItemWidth(widthmeta);
    itemWidthPushed = true;
  }
  if (!disablewhen.empty()) {
    // expand {path.to.parm} to its value
    // expand {menu:path.to.parm::item} to its value
    // expand {length:path.to.parm} to its value
    // translate != into ~=, || into or, && into and, ! into not
    // evaluate disablewhen expr in Lua

    // init the eval function:
    bool disabled = false;

    if (L == nullptr)
      L = parmset_->defaultLuaRuntime();
    sol::state_view lua{L};
    auto loaded = lua.load(R"LUA(
local ps, evalParm, expr=...
return expr:gsub('{([^}]+)}', function(expr)
  local e = evalParm(ps, expr)
  if e~=nil then
    return string.format("%q", e)
  else
    return '{error}'
  end
end):gsub('!=', '~='):gsub('||', ' or '):gsub('&&', ' and '):gsub('!', 'not ')
    )LUA");
    if (loaded.valid()) {
      std::string expanded = loaded.call(parm.root(), ParmSet::evalParm, disablewhen);
      // INFO("disablewhen \"%s\" expanded to \"%s\"\n", disablewhen.c_str(), expanded.c_str());
      if (expanded.find("{error}") == std::string::npos) {
        loaded = lua.load("return "+expanded, "disablewhen", sol::load_mode::text);
        if (loaded.valid()) {
          disabled = loaded.call();
        }
      }
    } else {
      WARN("failed to load disablewhen script");
    }

    ImGui::BeginDisabled(disabled);
  }

  auto ui = parm.ui();
  using ui_type_enum = Parm::ui_type_enum;
  using value_type_enum = Parm::value_type_enum;
  if (ui == ui_type_enum::FIELD) {
    imdirty = getFieldInspector(parm)(parm);
  } else if (ui == Parm::ui_type_enum::GROUP) {
    if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      displayChildren = false;
    }
  } else if (ui == Parm::ui_type_enum::STRUCT) {
    if (!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
      displayChildren = false;
    }
  } else if (ui == Parm::ui_type_enum::LIST) {
    displayChildren = false;
    int numitems = parm.numListValues();
    if (ImGui::InputInt(("# "+label).c_str(), &numitems)) {
      parm.resizeList(numitems);
      modified.insert(parm.path());
      imdirty = true;
    }
    for (int i=0; i<numitems; ++i) {
      for (auto field: parm.getListStruct(i)->allFields()) {
        imdirty |= inspect(*field, modified, L, fonts);
      }
      if (i+1<numitems)
        ImGui::Separator();
    }
  } else if (ui  == ui_type_enum::LABEL) {
    ImGui::TextUnformatted(parm.label().c_str());
  } else if (ui  == ui_type_enum::BUTTON) {
    if (ImGui::Button(label.c_str())) {
      modified.insert(parm.path());
      // TODO: callback(?)
    }
  } else if (ui == ui_type_enum::SPACER) {
    ImGui::Spacing();
  } else if (ui == ui_type_enum::SEPARATOR) {
    ImGui::Separator();
  } else if (ui == ui_type_enum::MENU) {
    std::vector<char const*> labels;
    labels.reserve(parm.menuLabels().size());
    for (auto const& label: parm.menuLabels()) {
      labels.push_back(label.c_str());
    }
    if (ImGui::Combo(label.c_str(), parm.getPtr<int>(), labels.data(), labels.size()))
      imdirty = true;
  } else {
    INFO("unknown ui %s of type %d\n", parm.name().c_str(), static_cast<int>(ui));
  }

  if (displayChildren && parm.numFields() != 0) {
    for (auto child: parm.allFields())
      imdirty |= inspect(*child, modified, L, fonts);
  }
  if (ui == ui_type_enum::STRUCT && displayChildren) {
    ImGui::TreePop();
  }

  if (itemWidthPushed)
    ImGui::PopItemWidth();
  if (fontPushed)
    ImGui::PopFont();
  if (!disablewhen.empty()) {
    ImGui::EndDisabled();
  }
  if (imdirty) {
    modified.insert(parm.path());
    if (ui != ui_type_enum::BUTTON) {
      edited_ = true;
      if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        editing_ = true;
    }
  }
  if (editing_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    editing_ = false;
  if (parm.getMeta<bool>("joinnext", false))
    ImGui::SameLine();
  return imdirty;
} 

FieldInspector ParmSetInspector::getFieldInspector(Parm const& parm)
{
  auto inspector = parm.getMeta<std::string>("inspector", "");
  if (inspector != "") {
    if (auto itr = inspectorOverrides_.find(inspector); itr != inspectorOverrides_.end())
      return itr->second;
  }
  using value_type_enum = Parm::value_type_enum;
  switch(parm.type()) {
    case value_type_enum::BOOL:
      return boolFieldInspector;
    case value_type_enum::INT:
      return scalarFieldInspector<int, int, 0, 10>;
    case value_type_enum::INT2:
      return scalarFieldInspector<Parm::int2, int, 0, 10>;
    case value_type_enum::FLOAT:
      return scalarFieldInspector<float, float, 0, 1>;
    case value_type_enum::FLOAT2:
      return scalarFieldInspector<Parm::float2, float, 0, 1>;
    case value_type_enum::FLOAT3:
      return scalarFieldInspector<Parm::float3, float, 0, 1>;
    case value_type_enum::FLOAT4:
      return scalarFieldInspector<Parm::float4, float, 0, 1>;
    case value_type_enum::DOUBLE:
      return scalarFieldInspector<double, double, 0, 1>;
    case value_type_enum::STRING:
      return stringFieldInspector;
    case value_type_enum::COLOR:
      return colorFieldInspector;
  }
  return [](Parm& parm)->bool {
    WARN("dow\'t know how to inspect parm \"%s\"\n", parm.path().c_str());
    return false;
  };
}

void ParmSetInspector::loadParmScript(std::string_view script)
{
  auto newparms = std::make_unique<ParmSet>();
  newparms->loadScript(script);
  parmset_.swap(newparms);
}

bool ParmSetInspector::inspect(lua_State* L, ParmFonts* fonts)
{
  if (!parmset_)
    return false;
  if (L == nullptr)
    L = parmset_->defaultLuaRuntime();
  parmset_->clearDirtyEntries();
  for (auto child: parmset_->root_->allFields())
    inspect(*child, parmset_->dirtyEntries_, L, fonts);
  edited_ |= !parmset_->dirtyEntries_.empty();
  return !parmset_->dirtyEntries_.empty();
}

}

