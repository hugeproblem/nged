#include "parmscript.h"

#include <array>
#include <memory>

extern "C" {
#include <stdint.h>
#include <lua.h>
#include <lauxlib.h>
}

#include <sol/sol.hpp>

static const char parmexpr_src[] = {
#include <parmexpr.lua.h>
};

#ifdef DEBUG
#include <stdio.h>
#define WARN(...) fprintf(stderr, __VA_ARGS__)
#define INFO(...) fprintf(stdout, __VA_ARGS__)
#else
#define WARN(...) /*nothing*/
#define INFO(...) /*nothing*/
#endif

namespace parmscript {

ParmPtr Parm::getField(string const& relpath) {
  if (auto dot=relpath.find('.'); dot!=string::npos) {
    if (auto f=getField(relpath.substr(0, dot)))
      return f->getField(relpath.substr(dot+1));
    else
      return nullptr;
  } else {
    string childname = relpath;
    auto idxstart = relpath.find('[');
    int idx = -1;
    if (idxstart!=string::npos) {
      auto idxend = relpath.find(']');
      if (idxend==string::npos)
        return nullptr;
      string idxstr = relpath.substr(idxstart+1, idxend-idxstart-1);
      idx = std::stoi(idxstr);
      childname = relpath.substr(0, idxstart);
    }
    if (auto f = std::find_if(fields_.begin(), fields_.end(), [&childname](ParmPtr p){
        return p->name()==childname;
      });
      f!=fields_.end()) {
      if (idxstart!=string::npos) {
        if (idx>=0 && idx<(*f)->listValues_.size()) {
          return (*f)->listValues_[idx];
        }
      } else {
        return *f;
      }
    }
    for (auto f : fields_) {
      if (f->ui()==ui_type_enum::GROUP) { // group members are in current namespace
        if (auto gf = f->getField(relpath))
          return gf;
      }
    }
  }
  return nullptr;
}

void Parm::resizeList(size_t cnt)
{
  root_->dirtyEntries_.insert(path());
  if (auto oldsize=listValues_.size(); oldsize<cnt) {
    for (auto i=oldsize; i<cnt; ++i) {
      auto newItem = std::make_shared<Parm>(root_);
      string indexstr = "["+std::to_string(i)+"]";
      newItem->setUI(ui_type_enum::STRUCT);
      newItem->setPath(path_+indexstr);
      listValues_.push_back(newItem);
      for (auto f: fields_) {
        auto newField = std::make_shared<Parm>(*f);
        newField->setPath(newItem->path()+"."+f->name());
        newField->setLabel(newField->label()+indexstr);
        newItem->fields_.push_back(newField);
      }
    }
  } else {
    listValues_.resize(cnt);
  }
}

//////////////////////// ParmSet /////////////////////////////

string& ParmSet::preloadScript()
{
  static string script_;
  return script_;
}

int ParmSet::processLuaParm(lua_State* lua)
{
  sol::state_view L{lua};
  auto self = sol::stack::get<ParmSet*>(lua, 1);
  auto parentid = sol::stack::get<int>(lua, 2);
  auto field = sol::stack::get<sol::table>(L, 3);
  if (parentid<0 || !field) {
    WARN("bad argument passed to processLuaParm()\n");
    return 0;
  }
  string ui   = field["ui"];
  string path = field["path"];
  string name = field["name"];
  string type = field["type"];
  sol::table meta = field["meta"];
  string label = meta["label"].get_or(Parm::titleize(name));
  auto  defaultfield = meta["default"];
  Parm::value_type defaultval;
  INFO("processing field label(%s) ui(%s) type(%s) name(%s) ... ", label.c_str(), ui.c_str(), type.c_str(), name.c_str());

  auto parent = self->parms_[parentid];
  auto newparm = std::make_shared<Parm>(Parm(self));

  Parm::ui_type_enum uitype = Parm::ui_type_enum::FIELD;
  if (ui=="label")          uitype=Parm::ui_type_enum::LABEL;
  else if (ui=="separator") uitype=Parm::ui_type_enum::SEPARATOR;
  else if (ui=="spacer")    uitype=Parm::ui_type_enum::SPACER;
  else if (ui=="button")    uitype=Parm::ui_type_enum::BUTTON;
  else if (ui=="menu")      uitype=Parm::ui_type_enum::MENU;
  else if (ui=="group")     uitype=Parm::ui_type_enum::GROUP;
  else if (ui=="struct")    uitype=Parm::ui_type_enum::STRUCT;
  else if (ui=="list")      uitype=Parm::ui_type_enum::LIST;

  auto parseminmax = [&meta, &newparm](auto t) {
    using T = decltype(t);
    if (meta["min"].valid())
      newparm->setMeta<T>("min", meta["min"].get<T>());
    if (meta["max"].valid())
      newparm->setMeta<T>("max", meta["max"].get<T>());
    if (meta["speed"].valid())
      newparm->setMeta<T>("speed", meta["speed"].get<T>());
    if (!meta["ui"].valid() && meta["min"].valid() && meta["max"].valid())
      newparm->setMeta<string>("ui", "slider");
  };
  auto boolmeta = [&meta, &newparm](string const& key) {
    if (meta[key].valid())
      newparm->setMeta<bool>(key, meta[key].get<bool>());
  };
  auto strmeta = [&meta, &newparm](string const& key) {
    if (meta[key].valid())
      newparm->setMeta<string>(key, meta[key].get<string>());
  };

  INFO("1 ");

  Parm::value_type_enum valuetype = Parm::value_type_enum::NONE;
  if (type=="bool") {
    valuetype = Parm::value_type_enum::BOOL;
    defaultval = defaultfield.get_or(false);
  } else if (type=="int") {
    valuetype = Parm::value_type_enum::INT;
    defaultval = defaultfield.get_or(0);
    parseminmax(0);
  } else if (type=="int2") {
    valuetype = Parm::value_type_enum::INT2;
    auto vals = defaultfield.get_or(std::array<int, 2>{0,0});
    defaultval = Parm::int2{vals[0], vals[1]};
    parseminmax(0);
  } else if (type=="float") {
    valuetype = Parm::value_type_enum::FLOAT;
    defaultval = defaultfield.get_or(0.f);
    parseminmax(0.f);
  } else if (type=="float2") {
    valuetype = Parm::value_type_enum::FLOAT2;
    auto vals = defaultfield.get_or(std::array<float, 2>{0,0});
    defaultval = Parm::float2{vals[0], vals[1]};
    parseminmax(0.f);
  } else if (type=="float3") {
    valuetype = Parm::value_type_enum::FLOAT3;
    auto vals = defaultfield.get_or(std::array<float, 3>{0,0,0});
    defaultval = Parm::float3{vals[0], vals[1], vals[2]};
    parseminmax(0.f);
  } else if (type=="float4") {
    valuetype = Parm::value_type_enum::FLOAT4;
    auto vals = defaultfield.get_or(std::array<float, 4>{0,0,0});
    defaultval = Parm::float4{vals[0], vals[1], vals[2], vals[3]};
    parseminmax(0.f);
  } else if (type=="color") {
    valuetype = Parm::value_type_enum::COLOR;
    auto vals = defaultfield.get_or(std::array<float, 4>{1,1,1,1});
    defaultval = Parm::color{vals[0], vals[1], vals[2], vals[3]};
    boolmeta("alpha");
    boolmeta("hsv");
    boolmeta("hdr");
    boolmeta("wheel");
    boolmeta("picker");
  } else if (type=="string") {
    valuetype = Parm::value_type_enum::STRING;
    defaultval.emplace<string>(defaultfield.get_or(string("")));
    boolmeta("multiline");
  } else if (type=="double") {
    valuetype = Parm::value_type_enum::DOUBLE;
    defaultval.emplace<double>(defaultfield.get_or(0.0));
  }
  boolmeta("joinnext");
  if (meta["width"].valid())
    newparm->setMeta<int>("width", meta["width"].get<int>());
  strmeta("ui");
  strmeta("disablewhen");
  strmeta("font");
  // any other none-predefined meta:
  meta.for_each([&newparm](sol::object key, sol::object val) {
    auto strkey = key.as<string>();
    if (newparm->hasMeta(strkey))
      return;
    switch (val.get_type())
    {
    case sol::type::string:
      newparm->setMeta<string>(strkey, val.as<string>()); break;
    case sol::type::boolean:
      newparm->setMeta<bool>(strkey, val.as<bool>()); break;
    case sol::type::number:
      newparm->setMeta<double>(strkey, val.as<double>()); break;
    default:
      break;
    }
  });
  INFO("2 ");
  newparm->setup(name, path, label, uitype, valuetype, defaultval);

  if (uitype == Parm::ui_type_enum::MENU) {
    auto items  = meta["items"].get_or(std::vector<string>());
    auto labels = meta["itemlabels"].get_or(std::vector<string>());
    auto values = meta["itemvalues"].get_or(std::vector<int>());
    string nativedefault = "";
    if (!items.empty())
      nativedefault = items.front();
    std::string strdefault = defaultfield.get_or(nativedefault);
    auto itrdefault = std::find(items.begin(), items.end(), strdefault);
    int idxdefault = 0;
    if (itrdefault != items.end())
      idxdefault = itrdefault-items.begin();
    newparm->setMenu(items, idxdefault, labels, values);
  }
  INFO("3 ");
  parent->addField(newparm);
  self->parms_.push_back(newparm);
  int  newid = self->parms_.size()-1;
  sol::stack::push(lua, newid);
  INFO("done.\n");
  return 1;
}

int ParmSet::pushParmValueToLuaStack(lua_State* L, ParmPtr parm)
{
  if (parm->ui()==Parm::ui_type_enum::FIELD) {
    switch (parm->type()) {
    case Parm::value_type_enum::BOOL: 
      sol::stack::push<bool>(L, parm->as<bool>());
      break;
    case Parm::value_type_enum::INT:
      sol::stack::push<int>(L, parm->as<int>());
      break;
    case Parm::value_type_enum::FLOAT:
      sol::stack::push<float>(L, parm->as<float>());
      break;
    case Parm::value_type_enum::DOUBLE:
      sol::stack::push<float>(L, parm->as<double>());
      break;
    case Parm::value_type_enum::STRING:
      sol::stack::push<string>(L, parm->as<string>());
      break;
    case Parm::value_type_enum::COLOR: {
      auto c = parm->as<Parm::color>();
      sol::stack::push(L, std::array<float, 4>{c.r, c.g, c.b, c.a});
      break;
    }
    case Parm::value_type_enum::FLOAT2: {
      auto v = parm->as<Parm::float2>();
      sol::stack::push(L, std::array<float, 2>{v.x, v.y});
      break;
    }
    case Parm::value_type_enum::FLOAT3: {
      auto v = parm->as<Parm::float3>();
      sol::stack::push(L, std::array<float, 3>{v.x, v.y, v.z});
      break;
    }
    case Parm::value_type_enum::FLOAT4: {
      auto v = parm->as<Parm::float4>();
      sol::stack::push(L, std::array<float, 4>{v.x, v.y, v.z, v.w});
      break;
    }
    default:
      WARN("evalParm: type not supported\n");
      return 0;
    }
    return 1;
  } else if (parm->ui()==Parm::ui_type_enum::MENU) {
    sol::stack::push<string>(L, parm->as<string>());
    return 1;
  } else if (parm->ui()==Parm::ui_type_enum::STRUCT) {
    lua_createtable(L, 0, parm->numFields());
    for (int f=0, nf=parm->numFields(); f<nf; ++f) {
      if (pushParmValueToLuaStack(L, parm->fields_[f]) == 0)
        lua_pushnil(L);
      lua_setfield(L, -2, parm->fields_[f]->name().c_str());
    }
    return 1;
  } else if (parm->ui()==Parm::ui_type_enum::LIST) {
    lua_createtable(L, parm->numListValues(), 0);
    for (int i=0, n=parm->numListValues(); i<n; ++i) {
      pushParmValueToLuaStack(L, parm->listValues_[i]);
      lua_seti(L, -2, i+1);
    }
    return 1;
  } else {
    WARN("don\'t known how to handle parm \"%s\" of type %d\n", parm->name().c_str(), static_cast<int>(parm->ui()));
  }
  return 0;
}

int ParmSet::evalParm(lua_State* L)
{
  sol::state_view lua{L};
  auto optself = sol::stack::check_get<ParmSet*>(L, 1);
  auto optexpr = sol::stack::check_get<string>(L, 2);
  if (!optself.has_value() || !optexpr.has_value()) {
    return luaL_error(L, "wrong arguments passed to ParmSet:evalParm, expected (ParmSet, string)");
  }
  auto* self = optself.value();
  auto& expr = optexpr.value();

  if (expr.find("menu:")==0) {
    expr = expr.substr(5);
    auto sep = expr.find("::");
    if (sep == string::npos)
      return 0;
    auto path = expr.substr(0, sep);
    auto name = expr.substr(sep+2);
    if (auto parm = self->get(path)) {
      // just a check
      if (parm->ui() != Parm::ui_type_enum::MENU)
        return 0;
      sol::stack::push<string>(L, name);
      return 1;
    } else {
      return 0;
    }
  } else if (expr.find("length:")==0) {
    expr = expr.substr(7);
    if (auto parm = self->get(expr)) {
      if (parm->ui() == Parm::ui_type_enum::LIST) {
        sol::stack::push<size_t>(L, parm->numListValues());
        return 1;
      }
    }
    return 0;
  }

  if (auto parm = self->get(expr)) {
    return pushParmValueToLuaStack(L, parm);
  }
  WARN("evalParm: \"%s\" cannot be evaluated\n", expr.c_str());
  return 0;
}

void ParmSet::loadScript(std::string_view sv, lua_State* L)
{
  loaded_ = false;
  if (L == nullptr)
    L = defaultLuaRuntime();
  int luasp = lua_gettop(L); // stack pointer
  if (LUA_OK != luaL_loadbufferx(L, parmexpr_src, sizeof(parmexpr_src)-1, "parmexpr", "t")) {
    throw LoadError("failed to load parmexpr\n");
    return;
  }
  if (LUA_OK != lua_pcall(L, 0, 1, 0)) {
    lua_settop(L, luasp);
    throw LoadError("failed to call parmexpr\n");
    return;
  }
  auto fullscript = preloadScript();
  fullscript += "\n";
  fullscript += sv;
  lua_pushlstring(L, fullscript.data(), fullscript.size());
  if (LUA_OK != lua_pcall(L, 1, 1, 0)) {
    std::string message = luaL_optstring(L, -1, "unknown");
    lua_settop(L, luasp);
    throw LoadError(message);
    return;
  }
  root_  = std::make_shared<Parm>(nullptr);
  root_->setUI(Parm::ui_type_enum::STRUCT);
  parms_ = {root_};

  sol::state_view lua{L};
  auto loaded = lua.load(R"LUA(
local parmscript, cpp, process = ...
local function dofield(cpp, parentid, field)
  local id = process(cpp, parentid, field)
  if field.fields and #field.fields > 0 then
    for _, v in pairs(field.fields) do
      dofield(cpp, id, v)
    end
  end
end

for _,v in pairs(parmscript.root.fields) do
  dofield(cpp, 0, v)
end
  )LUA");
  if (loaded.valid()) {
    lua_pushvalue(L, -2); // return value of last `pcall` left on stack, which is the parmscript object
    sol::stack::push(L, this);
    lua_pushcfunction(L, processLuaParm);
    if (LUA_OK != lua_pcall(L, 3, 0, 0)) {
      std::string message = luaL_optstring(L, -1, "unknown");
      lua_settop(L, luasp);
      throw LoadError(message);
      return;
    } else {
      // done. pop the parmscript object from stack
      loaded_ = true;
      lua_pop(L, 1);
    }
  } else {
    throw LoadError("failed to load finalizing script\n");
  }
  loaded_ = true;
}

void ParmSet::exposeToLua(lua_State *L)
{
  lua_CFunction luaopen_parmset = [](lua_State *L)->int {
    sol::state_view lua{L};
    auto ut = lua.new_usertype<ParmSet>("ParmSet");
    ut.set("loadScript", [](lua_State *L)->int{
      auto optself = sol::stack::check_get<ParmSet*>(L, 1);
      auto optsrc  = sol::stack::check_get<string>(L, 2);
      if (!optself.has_value() || !optsrc.has_value()) {
        return luaL_error(L, "wrong arguments passed to ParmSet:loadScript, expected (ParmSet, string)");
      }
      if (lua_gettop(L)>2) {
        WARN("extra arguments passed to ParmSet:loadScript are discarded\n");
      }
      try {
        optself.value()->loadScript(optsrc.value(), L);
      } catch (std::exception const& e) {
        return luaL_error(L, "failed to load: %s", e.what());
      }
      return 0;
    });
    //ut.set("updateInspector", [](lua_State *L)->int{
    //  auto optself = sol::stack::check_get<ParmSet*>(L, 1);
    //  if (!optself.has_value()) {
    //    return luaL_error(L, "wrong arguments passed to ParmSet:loadScript, expected (ParmSet)");
    //  }
    //  if (lua_gettop(L)>1) {
    //    WARN("extra arguments passed to ParmSet:updateInspector are discarded\n");
    //  }
    //  sol::stack::push(L, optself.value()->updateInspector(L));
    //  return 1;
    //});
    //ut.set("dirtyEntries", [](lua_State *L)->int{
    //  auto optself = sol::stack::check_get<ParmSet*>(L, 1);
    //  if (!optself.has_value()) {
    //    return luaL_error(L, "wrong arguments passed to ParmSet:dirtyEntries, expected (ParmSet)");
    //  }
    //  auto const& dirty = optself.value()->dirtyEntries();
    //  if (dirty.empty())
    //    return 0;
    //  lua_createtable(L, dirty.size(), 0);
    //  lua_Integer i = 1;
    //  for (auto& s: dirty) {
    //    lua_pushlstring(L, s.c_str(), s.size());
    //    lua_seti(L, -2, i++);
    //  }
    //  return 1;
    //});
    ut.set(sol::meta_function::index, evalParm);
    sol::stack::push(L, ut);
    return 1;
  };

  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  lua_pushcfunction(L, luaopen_parmset);
  lua_setfield(L, -2, "ParmSet");
}

lua_State* ParmSet::defaultLuaRuntime()
{
  class LuaRAII {
    lua_State* lua_=nullptr;
  public:
    LuaRAII() {
      lua_ = luaL_newstate();
      luaL_openlibs(lua_);
    }
    ~LuaRAII() {
      lua_close(lua_);
    }
    lua_State* lua() const {
      return lua_;
    }
  };
  static LuaRAII instance;
  return instance.lua();
}

} // namespace parmscript

