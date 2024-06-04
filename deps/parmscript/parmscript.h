#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

typedef struct lua_State lua_State;

namespace parmscript {

class Parm;
class ParmSet;
using ParmPtr = std::shared_ptr<Parm>;
using ConstParmPtr = std::shared_ptr<const Parm>;
using ParmSetPtr = std::shared_ptr<ParmSet>;
using string=std::string;
template <class K, class V>
using hashmap=std::unordered_map<K,V>;
template <class T>
using hashset=std::unordered_set<T>;

class Parm
{
public:
  struct int2   { int x,y; };
  struct float2 { float x,y; };
  struct float3 { float x,y,z; };
  struct float4 { float x,y,z,w; };
  struct color  { float r,g,b,a; };

  enum class value_type_enum : size_t {
    NONE,           BOOL, INT, INT2, FLOAT, DOUBLE, FLOAT2, FLOAT3, FLOAT4, COLOR, STRING};
  using value_type = std::variant<
    std::monostate, bool, int, int2, float, double, float2, float3, float4, color, string>;
  enum class ui_type_enum {
    FIELD, LABEL, BUTTON, SPACER, SEPARATOR, MENU, GROUP, STRUCT, LIST};

protected:
  ParmSet                    *root_=nullptr;
  ui_type_enum                ui_type_=ui_type_enum::LABEL;
  value_type_enum             expected_value_type_;
  value_type                  value_;
  value_type                  default_;
  string                      name_;
  string                      path_;
  string                      label_;
  hashmap<string, value_type> meta_;
  std::vector<int>            menu_values_;
  std::vector<string>         menu_items_;
  std::vector<string>         menu_labels_;
  // if the scope is a plain struct, then this holds everything
  std::vector<ParmPtr>        fields_; 
  // if the scope is a list, fields_ holds the template (label / default value / everything)
  // and listValues_ holds the values.
  // e.g., for a list of {string name; int value;} pairs
  // fields_[0] == {def of string name}, fields_[1] = {def of int value}
  // listValues_[0].fields_[0] = value of first string,  i.e., list[0].name
  // listValues_[0].fields_[1] = value of first int,     i.e., list[0].value
  // listValues_[1].fields_[0] = value of second string, i.e., list[1].name
  // listValues_[1].fields_[1] = value of second int,    i.e., list[1].value
  std::vector<ParmPtr>        listValues_;


  static string titleize(string s)
  {
    bool space = true;
    for (auto& c: s) {
      if (std::isspace(c)) {
        space = true;
      } else if (space) {
        c = std::toupper(c);
        space = false;
      }
    }
    return s;
  }

public:
  Parm(ParmSet* root):root_(root){}
  Parm(Parm&&)=default;
  ~Parm()=default;
  Parm(Parm const& that)
    : root_(that.root_)
    , ui_type_(that.ui_type_)
    , value_(that.value_)
    , expected_value_type_(that.expected_value_type_)
    , default_(that.default_)
    , name_(that.name_)
    , path_(that.path_)
    , label_(that.label_)
    , meta_(that.meta_)
    , menu_values_(that.menu_values_)
    , menu_items_(that.menu_items_)
    , menu_labels_(that.menu_labels_)
  {
    fields_.reserve(that.fields_.size());
    for (auto f: that.fields_)
      fields_.push_back(std::make_shared<Parm>(*f));
    listValues_.reserve(that.listValues_.size());
    for (auto v: that.listValues_)
      listValues_.push_back(std::make_shared<Parm>(*v));
  }

  auto const& name() const { return name_; }
  auto const& label() const { return label_; }
  auto const& path() const { return path_; }
  auto const& value() const { return value_; }
  auto const& defaultValue() const { return default_; }
  auto const  type() const { return expected_value_type_; }
  auto const  ui() const { return ui_type_; }
  auto*       root() const { return root_; }
  auto const& menuLabels() const { return menu_labels_; }

  // retrieve value:
  template <class T>
  constexpr std::enable_if_t<!std::is_same_v<T, int> && !std::is_same_v<T, string>, T>
  as() const { return std::get<T>(value_); }

  // get pointer:
  template <class T>
  constexpr auto
  getPtr() noexcept { return std::get_if<T>(&value_); }

  // special case for menu:
  template <class T>
  std::enable_if_t<std::is_same_v<T, int>, T>
  as() const {
    if (ui_type_ == ui_type_enum::MENU) {
      int idx = std::get<int>(value_);
      if (menu_values_.size() == menu_items_.size() && idx>=0 && idx<menu_values_.size()) {
        return menu_values_[idx];
      } else {
        return idx;
      }
    } else {
      return std::get<int>(value_);
    }
  }
  template <class T>
  std::enable_if_t<std::is_same_v<T, string>, T>
  as() const {
     if (ui_type_ == ui_type_enum::MENU) {
      int idx = std::get<int>(value_);
      if (idx>=0 && idx<menu_items_.size()) {
        return menu_items_[idx];
      } else {
        return string();
      }
    } else {
      return std::get<string>(value_);
    }   
  }

  // retrieve value:
  template <class T>
  std::enable_if_t<!std::is_same_v<T, string>, bool>
  is() const { return std::holds_alternative<T>(value_); }

  // special case for menu:
  template <class T>
  std::enable_if_t<std::is_same_v<T, string>, bool>
  is() const {
    if (ui_type_ == ui_type_enum::MENU) {
      return std::holds_alternative<int>(value_);
    } else {
      return std::holds_alternative<string>(value_);
    }   
  }

  auto    numFields() const { return fields_.size(); }
  ParmPtr getField(size_t i) const {
    if (i<fields_.size())
      return fields_[i];
    return nullptr;
  }
  auto&   allFields() { return fields_; }

  ParmPtr getField(string const& relpath);

  // retrieve list item:
  auto numListValues() const {
    return listValues_.size();
  }
  ParmPtr getListStruct(size_t i) const {
    return listValues_[i];
  }
  template <class T>
  bool getListValue(size_t i, size_t f, T& ret) {
    if (i<listValues_.size()) {
      assert(listValues_[i]);
      assert(listValues_[i]->fields_.size() == fields_.size());
      if (f<listValues_[i]->fields_.size()) {
        assert(listValues_[i]->fields_[f]);
        if (!listValues_[i]->fields_[f]->is<T>())
          return false;
        ret = listValues_[i]->fields_[f]->as<T>();
        return true;
      }
    }
    return false;
  }
  template <class T>
  bool getListValue(size_t i, string const& f, T& ret) {
    if (i<listValues_.size()) {
      assert(listValues_[i]);
      assert(listValues_[i]->fields_.size() == fields_.size());
      if (auto field = listValues_[i]->getField(f)) {
        if (!field->is<T>())
          return false;
        ret = field->as<T>();
        return true;
      }
    }
    return false;
  } 

  ParmPtr at(size_t idx) const {
    if (ui_type_ != ui_type_enum::LIST)
      throw std::domain_error("index into none-list parm");
    if (idx>=listValues_.size())
      throw std::out_of_range("index out of range");
    return listValues_.at(idx);
  }

  // set values:
  template <class T>
  inline bool set(T value);

  // set list:
  void resizeList(size_t cnt);

  template <class T>
  inline bool setListValue(size_t i, size_t f, T value);

  template <class T>
  T getMeta(std::string const& key, T const& defaultval) const
  {
    if(auto itr=meta_.find(key); itr!=meta_.end()) {
      if (std::holds_alternative<T>(itr->second)) {
        return std::get<T>(itr->second);
      } else {
        fprintf(stderr, "bad type held in %s: %zu\n", key.c_str(), itr->second.index());
      }
    }
    return defaultval;
  }

  bool hasMeta(std::string const& key) const
  {
    return meta_.find(key) != meta_.end();
  }

  template <class T>
  bool tryGetMeta(std::string const& key, T* retValue)
  {
    if(auto itr=meta_.find(key); itr!=meta_.end()) {
      if (std::holds_alternative<T>(itr->second)) {
        *retValue = std::get<T>(itr->second);
        return true;
      }
    }
    return false;
  }

protected:
  friend class ParmSet;
  // setup function should only be called by ParmSet
  void setName(string name) { name_ = std::move(name); }
  void setPath(string path) { path_ = std::move(path); }
  void setUI(ui_type_enum type) {ui_type_ = type; }
  void setType(value_type_enum type) {expected_value_type_ = type;}
  void setAsField(value_type defaultValue) {
    ui_type_ = ui_type_enum::FIELD;
    value_ = defaultValue;
    default_ = defaultValue;
  }
  void setLabel(string label) { label_ = std::move(label); }
  void setMeta(string const& key, value_type value) { meta_[key] = std::move(value_); }
  template <class T>
  void setMeta(string const& key, T const& value) {
    meta_[key].emplace<T>(value);
  }
  void setMenu(std::vector<string> items, int defaultValue, std::vector<string> labels={}, std::vector<int> values={}) {
    ui_type_ = ui_type_enum::MENU;
    menu_items_ = std::move(items);
    value_ = defaultValue;
    default_ = defaultValue;

    if (labels.size() == menu_items_.size())
      menu_labels_ = std::move(labels);
    else {
      menu_labels_.resize(menu_items_.size());
      std::transform(menu_items_.begin(), menu_items_.end(), menu_labels_.begin(), titleize);
    }

    if (values.size() == menu_items_.size())
      menu_values_ = std::move(values);
    else {
      menu_values_.resize(menu_values_.size());
      std::iota(menu_values_.begin(), menu_values_.end(), 0);
    }
  }
  void setup(string name, string path, string label, ui_type_enum ui, value_type_enum type, value_type defaultValue) {
    if (defaultValue.index() != static_cast<size_t>(type)) {
      throw std::invalid_argument("default value does not match type");
    }
    name_    = std::move(name);
    path_    = std::move(path);
    label_   = std::move(label);
    ui_type_ = ui;
    expected_value_type_ = type;
    default_ = defaultValue;
    value_   = defaultValue;
  }
  void addField(ParmPtr child) {
    fields_.push_back(child);
    if (!listValues_.empty()) {
      for (auto item: listValues_) {
        item->fields_.push_back(std::make_shared<Parm>(*child));
      }
    }
  }
  /* TODO: runtime adjusts on fields
  void removeField(size_t idx) {
  }
  */
};

class LoadError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class ParmSet
{
protected:
  ParmPtr              root_;
  std::vector<ParmPtr> parms_;
  bool                 loaded_ = false;
  hashset<string>      dirtyEntries_;

  static lua_State *defaultLuaRuntime(); // shared lua runtime for parmscript parsing and `disablewhen` expression evaluation

  static int processLuaParm(lua_State* lua);
  static int pushParmValueToLuaStack(lua_State* L, ParmPtr parm);
  static int evalParm(lua_State* lua);

  friend class Parm;
  friend class ParmSetInspector;

public:
  /// enable lua to call ParmSet functions
  ///
  /// example:
  /// ```lua
  /// local ParmSet = require('ParmSet')
  /// local parms = ParmSet.new()
  /// parms:loadParmScript(...)
  /// parms:updateInspector()
  /// ```
  static void exposeToLua(lua_State* lua);

  static string& preloadScript();

  void loadScript(std::string_view script, lua_State* runtime=nullptr); // if no lua runtime was given, default shared lua runtime will be used

  bool loaded() const { return loaded_; }

  ParmPtr get(string const& key) {
    if (key=="") return root_;
    return root_->getField(key);
  }
  ConstParmPtr const get(string const& key) const {
    if (key == "") return root_;
    return std::const_pointer_cast<const Parm>(root_->getField(key));
  }
  auto const& dirtyEntries() const { return dirtyEntries_; }
  void clearDirtyEntries() { dirtyEntries_.clear(); }

  auto operator[](string const& key) { return get(key); }
  auto operator[](string const& key) const { return get(key); }
};

template <class T>
inline bool Parm::set(T value)
{
  if constexpr (std::is_same_v<T, int>) {
    if (ui_type_ == ui_type_enum::MENU) {
      if (menu_values_.size() == menu_items_.size()) {
        auto itr = std::find(menu_values_.begin(), menu_values_.end(), value);
        if (itr == menu_values_.end())
          value_ = 0;
        else
          value_ = int(itr - menu_values_.begin());
      } else {
        value_ = value;
      }
    } else {
      value_ = value;
    }
  } else if (auto* ptr = std::get_if<T>(&value_)) {
    root_->dirtyEntries_.insert(path());
    *ptr = std::move(value);
    return true;
  }
  return false;
}

template <class T>
inline bool Parm::setListValue(size_t i, size_t f, T value)
{
  root_->dirtyEntries_.insert(path());
  return listValues_.at(i)->fields_.at(f)->set(std::move(value));
}

} // namespace parmscript

