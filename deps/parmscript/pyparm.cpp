#include "pyparm.h"
#include "parmscript.h"
#include "parminspector.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace parmscript;

// clang-format off
py::object parmToPy(ParmPtr parm)
{
  if (parm->ui() == Parm::ui_type_enum::LIST) {
    py::list list;
    for (size_t i=0, n=parm->numListValues(); i<n; ++i) {
      list.append(parmToPy(parm->at(i)));
    }
    return list;
  } else if (parm->ui() == Parm::ui_type_enum::STRUCT) {
    py::dict map;
    for (size_t i=0, n=parm->numFields(); i<n; ++i) {
      auto const ui = parm->getField(i)->ui();
      if (ui == Parm::ui_type_enum::LABEL ||
          ui == Parm::ui_type_enum::SPACER ||
          ui == Parm::ui_type_enum::SEPARATOR)
        continue;
      map[parm->getField(i)->name().c_str()] = parmToPy(parm->getField(i));
    }
    return map;
  } else if (parm->ui() == Parm::ui_type_enum::FIELD) {
    switch (parm->type())
    {
    case Parm::value_type_enum::BOOL:
      return py::bool_(parm->as<bool>());
    case Parm::value_type_enum::INT:
      return py::int_(parm->as<int>());
    case Parm::value_type_enum::FLOAT:
      return py::float_(parm->as<float>());
    case Parm::value_type_enum::DOUBLE:
      return py::float_(parm->as<double>());
    case Parm::value_type_enum::STRING:
      return py::str(parm->as<std::string>());
    case Parm::value_type_enum::INT2: {
      auto v = parm->as<Parm::int2>();
      return py::make_tuple(v.x, v.y);
    }
    case Parm::value_type_enum::FLOAT2: {
      auto v = parm->as<Parm::float2>();
      return py::make_tuple(v.x, v.y);
    }
    case Parm::value_type_enum::FLOAT3: {
      auto v = parm->as<Parm::float3>();
      return py::make_tuple(v.x, v.y, v.z);
    }
    case Parm::value_type_enum::FLOAT4: {
      auto v = parm->as<Parm::float4>();
      return py::make_tuple(v.x, v.y, v.z, v.w);
    }
    case Parm::value_type_enum::COLOR: {
      auto v = parm->as<Parm::color>();
      return py::make_tuple(v.r, v.g, v.b, v.a);
    }
    default:
      return py::none();
    }
  } else if (parm->ui() == Parm::ui_type_enum::MENU) {
    return py::int_(parm->as<int>());
  }
  return py::none();
}

void parmFromPy(ParmPtr parm, py::object val)
{
  if (parm->ui() == Parm::ui_type_enum::LIST) {
    if (py::isinstance<py::list>(val)) {
      auto listval = val.cast<py::list>();
      parm->resizeList(listval.size());
      for(size_t i=0, n=listval.size(); i<n; ++i) {
        parmFromPy(parm->at(i), listval[i]);
      }
    } else {
      throw py::type_error(parm->name() + " is a list, assign it with another list");
    }
  } else if (parm->ui() == Parm::ui_type_enum::STRUCT) {
    if (py::isinstance<py::dict>(val)) {
      auto map = val.cast<py::dict>();
      for (size_t i=0, n=parm->numFields(); i<n; ++i) {
        auto field = parm->getField(i);
        if (map.contains(field->name().c_str())) {
          parmFromPy(field, map[field->name().c_str()]);
        }
      }
    }
  } else if (parm->ui() == Parm::ui_type_enum::FIELD) {
    switch (parm->type())
    {
    case Parm::value_type_enum::BOOL:
      parm->set(val.cast<bool>());
      break;
    case Parm::value_type_enum::INT:
      parm->set(val.cast<int>());
      break;
    case Parm::value_type_enum::FLOAT:
      parm->set(val.cast<float>());
      break;
    case Parm::value_type_enum::DOUBLE:
      parm->set(val.cast<double>());
      break;
    case Parm::value_type_enum::STRING:
      parm->set(val.cast<std::string>());
      break;
    case Parm::value_type_enum::INT2: {
      auto v = val.cast<py::tuple>();
      parm->set(Parm::int2{v[0].cast<int>(), v[1].cast<int>()});
      break;
    }
    case Parm::value_type_enum::FLOAT2: {
      auto v = val.cast<py::tuple>();
      parm->set(Parm::float2{v[0].cast<float>(), v[1].cast<float>()});
      break;
    }
    case Parm::value_type_enum::FLOAT3: {
      auto v = val.cast<py::tuple>();
      parm->set(Parm::float3{v[0].cast<float>(), v[1].cast<float>(), v[2].cast<float>()});
      break;
    }
    case Parm::value_type_enum::FLOAT4: {
      auto v = val.cast<py::tuple>();
      parm->set(Parm::float4{v[0].cast<float>(), v[1].cast<float>(), v[2].cast<float>(), v[3].cast<float>()});
      break;
    }
    case Parm::value_type_enum::COLOR: {
      auto v = val.cast<py::tuple>();
      parm->set(Parm::color{v[0].cast<float>(), v[1].cast<float>(), v[2].cast<float>(), v[3].cast<float>()});
      break;
    }
    default:
      break;
    }
  }
}

void bindParmToPython(pybind11::module_& m)
{
  py::class_<Parm, ParmPtr>(m, "Parm")
    .def("value", &parmToPy)
    .def("set", &parmFromPy)
    .def("fieldNames", [](ParmPtr parm)->py::object{
      py::list keys;
      if (parm->ui() == Parm::ui_type_enum::STRUCT || parm->ui() == Parm::ui_type_enum::LIST) {
        for (size_t i=0, n=parm->numFields(); i<n; ++i) {
          keys.append(parm->getField(i)->name());
        }
        return keys;
      }
      return py::none();
    })
    .def("field", py::overload_cast<std::string const&>(&Parm::getField), py::arg("name"))
    .def("__len__", &Parm::numListValues)
    .def("__getitem__", [](ParmPtr parm, size_t i) {
      if (parm->ui() == Parm::ui_type_enum::LIST) {
        return parm->at(i);
      }
      throw py::type_error("Parm is not a list or struct");
    });

  py::class_<ParmSet, ParmSetPtr>(m, "ParmSet")
    .def("load", [](ParmSetPtr ps, std::string const& script){
      ps->loadScript(script);
    })
    .def("get", py::overload_cast<std::string const&>(&ParmSet::get));

  py::class_<ParmSetInspector, std::shared_ptr<ParmSetInspector>>(m, "ParmSetInspector")
    .def_static("setFieldInspector", &ParmSetInspector::setFieldInspector, py::arg("name"), py::arg("inspect_function"))
    .def_static("addPreloadScript", [](std::string const& script){
      ParmSet::preloadScript() += script;
    }, py::arg("script"));
}
