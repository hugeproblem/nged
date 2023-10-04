#pragma once

#include "parmscript.h"

struct ImFont;
struct ParmFonts
{
  ImFont* regular;
  ImFont* mono;
};

namespace parmscript {

using FieldInspector = std::function<bool(Parm&)>;

class ParmSetInspector final
{
  ParmSetInspector(ParmSetInspector const&) = delete;
protected:
  bool edited_ = false;
  bool editing_ = false;
  std::unique_ptr<ParmSet> parmset_;
  static hashmap<string, FieldInspector> inspectorOverrides_;

  bool inspect(Parm& parm, hashset<string>& dirty, lua_State* L = nullptr, ParmFonts* fonts = nullptr);

public:
  ParmSetInspector()
  {
    parmset_ = std::make_unique<ParmSet>();
  }
  static void setFieldInspector(string const& name, FieldInspector f)
  {
    inspectorOverrides_[name] = std::move(f);
  }
  static FieldInspector getFieldInspector(Parm const& parm);

  void setParms(std::unique_ptr<ParmSet> parms) { parmset_ = std::move(parms); }
  void loadParmScript(std::string_view script);
  auto& parms() { return *parmset_; }
  auto getParm(string const& name) -> ParmPtr
  {
    if (empty()) return nullptr;
    return parmset_->get(name);
  }
  auto const& parms() const { return *parmset_; }
  bool        empty() const { return !parmset_ || !parmset_->loaded(); }

  bool inspect(lua_State* L=nullptr, ParmFonts* fonts=nullptr);
  auto const& dirtyEntries() const { return parmset_->dirtyEntries(); }
  bool doneEditing() const { return edited_ && !editing_; } // supposed to be used as save points
  bool edited() const { return edited_; }
  bool dirty() const { return edited_; }
  void markClean() { parmset_->clearDirtyEntries(); edited_ = false; }     // when saved, remember to reset this
};

} // namespace parmscript

