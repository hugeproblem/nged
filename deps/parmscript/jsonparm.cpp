#include "parmscript.h"
#include "jsonparm.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <array>

namespace parmscript {

void from_json(nlohmann::json const& j, Parm& p)
{
  if (p.ui() == Parm::ui_type_enum::FIELD) {
    switch (p.type()) {
      case Parm::value_type_enum::BOOL:
        p.set(j.get<bool>());
        break;
      case Parm::value_type_enum::INT:
        p.set(j.get<int>());
        break;
      case Parm::value_type_enum::FLOAT:
        p.set(j.get<float>());
        break;
      case Parm::value_type_enum::DOUBLE:
        p.set(j.get<double>());
        break;
      case Parm::value_type_enum::STRING:
        p.set(j.get<std::string>());
        break;
      case Parm::value_type_enum::INT2: {
        std::vector<int> values = j.get<std::vector<int>>();
        if (values.size() != 2)
          throw std::runtime_error("Invalid float2 value");
        Parm::int2 i2 = { values[0], values[1] };
        p.set(i2);
        break;
      }
      case Parm::value_type_enum::FLOAT2: {
        std::vector<float> values = j.get<std::vector<float>>();
        if (values.size() != 2)
          throw std::runtime_error("Invalid float2 value");
        Parm::float2 f2 = { values[0], values[1] };
        p.set(f2);
        break;
      }
      case Parm::value_type_enum::FLOAT3: {
        std::vector<float> values = j.get<std::vector<float>>();
        if (values.size() != 3)
          throw std::runtime_error("Invalid float3 value");
        Parm::float3 f3 = { values[0], values[1], values[2] };
        p.set(f3);
        break;
      }
      case Parm::value_type_enum::FLOAT4: {
        std::vector<float> values = j.get<std::vector<float>>();
        if (values.size() != 4)
          throw std::runtime_error("Invalid float4 value");
        Parm::float4 f4 = { values[0], values[1], values[2], values[3] };
        p.set(f4);
        break;
      }
      case Parm::value_type_enum::COLOR: {
        std::vector<float> values = j.get<std::vector<float>>();
        if (values.size() != 4)
          throw std::runtime_error("Invalid color value");
        Parm::color c = { values[0], values[1], values[2], values[3] };
        p.set(c);
        break;
      }
      default:
        throw std::runtime_error("Invalid parameter type");
    }
  } else if (p.ui() == Parm::ui_type_enum::STRUCT) {
    if (!j.is_object())
      throw std::runtime_error("Invalid struct value");
    for (size_t numfields = p.numFields(), i = 0; i < numfields; ++i) {
      ParmPtr field = p.getField(i);
      if (!j.contains(field->name())) {
        // TODO: warn about missing field
        continue;
      }
      try {
        from_json(j[field->name()], *field);
      } catch (std::exception const& e) {
        continue; // skip invalid fields
      }
    }
  } else if (p.ui() == Parm::ui_type_enum::LIST) {
    if (!j.is_array())
      throw std::runtime_error("Invalid list value");
    p.resizeList(j.size());
    for (size_t numitems = p.numListValues(), i = 0; i < numitems; ++i) {
      ParmPtr item = p.at(i);
      try {
        from_json(j[i], *item);
      } catch (std::exception const& e) {
        continue; // skip invalid index
      }
    }
  } else if (p.ui() == Parm::ui_type_enum::MENU) {
    p.set<int>(j.get<int>());
  }
}

//////////////////////////////////////////////////////////////////////////////////////

void to_json(nlohmann::json& j, Parm const& p)
{
  if (p.ui() == Parm::ui_type_enum::FIELD) {
    switch (p.type()) {
      case Parm::value_type_enum::BOOL:
        j = p.as<bool>();
        break;
      case Parm::value_type_enum::INT:
        j = p.as<int>();
        break;
      case Parm::value_type_enum::FLOAT:
        j = p.as<float>();
        break;
      case Parm::value_type_enum::DOUBLE:
        j = p.as<double>();
        break;
      case Parm::value_type_enum::STRING:
        j = p.as<std::string>();
        break;
      case Parm::value_type_enum::INT2: {
        Parm::int2 i2 = p.as<Parm::int2>();
        j = std::array<int, 2>{ i2.x, i2.y };
        break;
      }
      case Parm::value_type_enum::FLOAT2: {
        Parm::float2 f2 = p.as<Parm::float2>();
        j = std::array<float, 2>{ f2.x, f2.y };
        break;
      }
      case Parm::value_type_enum::FLOAT3: {
        Parm::float3 f3 = p.as<Parm::float3>();
        j = std::array<float, 3>{ f3.x, f3.y, f3.z };
        break;
      }
      case Parm::value_type_enum::FLOAT4: {
        Parm::float4 f4 = p.as<Parm::float4>();
        j = std::array<float, 4>{ f4.x, f4.y, f4.z, f4.w };
        break;
      }
      case Parm::value_type_enum::COLOR: {
        Parm::color c = p.as<Parm::color>();
        j = std::array<float, 4>{ c.r, c.g, c.b, c.a };
        break;
      }
      default:
        throw std::runtime_error("Invalid parameter type");
    }
  } else if (p.ui() == Parm::ui_type_enum::STRUCT) {
    j = nlohmann::json::object();
    for (size_t numfields = p.numFields(), i = 0; i < numfields; ++i) {
      ParmPtr field = p.getField(i);
      try {
        to_json(j[field->name()], *field);
      } catch (std::exception const& e) {
        continue;
      }
    }
  } else if (p.ui() == Parm::ui_type_enum::LIST) {
    j = nlohmann::json::array();
    for (size_t numitems = p.numListValues(), i = 0; i < numitems; ++i) {
      try {
        auto item = p.at(i);
        to_json(j.emplace_back(), *item);
      } catch (std::exception const& e) {
        continue;
      }
    }
  } else if (p.ui() == Parm::ui_type_enum::MENU) {
    j = p.as<int>();
  }
}

//////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, ParmSet& p)
{
  if (auto root = p.get(""))
    from_json(j, *root);
}

void to_json(nlohmann::json& j, ParmSet const& p)
{
  if (auto root = p.get(""))
    to_json(j, *root);
}

} // namespace jsoncpp

