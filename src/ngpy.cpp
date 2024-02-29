#include <nged/ngpy.h>

#include <nged/ngdoc.h>
#include <nged/nged.h>
#include <nged/nged_imgui.h>
#include <nged/style.h>
#include <nged/pybind11_imgui.h>

// from parmscript:
#include <jsonparm.h>
#include <pyparm.h>
#include <inspectorext.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/chrono.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include <iostream>

namespace py = pybind11;
using msghub = nged::MessageHub;

// clang-format off

// only types that will be used in c++ code
struct OurPyTypes
{
  py::object GraphItemType = py::none();

  static OurPyTypes& instance() {
    static std::unique_ptr<OurPyTypes> instance_ = std::make_unique<OurPyTypes>();
    return *instance_;
  }
};

// --------------------------------------- PyGraphItemFactory --------------------------------------------------
// PyGraphItemFactory {{{
nged::GraphItemPtr PyGraphItemFactory::make(nged::Graph* parent, nged::String const& name) const
{
  if (auto itr = pyFactories_.find(name); itr != pyFactories_.end()) {
    pybind11::gil_scoped_acquire gil;
    try {
      auto pyobj = itr->second.call(parent);
      if (auto itemptr = py::cast<nged::GraphItemPtr>(pyobj)) {
        if (auto* pyitem = dynamic_cast<PyGraphItem*>(itemptr.get())) {
          pyitem->factory_ = -9;
          pyitem->pyFactoryName_ = name;
        }
        if (parent && parent->docRoot()) {
          assert(dynamic_cast<PyNodeGraphDoc*>(parent->docRoot()));
          if (auto* pydoc = static_cast<PyNodeGraphDoc*>(parent->docRoot()))
            pydoc->pyObjects.insert(pyobj);
          return itemptr;
        }
      } else {
        throw py::type_error(std::string("expected nged::GraphItem being returned by ") + std::string(itr->second.str()));
      }
    } catch (std::exception const& e) {
      msghub::errorf("failed to create item {}: {}", name, e.what());
      return nullptr;
    }
  } else {
    return nged::GraphItemFactory::make(parent, name);
  }
  return nullptr;
}

nged::String PyGraphItemFactory::factoryName(nged::GraphItemPtr item) const
{
  if (auto* pyitem = dynamic_cast<PyGraphItem*>(item.get())) {
    return pyitem->pyFactoryName_;
  } else {
    return nged::GraphItemFactory::factoryName(std::move(item));
  }
}

nged::Vector<nged::String> PyGraphItemFactory::listNames(bool onlyUserCreatable) const
{
  auto names = nged::GraphItemFactory::listNames(onlyUserCreatable);
  auto numNative = names.size();
  for (auto& [name, pyobj]: pyFactories_) {
    if (onlyUserCreatable && !pyUserCreatable_.at(name))
      continue;
    names.push_back(name);
  }
  std::sort(names.begin()+numNative, names.end()); // only sort py-factory names
  return names;
}

void PyGraphItemFactory::discard(nged::Graph* graph, nged::GraphItem* item) const
{
  if (graph && graph->docRoot()) {
    auto pyhandle = py::detail::get_object_handle(item, py::detail::get_type_info(typeid(PyGraphItem)));
    if (!pyhandle)
      return;
    assert(dynamic_cast<PyNodeGraphDoc*>(graph->docRoot()));
    if (auto* pydoc = static_cast<PyNodeGraphDoc*>(graph->docRoot()))
      pydoc->pyObjects.erase(py::object(pyhandle, true));
  }
}
// }}}

// --------------------------------------- PyNode --------------------------------------------------
// PyNode {{{
void PyNode::setExtraParms(std::string extraParms)
{
  if (extraParms == this->extraParms)
    return;
  auto parmDesc = def->parms + "\n" + extraParms;
  auto newparms = std::make_unique<parmscript::ParmSet>();
  try {
    newparms->loadScript(parmDesc);
    nged::Json oldvalues;
    to_json(oldvalues, parmInspector.parms());
    from_json(oldvalues, *newparms);
    parmInspector.setParms(std::move(newparms));
    this->extraParms = extraParms;
  } catch (std::exception const& e) {
    msghub::errorf("failed to load parmscript: {}", e.what());
  }
}

bool PyNode::serialize(nged::Json& json) const
{
  if (!nged::Node::serialize(json))
    return false;
  if (auto* graph = asGraph()) {
    if (!graph->serialize(json))
      return false;
  }

  if (!parmInspector.empty())
    to_json(json["parms"], parmInspector.parms());
  else
    json["parms"] = nullptr;
  if (!extraParms.empty())
    json["extraParms"] = extraParms;
  try {
    pybind11::gil_scoped_acquire gil;
    auto pyserialize = pybind11::get_override(this, "serialize");
    if (pyserialize) {
      auto pystr = pyserialize();
      if (pystr.is_none())
        return false;
      json["pynode"] = pystr.cast<std::string>();
    } else {
      json["pynode"] = "";
    }
  } catch (std::exception const& e) {
    msghub::errorf("failed to serialize node {}: {}", name(), e.what());
    return false;
  }
  return true;
}

bool PyNode::deserialize(nged::Json const& json)
{
  if (!nged::Node::deserialize(json))
    return false;
  if (auto* graph = asGraph()) {
    if (!graph->deserialize(json))
      return false;
    else
      graph->rename(name());
  }

  extraParms = json.value("extraParms", "");
  if (!def->parms.empty() || !extraParms.empty()) {
    std::string parmDesc = def->parms + "\n" + extraParms;
    try {
      parmInspector.loadParmScript(parmDesc);
    } catch (std::exception const& e) {
      msghub::errorf("cannot load parm script: {}", e.what());
      parmInspector.setParms(nullptr);
    }
  } else {
    parmInspector.setParms(nullptr);
  }
  if (!parmInspector.empty())
    from_json(json["parms"], parmInspector.parms());
  try {
    pybind11::gil_scoped_acquire gil;
    auto pydeserialize = pybind11::get_override(this, "deserialize");
    if (pydeserialize) {
      auto pystr = json.value("pynode", "");
      pydeserialize(pystr);
    }
  } catch (std::exception const& e) {
    msghub::errorf("failed to deserialize node {}: {}", name(), e.what());
    return false;
  }

  return true;
}

void PyNode::settled()
{
  try {
    pybind11::gil_scoped_acquire gil;
    auto pyPostDeserialize = pybind11::get_override(this, "settled");
    if (pyPostDeserialize) {
      pyPostDeserialize();
    }
  } catch (std::exception const& e) {
    msghub::errorf("failed to call settled on node {}: {}", name(), e.what());
  }
}

size_t PyNode::getExtraDependencies(nged::Vector<nged::ItemID>& deps)
{
  deps.clear();
  try {
    pybind11::gil_scoped_acquire gil;
    auto pyGetExtraDeps = pybind11::get_override(this, "getExtraDependencies");
    if (pyGetExtraDeps) {
      auto result = pyGetExtraDeps();
      if (result && result != py::none())
        for (auto&& obj: result)
          deps.push_back(pybind11::cast<nged::ItemID>(obj));
    }
  } catch (std::exception const& e) {
    msghub::errorf("failed to call getExtraDependencies() on node {}: {}", name(), e.what());
  }
  return deps.size();
}
// }}}

// --------------------------------------- PyGraph --------------------------------------------------
// PyGraph {{{
bool PyGraph::serialize(nged::Json& json) const
{
  if (!Graph::serialize(json))
    return false;
  try {
    py::gil_scoped_acquire gil;
    auto pyserialize = py::get_override(this, "serialize");
    if (pyserialize) {
      auto pystr = pyserialize();
      json["pygraph"] = pystr.cast<std::string_view>();
    } else {
      json["pygraph"] = "";
    }
    return true;
  } catch (std::exception const& e) {
    msghub::errorf("error serilizing graph: {}", e.what());
    return false;
  }
}

bool PyGraph::deserialize(nged::Json const& json)
{
  if (!Graph::deserialize(json))
    return false;
  try {
    py::gil_scoped_acquire gil;
    auto pydeserialize = py::get_override(this, "deserialize");
    if (pydeserialize) {
      pydeserialize(json.value("pygraph", ""));
    }
    return true;
  } catch (std::exception const& e) {
    msghub::errorf("error deserilizing graph: {}", e.what());
    return false;
  }
}
// }}}

// --------------------------------------- PyNodeFactory --------------------------------------------------
// PyNodeFactory {{{
nged::GraphPtr PyNodeFactory::createRootGraph(nged::NodeGraphDoc* doc) const
{
  auto pyCreateRootGraph = py::get_override(this, "createRootGraph");
  if (pyCreateRootGraph) {
    try {
      auto pyroot = pyCreateRootGraph(doc);
      auto graph  = py::cast<nged::GraphPtr>(pyroot);
      static_cast<PyNodeGraphDoc*>(doc)->pyRootGraph = pyroot;
      return graph;
    } catch (std::exception const& e) {
      msghub::errorf("failed to create root graph:{}", e.what());
    }
  } else {
    msghub::error("NodeFactory::createRootGraph not implemented");
  }
  return nullptr;
 }

nged::NodePtr PyNodeFactory::createNode(nged::Graph* parent, nged::StringView type) const
{
  pybind11::gil_scoped_acquire gil;
  nged::String typestr(type);
  auto factoryItr = pyFactories_.find(typestr);
  if (factoryItr == pyFactories_.end() || !factoryItr->second) {
    pybind11::pybind11_fail(
      fmt::format("NodeFactory::createNode: type \"{}\" not found", type));
    return nullptr;
  }
  auto defItr = pyDescs_.find(typestr);
  if (defItr == pyDescs_.end() || !defItr->second) {
    pybind11::pybind11_fail(
      fmt::format("NodeFactory::createNode: type \"{}\" not found", type));
    return nullptr;
  }
  auto factory = factoryItr->second;
  auto def = defItr->second;
  try {
    auto pyobj = factory(parent, def);
    if (!pyobj) {
      msghub::errorf("NodeFactory::createNode: factory \"{}\" returned nullptr", type);
      return nullptr;
    }
    if (auto nodeptr = py::cast<nged::NodePtr>(pyobj)) {
      if (parent && parent->docRoot()) {
        assert(dynamic_cast<PyNodeGraphDoc*>(parent->docRoot()));
        if (auto* pydoc = static_cast<PyNodeGraphDoc*>(parent->docRoot()))
          pydoc->pyObjects.insert(pyobj);
      }
      return nodeptr;
    } else {
      pybind11::pybind11_fail("expected nged::Node being returned by NodeFactory::createNode");
      return nullptr;
    }
  } catch (std::exception const& e) {
    msghub::errorf("failed to create node {}: {}", type, e.what());
    return nullptr;
  }
}

void PyNodeFactory::listNodeTypes(
  nged::Graph* parent,
  void* context,
  void (*callback)(void* context, nged::StringView category, nged::StringView type, nged::StringView name)) const
{
  for (auto&& [name, def]: pyDescs_) {
    if (def->hidden)
      continue;
    callback(context, def->category, def->type, def->label);
  }
}

void PyNodeFactory::discard(nged::Graph* graph, nged::Node* node) const
{
  if (graph && graph->docRoot()) {
    auto pyhandle = py::detail::get_object_handle(node, py::detail::get_type_info(typeid(PyNode)));
    if (!pyhandle)
      return;
    assert(dynamic_cast<PyNodeGraphDoc*>(graph->docRoot()));
    if (auto* pydoc = static_cast<PyNodeGraphDoc*>(graph->docRoot()))
      pydoc->pyObjects.erase(py::object(pyhandle, true));
  }
}
// }}}

// --------------------------------------- Editor --------------------------------------------------
// NodeGraphEditor {{{
struct ImGuiNodeGraphEditorInitializer
{
  ImGuiNodeGraphEditorInitializer()
  {
    nged::addImGuiInteractions();
    nged::ImGuiResource::reloadFonts();
  }
};


PyImGuiNodeGraphEditor::PyImGuiNodeGraphEditor(py::object docFactory, py::object nodeFactory, py::object itemFactory):
  ImGuiNodeGraphEditor(),
  pyDocFactory(docFactory),
  pyNodeFactory(nodeFactory),
  pyItemFactory(itemFactory)
{
  setDocFactory([this](nged::NodeFactoryPtr nodeFactory, nged::GraphItemFactory const* itemFactory)->nged::NodeGraphDocPtr{
    auto pydoc = pyDocFactory(nodeFactory, itemFactory);
    if (!pydoc) return nullptr;
    auto docptr = py::cast<nged::NodeGraphDocPtr>(pydoc);
    if (!docptr) {
      throw py::type_error(std::string("expected Document being returned by ") + std::string(pyDocFactory.str()));
      return nullptr;
    }
    pyDocs.insert(pydoc);
    return docptr;
  });
  setNodeFactory(py::cast<nged::NodeFactoryPtr>(nodeFactory));
  auto itemFactoryCopy = std::make_shared<PyGraphItemFactory>(*py::cast<PyGraphItemFactory*>(itemFactory));
  nged::addImGuiItems(itemFactoryCopy);
  setItemFactory(itemFactoryCopy);
  dyingRefCount_ = 1; // pyDocs always holds one reference
}

PyImGuiNodeGraphEditor::~PyImGuiNodeGraphEditor()
{
  // clear python objects first
  pyDocFactory = py::none();
  pyNodeFactory = py::none();
  pyItemFactory = py::none();
  pyDocs.clear();
  pyCommands.clear();
  for (auto view: pyViews) {
    views_.erase(py::cast<nged::GraphView*>(view));
  }
  pyViews.clear();
}

void PyImGuiNodeGraphEditor::beforeDocRemoved(DocPtr doc)
{
  py::object pyobj(py::detail::get_object_handle(doc.get(), py::detail::get_type_info(typeid(PyNodeGraphDoc))), true);
  if (auto itr = pyDocs.find(pyobj); itr != pyDocs.end()) {
    msghub::infof("{} was removed", doc->title());
    pyDocs.erase(pyobj);
  } else {
    msghub::errorf("{} was not found in known docs", doc->title());
  }
}

void PyImGuiNodeGraphEditor::addCommand(py::object command)
{
  auto cmdptr = py::cast<nged::CommandManager::CommandPtr>(command);
  commandManager().add(cmdptr);
  pyCommands[cmdptr->name()] = command;
}

void PyImGuiNodeGraphEditor::removeCommand(nged::String const& name)
{
  commandManager().remove(name);
  pyCommands.erase(name);
}

void PyImGuiNodeGraphEditor::onParmModified(PyNode* node, parmscript::hashset<parmscript::string> const& parms)
{
  try {
    if (pyParmModifiedCallback) {
      py::gil_scoped_acquire gil;
      pyParmModifiedCallback(node, parms);
    }
  } catch (std::exception const& e) {
    msghub::errorf("cannot call onParmModified callback: {}", e.what());
  }
}

std::shared_ptr<PyImGuiNodeGraphEditor> createEditor(py::object docFactory, py::object nodeFactory, py::object itemFactory)
{
  static ImGuiNodeGraphEditorInitializer initOnce;
  auto editor = std::make_shared<PyImGuiNodeGraphEditor>(docFactory, nodeFactory, itemFactory);

  editor->setViewFactory(std::make_shared<PyViewFactory>());
  editor->initCommands();
  editor->setResponser(std::make_shared<PyResponser>());

  return editor;
}
//}}}

// Responser {{{
void PyResponser::onInspect(nged::InspectorView* view, nged::GraphItem** items, size_t count)
{
  bool  handled  = false;
  nged::Node* solyNode = nullptr;
  for (size_t i = 0; i < count; ++i) {
    if (auto* node = items[i]->asNode())
      if (solyNode) {
        solyNode = nullptr;
        break;
      } else {
        solyNode = node;
      }
  }
  if (auto* node = solyNode) {
    auto* pynode = static_cast<PyNode*>(node);
    bool inputModified = false;
    ParmFonts fonts = {
      nged::ImGuiResource::instance().sansSerifFont,
      nged::ImGuiResource::instance().monoFont
    };

    if (!pynode->parmInspector.empty())
      inputModified = pynode->parmInspector.inspect(nullptr, &fonts);

    if (inputModified) {
      static_cast<PyImGuiNodeGraphEditor*>(view->editor())->onParmModified(pynode, pynode->parmInspector.dirtyEntries());
    }

    if (pynode->parmInspector.doneEditing()) {
      view->graph()->docRoot()->history().commit("edit parm");
    }
 
    handled = true;
  }

  if (!handled) {
    DefaultImGuiResponser::onInspect(view, items, count);
  }

  if (pyOnInspectCallback && count != 0) {
    try {
      py::gil_scoped_acquire gil;
      py::tuple pyitems(count);
      for (size_t i=0; i<count; ++i)
        pyitems[i] = items[i];
      pyOnInspectCallback(view, pyitems);
    } catch (std::exception const& e) {
      msghub::errorf("failed to call onInspect callback: {}", e.what());
    }
  }

  if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    nged::NetworkView* netviewToFocus = nullptr;
    if (auto lv = view->linkedView()) {
      if (auto* netview = dynamic_cast<nged::NetworkView*>(lv.get()))
        netviewToFocus = netview;
    }
    if (!netviewToFocus && count > 0) {
      for (auto v : view->editor()->views()) {
        if (auto* netview = dynamic_cast<nged::NetworkView*>(v.get())) {
          if (!netviewToFocus)
            netviewToFocus = netview;
          else if (netview->graph().get() == items[0]->parent())
            netviewToFocus = netview;
        }
      }
    }
    if (netviewToFocus) {
      if (auto* imguiWindow = dynamic_cast<nged::ImGuiNamedWindow*>(netviewToFocus)) {
        msghub::infof("focusing window {}", imguiWindow->titleWithId());
        ImGui::SetWindowFocus(imguiWindow->titleWithId().c_str());
      }
    }
  }
}

bool PyResponser::beforeItemAdded(nged::Graph* graph, nged::GraphItem* item, nged::GraphItem** replacement)
{
  if (pyBeforeItemAddedCallback) {
    try {
      py::gil_scoped_acquire gil;
      auto ret = pyBeforeItemAddedCallback(graph, item);
      if (py::isinstance<py::bool_>(ret)) {
        *replacement = nullptr;
        return ret.cast<bool>(); 
      } else if (py::isinstance(ret, OurPyTypes::instance().GraphItemType)) {
        *replacement = ret.cast<nged::GraphItem*>();
        return true;
      } else if (py::isinstance<py::tuple>(ret)) {
        auto tp = ret.cast<py::tuple>();
        if (tp.size() != 2)
          throw std::range_error("beforeItemAdded() should return tuple(accept, replacement)");
        bool accept = tp[0].cast<bool>();
        nged::GraphItem* repl = tp[1].cast<nged::GraphItem*>();
        *replacement = repl;
        return accept;
      }
    } catch (std::exception const& e) {
      msghub::errorf("error calling beforeItemAdded: {}", e.what());
    }
  }
  return Base::beforeItemAdded(graph, item, replacement);
}

void PyResponser::afterItemAdded(nged::Graph* graph, nged::GraphItem* item)
{
  if (pyAfterItemAddedCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyAfterItemAddedCallback(graph, item);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling afterItemAdded: {}", e.what());
    }
  }
  Base::afterItemAdded(graph, item);
}

bool PyResponser::beforeItemRemoved(nged::Graph* graph, nged::GraphItem* item)
{
  if (pyBeforeItemRemovedCallback) {
    try {
      py::gil_scoped_acquire gil;
      return pybind11::cast<bool>(pyBeforeItemRemovedCallback(graph, item));
    } catch (std::exception const& e) {
      msghub::errorf("error calling beforeItemRemoved: {}", e.what());
    }
  }
  return Base::beforeItemRemoved(graph, item);
}

bool PyResponser::beforeNodeRenamed(nged::Graph* graph, nged::Node* node)
{
  if (pyBeforeNodeRenamedCallback) {
    try {
      py::gil_scoped_acquire gil;
      return pybind11::cast<bool>(pyBeforeNodeRenamedCallback(graph, node));
    } catch (std::exception const& e) {
      msghub::errorf("error calling beforeNodeRenamed: {}", e.what());
    }
  }
  return Base::beforeNodeRenamed(graph, node);
}

void PyResponser::afterNodeRenamed(nged::Graph* graph, nged::Node* node)
{
  if (pyAfterNodeRenamedCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyAfterNodeRenamedCallback(graph, node);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling afterNodeRenamed: {}", e.what());
    }
  }
  Base::afterNodeRenamed(graph, node);
}

void PyResponser::beforeViewUpdate(nged::GraphView* view)
{
  if (pyBeforeViewUpdateCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyBeforeViewUpdateCallback(view);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling beforeViewUpdate: {}", e.what());
    }
  }
  Base::beforeViewUpdate(view);
}

void PyResponser::afterViewUpdate(nged::GraphView* view)
{
  if (pyAfterViewUpdateCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyAfterViewUpdateCallback(view);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling afterViewUpdate: {}", e.what());
    }
  }
  Base::afterViewUpdate(view);
}

void PyResponser::beforeViewDraw(nged::GraphView* view)
{
  if (pyBeforeViewDrawCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyBeforeViewDrawCallback(view);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("failed calling beforeViewDraw: {}", e.what());
    }
  }
  Base::beforeViewDraw(view);
}

void PyResponser::afterViewDraw(nged::GraphView* view)
{
  if (pyAfterViewDrawCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyAfterViewDrawCallback(view);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling afterViewDraw: {}", e.what());
    }
  }
  Base::afterViewDraw(view);
}

void PyResponser::onItemClicked(nged::NetworkView* view, nged::GraphItem* item, int button)
{
  if (pyOnItemClickedCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyOnItemClickedCallback(view, item, button);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling onItemClicked: {}", e.what());
    }
  }
  Base::onItemClicked(view, item, button);
}

void PyResponser::onItemDoubleClicked(nged::NetworkView* view, nged::GraphItem* item, int button)
{
  if (pyOnItemDoubleClickedCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyOnItemDoubleClickedCallback(view, item, button);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling onItemDoubleClicked: {}", e.what());
    }
  }
  Base::onItemDoubleClicked(view, item, button);
}

void PyResponser::onItemHovered(nged::NetworkView* view, nged::GraphItem* item)
{
  if (pyOnItemHoveredCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyOnItemHoveredCallback(view, item);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling onItemHovered: {}", e.what());
    }
  }
  Base::onItemHovered(view, item);
}

void PyResponser::onSelectionChanged(nged::NetworkView* view)
{
  if (pyOnSelectionChangedCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyOnSelectionChangedCallback(view);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling onSelectionChanged: {}", e.what());
    }
  }
  Base::onSelectionChanged(view);
}

bool PyResponser::beforeLinkSet(nged::Graph* graph, nged::InputConnection src, nged::OutputConnection dst)
{
  if (pyBeforeLinkSetCallback) {
    try {
      py::gil_scoped_acquire gil;
      return pybind11::cast<bool>(pyBeforeLinkSetCallback(graph, src.sourceItem, src.sourcePort, dst.destItem, dst.destPort));
    } catch (std::exception const& e) {
      msghub::errorf("error calling beforeLinkSet: {}", e.what());
    }
  }
  return Base::beforeLinkSet(graph, src, dst);
}

void PyResponser::onLinkSet(nged::Link* link)
{
  if (pyOnLinkSetCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyOnLinkSetCallback(link);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling onLinkSet: {}", e.what());
    }
  }
  Base::onLinkSet(link);
}

void PyResponser::onLinkRemoved(nged::Link* link)
{
  if (pyOnLinkRemovedCallback) {
    try {
      py::gil_scoped_acquire gil;
      pyOnLinkRemovedCallback(link);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling onLinkRemoved: {}", e.what());
    }
  }
  Base::onLinkRemoved(link);
}

void PyResponser::afterPaste(nged::Graph* graph, nged::GraphItem** items, size_t count)
{
  if (pyAfterPasteCallback) {
    try {
      py::gil_scoped_acquire gil;
      py::list newitems;
      for(size_t i=0; i<count; ++i)
        newitems.append(items[i]);
      pyAfterPasteCallback(graph, newitems);
      return;
    } catch (std::exception const& e) {
      msghub::errorf("error calling afterPaste: {}", e.what());
    }
  }
  Base::afterPaste(graph, items, count);
}

void PyResponser::afterViewRemoved(nged::GraphView* view)
{
  auto* editor = view->editor();
  auto pyview = py::detail::get_object_handle(view, py::detail::get_type_info(typeid(PyGraphView)));
  if (pyview)
  {
    assert(dynamic_cast<PyImGuiNodeGraphEditor*>(editor));
    auto* pyeditor = static_cast<PyImGuiNodeGraphEditor*>(editor);
    pyeditor->pyViews.erase(py::object(pyview, true));
  }
}
// }}}

// ViewFactory {{{
PyViewFactory::PyViewFactory()
{
  fallback_ = nged::defaultViewFactory();
}

nged::GraphViewPtr PyViewFactory::createView(nged::String const& kind, nged::NodeGraphEditor* editor, nged::NodeGraphDocPtr doc) const 
{
  if (auto itr = pyFactories_.find(kind); itr != pyFactories_.end()) {
    try {
      auto pyview = itr->second(editor, doc);
      auto view = py::cast<nged::GraphViewPtr>(pyview);
      nged::ViewFactory::finalize(view.get(), kind, editor);
      static_cast<PyImGuiNodeGraphEditor*>(editor)->pyViews.insert(pyview);
      return view;
    } catch (std::exception const& e) {
      msghub::errorf("cannot create view of kind \"{}\": {}", kind, e.what());
    }
    return nullptr;
  } else {
    return fallback_->createView(kind, editor, doc);
  }
}
// }}}

// --------------------------------------- PyApp --------------------------------------------------
// PyApp {{{
void PyApp::init()
{
#ifdef _WIN32
  spdlog::set_default_logger(std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>()));
  spdlog::default_logger()->sinks().emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
  spdlog::set_default_logger(std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>()));
#endif
#ifdef DEBUG
  spdlog::set_level(spdlog::level::trace);
#else
  spdlog::set_level(spdlog::level::info);
#endif

  PYBIND11_OVERLOAD(
    void,
    nged::App,
    init);
}
// }}}

// --------------------------------------- The Python Module --------------------------------------------------

PYBIND11_MODULE(nged, m) {
  m.doc() = "NodeGraph Editor";

  parmscript::addExtensions();

  // Vec2 {{{
  py::class_<nged::Vec2>(m, "Vec2")
    .def(py::init<>())
    .def(py::init<nged::Vec2 const&>())
    .def(py::init<float, float>())
    .def_readwrite("x", &nged::Vec2::x)
    .def_readwrite("y", &nged::Vec2::y)
    .def(py::self+py::self)
    .def(py::self-py::self)
    .def(py::self*py::self)
    .def(py::self*float())
    .def(py::self/float())
    .def("__repr__", [](nged::Vec2 v){return fmt::format("Vec2({}, {})", v.x, v.y);});
  // }}}
  
  // AABB {{{
  py::class_<nged::AABB>(m, "AABB")
    .def(py::init<nged::Vec2, nged::Vec2>())
    .def_readwrite("min", &nged::AABB::min)
    .def_readwrite("max", &nged::AABB::max)
    .def("center", &nged::AABB::center)
    .def("size", &nged::AABB::size)
    .def("move", &nged::AABB::move, py::arg("offset"))
    .def("moved", &nged::AABB::moved, py::arg("offset"))
    .def("expand", &nged::AABB::expand, py::arg("amount"))
    .def("expanded", &nged::AABB::expanded, py::arg("amount"))
    .def("merge", py::overload_cast<nged::Vec2 const&>(&nged::AABB::merge), py::arg("pt"))
    .def("merge", py::overload_cast<nged::AABB const&>(&nged::AABB::merge), py::arg("aabb"))
    .def("contains", py::overload_cast<nged::Vec2 const&>(&nged::AABB::contains, py::const_), py::arg("pt"))
    .def("contains", py::overload_cast<nged::AABB const&>(&nged::AABB::contains, py::const_), py::arg("aabb"))
    .def("intersects", py::overload_cast<nged::AABB const&>(&nged::AABB::intersects, py::const_), py::arg("aabb"))
    .def("__repr__", [](nged::AABB v){
      return fmt::format("AABB(({}, {}), ({}, {}))", v.min.x, v.min.y, v.max.x, v.max.y);
    });
  // }}}

  // IconType {{{
  py::enum_<nged::IconType>(m, "IconType")
    .value("IconFont", nged::IconType::IconFont)
    .value("Text", nged::IconType::Text);
  // }}}

  // GraphItemState {{{
  py::enum_<nged::GraphItemState>(m, "GraphItemState")
    .value("DEFAULT", nged::GraphItemState::DEFAULT)
    .value("HOVERED", nged::GraphItemState::HOVERED)
    .value("SELECTED", nged::GraphItemState::SELECTED)
    .value("PRESSED", nged::GraphItemState::PRESSED)
    .value("DISABLED", nged::GraphItemState::DISABLED)
    .value("DESELECTED", nged::GraphItemState::DESELECTED);
  // }}}

  // ItemID {{{
  py::class_<nged::ItemID>(m, "ItemID")
    .def(py::init([]() { return nged::ID_None; }))
    .def(py::init<nged::ItemID>())
    .def_property_readonly("value", &nged::ItemID::value)
    .def(py::self == py::self)
    .def(py::self < py::self)
    .def("__eq__", [](nged::ItemID lhs, nged::ItemID rhs) { return lhs==rhs; })
    .def("__hash__", [](nged::ItemID v){ return std::hash<nged::ItemID>()(v); })
    .def("__repr__", [](nged::ItemID v){ return fmt::format("ItemID({})", v.value()); })
    .def(py::pickle(
          [](nged::ItemID const& id){ return py::int_(id.value()); },
          [](py::int_ v){ return nged::ItemID(v.cast<uint64_t>()); }));
  m.attr("ID_None") = nged::ID_None;
  // }}}

  // forward decl
  auto pydoc = py::class_<nged::NodeGraphDoc, PyNodeGraphDoc, nged::NodeGraphDocPtr>(m, "Document");
  auto pygraph = py::class_<nged::Graph, PyGraph, nged::GraphPtr>(m, "Graph");
  auto canvas = py::class_<nged::Canvas>(m, "Canvas");
  auto editor = py::class_<nged::NodeGraphEditor, nged::EditorPtr>(m, "EditorBase");

  // parmscript
  bindParmToPython(m);

  // GraphItem {{{
  OurPyTypes::instance().GraphItemType =
  py::class_<nged::GraphItem, PyGraphItem, nged::GraphItemPtr>(m, "GraphItem")
    .def(py::init<nged::Graph*>())
    .def_property_readonly("id", &nged::GraphItem::id)
    .def_property_readonly("pos", &nged::GraphItem::pos)
    .def_property_readonly("aabb", &nged::GraphItem::aabb)
    .def_property_readonly("graph", &nged::GraphItem::parent)
    .def_property_readonly("uid", [](nged::GraphItem* item){return nged::uidToString(item->uid());})
    .def_property_readonly("sourceUID", [](nged::GraphItem* item){return nged::uidToString(item->sourceUID());})
    .def("draw", &nged::GraphItem::draw, py::arg("canvas"), py::arg("state") = nged::GraphItemState::DEFAULT)
    .def("hitTest", py::overload_cast<nged::Vec2>(&nged::GraphItem::hitTest, py::const_), py::arg("pos"))
    .def("hitTest", py::overload_cast<nged::AABB>(&nged::GraphItem::hitTest, py::const_), py::arg("box"))
    .def("zOrder", &nged::GraphItem::zOrder)
    .def("canMove", &nged::GraphItem::canMove);
  // }}}

  // NodeDesc {{{
  py::class_<PyNodeDesc, std::shared_ptr<PyNodeDesc>>(m, "NodeDesc")
    .def_readwrite("type", &PyNodeDesc::type)
    .def_readwrite("label", &PyNodeDesc::label)
    .def_readwrite("category", &PyNodeDesc::category)
    .def_readwrite("description", &PyNodeDesc::description)
    .def_readwrite("color", &PyNodeDesc::color)
    .def_readwrite("iconData", &PyNodeDesc::iconData)
    .def_readwrite("iconType", &PyNodeDesc::iconType)
    .def_readwrite("size", &PyNodeDesc::size)
    .def_readwrite("numMaxInputs", &PyNodeDesc::numMaxInputs)
    .def_readwrite("numRequiredInputs", &PyNodeDesc::numRequiredInputs)
    .def_readwrite("numOutputs", &PyNodeDesc::numOutputs)
    .def_readwrite("parms", &PyNodeDesc::parms)
    .def_readwrite("hidden", &PyNodeDesc::hidden)
    .def_readwrite("inputDescriptions", &PyNodeDesc::inputDescriptions)
    .def_readwrite("outputDescriptions", &PyNodeDesc::outputDescriptions)
    .def(py::init())
    .def(py::init([](
        std::string type,
        std::string label,
        std::string category,
        std::string description,
        uint32_t color,
        std::string iconData,
        nged::IconType iconType,
        nged::Vec2 size,
        nged::sint numMaxInputs,
        nged::sint numRequiredInputs,
        nged::sint numOutputs,
        std::string parms,
        bool        hidden,
        std::vector<std::string> inputDescriptions,
        std::vector<std::string> outputDescriptions
      ){
        auto* desc = new PyNodeDesc{
          type,
          label,
          category,
          description,
          color,
          iconData,
          iconType,
          size,
          numMaxInputs,
          numRequiredInputs,
          numOutputs,
          parms,
          hidden,
          inputDescriptions,
          outputDescriptions
        };
        return std::shared_ptr<PyNodeDesc>(std::move(desc));
      }),
      py::arg("type"),
      py::arg("label"),
      py::arg("category")="",
      py::arg("description")="",
      py::kw_only(),
      py::arg("color")=0xFFFFFFFF,
      py::arg("iconData")="",
      py::arg("iconType")=nged::IconType::IconFont,
      py::arg("size")=nged::Vec2(60, 30),
      py::arg("numMaxInputs")=1,
      py::arg("numRequiredInputs")=0,
      py::arg("numOutputs")=1,
      py::arg("parms")="",
      py::arg("hidden")=false,
      py::arg("inputDescriptions")=std::vector<std::string>(),
      py::arg("outputDescriptions")=std::vector<std::string>()
    )
    .def(py::init([](py::dict args){
      auto desc = std::make_shared<PyNodeDesc>();
      if (args.contains("type")) desc->type = args["type"].cast<nged::String>();
      if (args.contains("label")) desc->label = args["label"].cast<nged::String>();
      if (args.contains("category")) desc->category = args["category"].cast<nged::String>();
      if (args.contains("description")) desc->description = args["description"].cast<nged::String>();
      if (args.contains("color")) desc->color = args["color"].cast<uint32_t>();
      if (args.contains("iconData")) desc->iconData = args["iconData"].cast<nged::String>();
      if (args.contains("iconType")) desc->iconType = args["iconType"].cast<nged::IconType>();
      if (args.contains("size")) desc->size = args["size"].cast<nged::Vec2>();
      if (args.contains("numMaxInputs")) desc->numMaxInputs = args["numMaxInputs"].cast<nged::sint>();
      if (args.contains("numRequiredInputs")) desc->numRequiredInputs = args["numRequiredInputs"].cast<nged::sint>();
      if (args.contains("numOutputs")) desc->numOutputs = args["numOutputs"].cast<nged::sint>();
      if (args.contains("parms")) desc->parms = args["parms"].cast<nged::String>();
      if (args.contains("inputDescriptions")) desc->inputDescriptions = args["inputDescriptions"].cast<nged::Vector<nged::String>>();
      if (args.contains("outputDescriptions")) desc->outputDescriptions = args["outputDescriptions"].cast<nged::Vector<nged::String>>();
      return desc;
    }));
  // }}} NodeDesc

  // Node {{{
  py::class_<nged::Node, PyNode, nged::GraphItem, nged::NodePtr>(m, "Node")
    .def("inputPinPos", &nged::Node::inputPinPos, py::arg("pin"))
    .def("outputPinPos", &nged::Node::outputPinPos, py::arg("pin"))
    .def("inputPinDir", &nged::Node::inputPinDir, py::arg("pin"))
    .def("outputPinDir", &nged::Node::outputPinDir, py::arg("pin"))
    .def("getNodeDescription", [](nged::NodePtr self) -> pybind11::object {
      nged::String desc;
      if (self && self->getNodeDescription(desc)) {
        return pybind11::str(desc);
      }
      return pybind11::none();
    })
    .def("getInputDescription", [](nged::NodePtr self, sint pin) -> pybind11::object {
      nged::String desc;
      if (self && self->getInputDescription(pin, desc)) {
        return pybind11::str(desc);
      }
      return pybind11::none();
    }, py::arg("pin"))
    .def("getOutputDescription", [](nged::NodePtr self, sint pin) -> pybind11::object {
      nged::String desc;
      if (self && self->getOutputDescription(pin, desc)) {
        return pybind11::str(desc);
      }
      return pybind11::none();
    }, py::arg("pin"))
    .def("getIcon", [](nged::NodePtr self) -> pybind11::object {
      nged::StringView icon;
      nged::IconType   type;
      if (self && self->getIcon(type, icon)) {
        return pybind11::make_tuple(type, pybind11::str(icon));
      }
      return pybind11::none();
    })
    .def("draw", &nged::Node::draw, py::arg("canvas"), py::arg("state") = nged::GraphItemState::DEFAULT)
    .def_property_readonly("numMaxInputs", &nged::Node::numMaxInputs)
    .def_property_readonly("numFixedInputs", &nged::Node::numFixedInputs)
    .def_property_readonly("numOutputs", &nged::Node::numOutputs)
    .def("acceptInput", &nged::Node::acceptInput, py::arg("pin"), py::arg("sourceNode"), py::arg("sourcePin"))
    .def_property_readonly("label", &nged::Node::label)
    .def_property_readonly("type", &nged::Node::type)
    .def_property_readonly("name", &nged::Node::name)
    .def("getInput", [](nged::Node* self, nged::sint pin)->py::object {
      nged::NodePtr nodeptr;
      nged::sint    outpin;
      if (self->getInput(pin, nodeptr, outpin)) {
        return py::make_tuple(nodeptr, outpin);
      } else {
        return py::none();
      }
    }, py::arg("pin"))
    .def("rename", [](nged::Node* self, nged::String const& name)->py::str {
      nged::String acceptedName = name;
      if (self && self->rename(name, acceptedName)) {
        return py::str(acceptedName);
      }
      return py::none();
    }, py::arg("name"))
    .def("asGraph", py::overload_cast<>(&nged::Node::asGraph))
    .def("lastConnectedInputPort", &nged::Node::getLastConnectedInputPort)
    .def("extraDependencies", [](nged::Node* self){
      py::list result;
      nged::Vector<nged::ItemID> deps;
      if (self->getExtraDependencies(deps))
        for (auto id: deps)
          result.append(id);
      return result;
    })

    .def(py::init([](nged::Graph* graph, std::shared_ptr<PyNodeDesc> desc) {
      return std::make_shared<PyNode>(graph, desc);
    }), py::arg("graph"), py::arg("desc"))
    .def("parm", [](PyNode* self, nged::String const& name) {
      return self->parmInspector.getParm(name);
    }, py::arg("name"))
    .def("getExtraParms", [](PyNode* self) {
      return self->extraParms;
    })
    .def("setExtraParms", [](PyNode* self, nged::String const& parms){
      self->setExtraParms(parms);
    }, py::arg("extraParms"))
    .def("parmDirty", [](PyNode* self) {
      return self->parmInspector.dirty();
    })
    .def("parmMarkClean", [](PyNode* self) {
        self->parmInspector.markClean();
    });
  // }}} Node

  // GraphItemFactory {{{
  py::class_<nged::GraphItemFactory, nged::GraphItemFactoryPtr>(m, "GraphItemFactoryBase")
    .def("make", &nged::GraphItemFactory::make, py::arg("graph"), py::arg("name"))
    .def("listNames", &nged::GraphItemFactory::listNames, py::arg("userCreatable") = true)
    .def("factoryName", &nged::GraphItemFactory::factoryName, py::arg("item"));

  py::class_<PyGraphItemFactory, nged::GraphItemFactory, std::shared_ptr<PyGraphItemFactory>>(m, "GraphItemFactory")
    .def("register", [](PyGraphItemFactory* factory, nged::String const& name, bool userCreatable, py::object func) {
      factory->pyFactories_[name] = std::move(func);
      factory->pyUserCreatable_[name] = userCreatable;
    }, py::arg("name"), py::arg("userCreatable"), py::arg("factoryFunction"));

  m.def("builtinGraphItemFactory", [](){
     auto newFactory = std::make_shared<PyGraphItemFactory>(*nged::defaultGraphItemFactory());
     nged::addImGuiItems(newFactory);
     return newFactory;
  });
  // }}}

  // NodeFactory {{{
  py::class_<nged::NodeFactory, PyNodeFactory, nged::NodeFactoryPtr>(m, "NodeFactory")
    .def(py::init([](){return std::make_shared<PyNodeFactory>();}))
    .def("register", [](nged::NodeFactory* factory, std::shared_ptr<PyNodeDesc> def, py::function func) {
      auto* pyfactory = dynamic_cast<PyNodeFactory*>(factory);
      assert(pyfactory != nullptr && "this NodeFactory is not made for python");
      auto name = def->type;
      pyfactory->define(name, std::move(def), std::move(func));
    }, py::arg("desc"), py::arg("factoryFunction"))
    .def_property_readonly("descs", [](nged::NodeFactory* factory) {
      auto* pyfactory = dynamic_cast<PyNodeFactory*>(factory);
      assert(pyfactory != nullptr && "this NodeFactory is not made for python");
      return pyfactory->descs();
    })
    .def("createRootGraph", &nged::NodeFactory::createRootGraph)
    .def("createNode", &nged::NodeFactory::createNode);
  // }}}

  // Link {{{
  py::class_<nged::Link, nged::GraphItem, nged::LinkPtr>(m, "Link")
    .def_property_readonly("sourceItem", [](nged::Link* self) -> pybind11::object {
      return pybind11::cast(self->input().sourceItem);
    })
    .def_property_readonly("sourcePin", [](nged::Link* self) -> pybind11::object {
      return pybind11::cast(self->input().sourcePort);
    })
    .def_property_readonly("targetItem", [](nged::Link* self) -> pybind11::object {
      return pybind11::cast(self->output().destItem);
    })
    .def_property_readonly("targetPin", [](nged::Link* self) -> pybind11::object {
      return pybind11::cast(self->output().destPort);
    })
    .def("__repr__", [](nged::Link* self) {
      return fmt::format("Link({}.{} --> {}.{})",
        self->input().sourceItem.value(), self->input().sourcePort,
        self->output().destItem.value(), self->output().destPort);
    });
  // }}}

  // Traverse Result {{{
  class GraphTraverseResultMadePublic : public nged::GraphTraverseResult
  {
  public:
    auto& inputs() { return inputs_; }
    auto& outputs() { return inputs_; }
    auto& nodes() { return inputs_; }
    auto& closures() { return closures_; }
    auto& idmap() { return idmap_; }
  };
  py::class_<nged::GraphTraverseResult::Accessor>(m, "GraphTraverseResultAccesor")
    .def_property_readonly("node", &nged::GraphTraverseResult::Accessor::node)
    .def_property_readonly("inputCount", &nged::GraphTraverseResult::Accessor::inputCount)
    .def_property_readonly("outputCount", &nged::GraphTraverseResult::Accessor::outputCount)
    .def_property_readonly("valid", &nged::GraphTraverseResult::Accessor::valid)
    .def("input", &nged::GraphTraverseResult::Accessor::input, py::arg("pin"))
    .def("output", &nged::GraphTraverseResult::Accessor::output, py::arg("pin"))
    .def("inputIndex", &nged::GraphTraverseResult::Accessor::inputIndex, py::arg("pin"))
    .def("outputIndex", &nged::GraphTraverseResult::Accessor::outputIndex, py::arg("pin"))
    .def("__repr__", [](nged::GraphTraverseResult::Accessor* self) {
      return fmt::format("NodeAccesor[{}]({})", self->index(), self->node()->label());
    });

  py::class_<nged::GraphTraverseResult>(m, "GraphTraverseResult")
    .def_property_readonly("count", &nged::GraphTraverseResult::count)
    .def("__len__", &nged::GraphTraverseResult::count)
    .def("__getitem__", [](nged::GraphTraverseResult* result, size_t index) {
      if (index >= result->count()) {
        throw py::index_error();
      }
      return nged::GraphTraverseResult::Accessor(result, index);
    }, py::arg("index"), py::keep_alive<0, 1>())
    .def("__iter__", [](nged::GraphTraverseResult* result) {
      return py::make_iterator(result->begin(), result->end());
    }, py::keep_alive<0, 1>())
    .def("node", &nged::GraphTraverseResult::node, py::arg("index"))
    .def("inputCount", &nged::GraphTraverseResult::inputCount, py::arg("nodeIndex"))
    .def("outputCount", &nged::GraphTraverseResult::outputCount, py::arg("nodeIndex"))
    .def("inputOf", &nged::GraphTraverseResult::inputOf, py::arg("nodeIndex"), py::arg("pin"))
    .def("outputOf", &nged::GraphTraverseResult::outputOf, py::arg("nodeIndex"), py::arg("pin"))
    .def("inputIndexOf", &nged::GraphTraverseResult::inputIndexOf, py::arg("nodeIndex"), py::arg("pin"))
    .def("outputIndexOf", &nged::GraphTraverseResult::outputIndexOf, py::arg("nodeIndex"), py::arg("pin"))
    .def("find", &nged::GraphTraverseResult::find, py::arg("id"));
  // }}}

  // Graph {{{
  pygraph
    .def(py::init([](nged::NodeGraphDoc* doc, nged::Graph* parent, nged::String name){
      return std::make_shared<PyGraph>(doc, parent, std::move(name));
    }))
    .def_property_readonly("doc", &nged::Graph::docRoot)
    .def_property_readonly("name", &nged::Graph::name)
    .def_property("selfReadonly", &nged::Graph::selfReadonly, &nged::Graph::setSelfReadonly)
    .def_property_readonly("readonly", &nged::Graph::readonly, "if any parent or self is readonly")
    .def_property_readonly("parent", &nged::Graph::parent)
    .def("rename", &nged::Graph::rename, py::arg("newName"))
    .def("items", [](nged::Graph* graph){
      auto const& items = graph->items();
      py::set result;
      for (auto id: items) {
        result.add(graph->get(id));
      }
      return result;
    })
    .def("links", [](nged::Graph* graph) {
      auto const& links = graph->allLinks();
      py::dict result;
      for (auto&& pair: links) {
        auto key = py::make_tuple(pair.first.destItem, pair.first.destPort);
        auto val = py::make_tuple(pair.second.sourceItem, pair.second.sourcePort);
        result[key] = val;
      }
      return result;
    })
    //.def("pinPos", &nged::Graph::pinPos)
    //.def("pinDir", &nged::Graph::pinDir)
    .def("getLink", &nged::Graph::getLink)
    .def("getLinkSource", [](nged::Graph* self, nged::ItemID destItem, sint destPin)->py::object {
      if (nged::InputConnection inconn; self->getLinkSource(destItem, destPin, inconn)) {
        return py::make_tuple(inconn.sourceItem, inconn.sourcePort);
      }
      return py::none();
    }, py::arg("targetItem"), py::arg("targetPin"))
    .def("getLinkDestiny", [](nged::Graph* self, nged::ItemID sourceItem, sint sourcePin)->py::list {
      py::list listout;
      if (nged::Vector<nged::OutputConnection> outconn; self->getLinkDestiny(sourceItem, sourcePin, outconn)) {
        for (auto&& c: outconn)
          listout.append(py::make_tuple(c.destItem, c.destPort));
      }
      return listout;
    }, py::arg("sourceItem"), py::arg("sourcePin"))
    .def("createNode", &nged::Graph::createNode, py::arg("type"))
    .def("add", [](nged::Graph* graph, py::object item) { // don't expose this to python, always use graph.createNode / itemFactory.make to add items
      auto itemptr = item.cast<nged::GraphItemPtr>();
      if (!itemptr)
        throw py::type_error("Graph.add(item) expect GraphItem as argument");
      assert(dynamic_cast<PyNodeGraphDoc*>(graph->docRoot()));
      static_cast<PyNodeGraphDoc*>(graph->docRoot())->pyObjects.insert(item);
      graph->add(itemptr);
    }, py::arg("item"))
    .def("get", &nged::Graph::get, py::arg("id"))
    .def("tryGet", &nged::Graph::tryGet, py::arg("id"))
    .def("getItemByUID", [](nged::Graph* graph, std::string const& uidstr)->nged::GraphItemPtr{
      if (uidstr.empty())
        return nullptr;
      auto uid = nged::uidFromString(uidstr);
      return graph->docRoot()->findItemByUID(uid);
    })
    .def("remove", [](nged::Graph* graph, py::list ids){
      nged::HashSet<nged::ItemID> idSet;
      for (auto id : ids) {
        idSet.insert(id.cast<nged::ItemID>());
      }
      graph->remove(idSet);
    }, py::arg("ids"))
    .def("remove", [](nged::Graph* graph, nged::ItemID id){
      graph->remove({id});
    }, py::arg("id"))
    .def("move", [](nged::Graph* graph, nged::ItemID id, nged::Vec2 delta) {
      graph->move({id}, delta);
    }, py::arg("id"), py::arg("delta"))
    .def("move", [](nged::Graph* graph, std::vector<nged::ItemID> ids, nged::Vec2 delta) {
      nged::HashSet<nged::ItemID> idset;
      for (auto&& id: ids) idset.insert(id);
      graph->move(idset, delta);
    }, py::arg("idList"), py::arg("delta"))
    .def("setLink", &nged::Graph::setLink, py::arg("sourceItem"), py::arg("sourcePin"), py::arg("targetItem"), py::arg("targetPin"))
    .def("removeLink", &nged::Graph::removeLink, py::arg("targetItem"), py::arg("targetPin"))
    .def("traverse", [](nged::Graph* graph, std::vector<nged::ItemID> const& startIds, nged::StringView direction, bool allowLoop) -> std::unique_ptr<nged::GraphTraverseResult> {
      auto result = std::make_unique<nged::GraphTraverseResult>();
      if (graph->traverse(*result, startIds, direction=="down", allowLoop)) {
        return result;
      } else {
        return nullptr;
      }
    }, py::arg("startIds"), py::arg("direction") = "up", py::arg("allowLoop") = false);
  // }}}

  // Document {{{
  pydoc
    .def(py::init([](nged::NodeFactoryPtr nodeFactory, nged::GraphItemFactory const* itemFactory){
      return std::make_shared<PyNodeGraphDoc>(nodeFactory, itemFactory);
    }))
    .def("filterFileInput", &nged::NodeGraphDoc::filterFileInput)
    .def("filterFileOutput", &nged::NodeGraphDoc::filterFileOutput)
    .def_property_readonly("title", &nged::NodeGraphDoc::title)
    .def_property_readonly("savePath", &nged::NodeGraphDoc::savePath)
    .def_property_readonly("root", &nged::NodeGraphDoc::root)
    .def_property("readonly", &nged::NodeGraphDoc::readonly, &nged::NodeGraphDoc::setReadonly)
    .def("open", &nged::NodeGraphDoc::open, py::arg("path"))
    .def("save", &nged::NodeGraphDoc::save)
    .def("saveAs", &nged::NodeGraphDoc::saveAs, py::arg("path"))
    .def("saveTo", &nged::NodeGraphDoc::saveTo, py::arg("path"))
    .def_property_readonly("dirty", &nged::NodeGraphDoc::dirty)
    .def("touch", &nged::NodeGraphDoc::touch)
    .def("undo", &nged::NodeGraphDoc::undo)
    .def("redo", &nged::NodeGraphDoc::redo)
    .def("getItem", &nged::NodeGraphDoc::getItem, py::arg("id"))
    .def("getItemByUID", [](nged::NodeGraphDoc* doc, std::string const& uidstr){
      auto uid = nged::uidFromString(uidstr);
      return doc->findItemByUID(uid);
    })
    .def("beginEditGroup", [](nged::NodeGraphDoc* self) {
      self->history().beginEditGroup();
    })
    .def("endEditGroup", [](nged::NodeGraphDoc* self, nged::String message){
      self->history().endEditGroup(std::move(message));
    }, py::arg("message"))
    .def_property_readonly("nodeFactory", &nged::NodeGraphDoc::nodeFactory)
    .def_property_readonly("itemFactory", &nged::NodeGraphDoc::itemFactory);
  // }}}

  // View {{{
  py::class_<nged::GraphView, PyGraphView, nged::GraphViewPtr>(m, "View")
    .def(py::init<nged::NodeGraphEditor*, nged::NodeGraphDocPtr>())
    .def_property_readonly("doc", &nged::GraphView::doc)
    .def_property_readonly("graph", &nged::GraphView::graph)
    .def_property_readonly("editor", &nged::GraphView::editor)
    .def_property_readonly("kind", &nged::GraphView::kind)
    .def_property_readonly("id", &nged::GraphView::id)
    .def_property("title", &nged::GraphView::title, &nged::GraphView::setTitle)
    .def_property_readonly("focused", &nged::GraphView::isFocused)
    .def_property_readonly("hovered", &nged::GraphView::isHovered)
    .def_property_readonly("defaultSize", &nged::GraphView::defaultSize)
    .def("reset", [](nged::GraphView* self, nged::GraphPtr graph){
      self->reset(graph);
    }, py::arg("graph"))
    .def("reset", py::overload_cast<nged::NodeGraphDocPtr>(&nged::GraphView::reset), py::arg("doc"))
    .def_property("windowFlags",
        [](PyGraphView* self){return self->windowFlags();},
        [](PyGraphView* self, ImGuiWindowFlags flags){self->setWindowFlags(flags);})
    .def("please", &nged::GraphView::please, py::arg("doThis"));

  py::class_<nged::NetworkView, nged::GraphView, std::shared_ptr<nged::NetworkView>>(m, "NetworkView")
    .def_property_readonly("hoveringItem", [](nged::NetworkView* view){
      return view->graph()->tryGet(view->hoveringItem());
    })
    .def_property_readonly("selectedItems", [](nged::NetworkView* view){
      py::set result;
      for (auto id: view->selectedItems()) {
        if (auto item = view->graph()->tryGet(id))
          result.add(item);
      }
      return result;
    })
    .def_property_readonly("solySelectedNode", &nged::NetworkView::solySelectedNode)
    .def("setSelectedItems", [](nged::NetworkView* view, py::object items){
      nged::HashSet<nged::ItemID> ids;
      for (auto item: items) {
        auto* itemptr = py::cast<nged::GraphItem*>(item);
        if (!itemptr) {
          msghub::warnf("{} is not a GraphItem", std::string(item.str()));
          continue;
        }
        ids.insert(itemptr->id());
      }
      view->setSelectedItems(ids);
    }, py::arg("items"));

  py::class_<nged::InspectorView, nged::GraphView, std::shared_ptr<nged::InspectorView>>(m, "InspectorView");
  // }}}

  // Canvas {{{
  auto const& defaultShapeStyle = nged::Canvas::defaultShapeStyle;
  py::class_<nged::Canvas::ShapeStyle>(canvas, "ShapeStyle")
    .def_readwrite("filled", &nged::Canvas::ShapeStyle::filled)
    .def_readwrite("fillColor", &nged::Canvas::ShapeStyle::fillColor)
    .def_readwrite("strokeWidth", &nged::Canvas::ShapeStyle::strokeWidth)
    .def_readwrite("strokeColor", &nged::Canvas::ShapeStyle::strokeColor)
    .def(py::init([](bool filled, uint32_t fillcolor, float strokewidth, uint32_t strokecolor){
        return new nged::Canvas::ShapeStyle { filled, fillcolor, strokewidth, strokecolor };
      }),
      py::arg("filled") = defaultShapeStyle.filled,
      py::arg("fillColor") = defaultShapeStyle.fillColor,
      py::arg("strokeWidth") = defaultShapeStyle.strokeWidth,
      py::arg("strokeColor") = defaultShapeStyle.strokeColor);
  m.attr("defaultShapeStyle") = defaultShapeStyle;
  auto const& pyDefaultShapeStyle = m.attr("defaultShapeStyle");

  py::enum_<nged::Canvas::TextAlign>(canvas, "TextAlign")
    .value("Left", nged::Canvas::TextAlign::Left)
    .value("Center", nged::Canvas::TextAlign::Center)
    .value("Right", nged::Canvas::TextAlign::Right);

  py::enum_<nged::Canvas::TextVerticalAlign>(canvas, "VerticalAlign")
    .value("Top", nged::Canvas::TextVerticalAlign::Top)
    .value("Middle", nged::Canvas::TextVerticalAlign::Center)
    .value("Bottom", nged::Canvas::TextVerticalAlign::Bottom);

  py::enum_<nged::Canvas::FontFamily>(canvas, "FontFamily")
    .value("Sans", nged::Canvas::FontFamily::SansSerif)
    .value("Serif", nged::Canvas::FontFamily::Serif)
    .value("Mono", nged::Canvas::FontFamily::Mono)
    .value("Icon", nged::Canvas::FontFamily::Icon);

  py::enum_<nged::Canvas::FontStyle>(canvas, "FontStyle")
    .value("Regular", nged::Canvas::FontStyle::Regular)
    .value("Italic", nged::Canvas::FontStyle::Italic)
    .value("Strong", nged::Canvas::FontStyle::Strong);

  py::enum_<nged::Canvas::FontSize>(canvas, "FontSize")
    .value("Small", nged::Canvas::FontSize::Small)
    .value("Normal", nged::Canvas::FontSize::Normal)
    .value("Large", nged::Canvas::FontSize::Large);

  py::enum_<nged::Canvas::Layer>(canvas, "Layer")
    .value("Lower", nged::Canvas::Layer::Lower)
    .value("Low", nged::Canvas::Layer::Low)
    .value("Standard", nged::Canvas::Layer::Standard)
    .value("High", nged::Canvas::Layer::High)
    .value("Higher", nged::Canvas::Layer::Higher);

  auto const& defaultTextStyle = nged::Canvas::defaultTextStyle;
  py::class_<nged::Canvas::TextStyle>(canvas, "TextStyle")
    .def_readwrite("align", &nged::Canvas::TextStyle::align)
    .def_readwrite("valign", &nged::Canvas::TextStyle::valign)
    .def_readwrite("font", &nged::Canvas::TextStyle::font)
    .def_readwrite("style", &nged::Canvas::TextStyle::style)
    .def_readwrite("size", &nged::Canvas::TextStyle::size)
    .def_readwrite("color", &nged::Canvas::TextStyle::color)
    .def(py::init([](
         nged::Canvas::TextAlign         align,
         nged::Canvas::TextVerticalAlign valign,
         nged::Canvas::FontFamily        font,
         nged::Canvas::FontStyle         style,
         nged::Canvas::FontSize          size,
         uint32_t                        color)
      {
        return nged::Canvas::TextStyle {
          align, valign, font, style, size, color
        };
      }),
      py::arg("align") = defaultTextStyle.align,
      py::arg("valign") = defaultTextStyle.valign,
      py::arg("font") = defaultTextStyle.font,
      py::arg("style") = defaultTextStyle.style,
      py::arg("size") = defaultTextStyle.size,
      py::arg("color") = defaultTextStyle.color); 
  m.attr("defaultTextStyle") = defaultTextStyle;
  auto const& pyDefaultTextStyle = m.attr("defaultTextStyle");

  canvas
    .def_property_readonly("size",     &nged::Canvas::viewSize)
    .def_property_readonly("pos",      &nged::Canvas::viewPos)
    .def_property_readonly("scale",    &nged::Canvas::viewScale)
    .def_property_readonly("viewport", &nged::Canvas::viewport)
    .def("pushLayer", &nged::Canvas::pushLayer, py::arg("layer") = nged::Canvas::Layer::Standard)
    .def("popLayer", &nged::Canvas::popLayer)
    .def("drawLine", &nged::Canvas::drawLine, py::arg("a"), py::arg("b"), py::arg("color")=0xffffffff, py::arg("width") = 1.0f)
    .def("drawRect", &nged::Canvas::drawRect, py::arg("topLeft"), py::arg("bottomRight"), py::arg("cornerRadius") = 0.f, py::arg("style") = pyDefaultShapeStyle)
    .def("drawCircle", &nged::Canvas::drawCircle, py::arg("center"), py::arg("radius"), py::arg("segments") = 0, py::arg("style") = pyDefaultShapeStyle)
    .def("drawPoly", [](nged::Canvas* canvas, py::list pts, bool closed, nged::Canvas::ShapeStyle style){
      std::vector<nged::Vec2> ptlist;
      for (auto pt: pts) {
        ptlist.push_back(pt.cast<nged::Vec2>());
      }
      canvas->drawPoly(ptlist.data(), ptlist.size(), closed, style);
    }, py::arg("points"), py::arg("closed") = true, py::arg("style") = pyDefaultShapeStyle)
    .def("drawText", &nged::Canvas::drawText, py::arg("pos"), py::arg("text"), py::arg("style") = pyDefaultTextStyle);

  py::class_<nged::Canvas::Image, nged::Canvas::ImagePtr>(canvas, "Image");
  canvas
    .def_static("createImage", [](py::array_t<uint8_t> nparray){
      auto info = nparray.request();
      if (info.ndim != 3 || info.shape[2] != 4 || info.format != py::format_descriptor<uint8_t>::format())
        throw std::runtime_error("createImage: expect 3D buffer of 4 channels of uint8_t");
      if (info.strides[2] == 4 && info.strides[1] == 4*info.shape[2] && info.strides[0] == 4*info.shape[1]*info.shape[2]) // continuous
        return nged::Canvas::createImage(static_cast<uint8_t const*>(info.ptr), info.shape[0], info.shape[1]);
      else {
        auto pixels = std::make_unique<uint8_t[]>(info.shape[0]*info.shape[1]*info.shape[2]);
        for (py::ssize_t y = 0; y < info.shape[0]; ++y) {
          auto src = static_cast<uint8_t const*>(info.ptr) + y*info.strides[0];
          auto dst = pixels.get() + y*info.shape[1]*info.shape[2];
          std::copy(src, src+info.shape[1]*info.shape[2], dst);
        }
        return nged::Canvas::createImage(pixels.get(), info.shape[0], info.shape[1]);
      }
    }, py::arg("nparray_of_rgba"))
    .def("drawImage", &nged::Canvas::drawImage,
      py::arg("image"), py::arg("topLeft"), py::arg("bottomRight"), py::arg("uvMin") = nged::Vec2(0,0), py::arg("uvMax") = nged::Vec2(1,1));
  // }}}

  // Editor {{{
  py::enum_<nged::ModKey>(m, "ModKey")
    .value("NONE", nged::ModKey::NONE)
    .value("CTRL", nged::ModKey::CTRL)
    .value("SHIFT", nged::ModKey::SHIFT)
    .value("ALT",   nged::ModKey::ALT)
    .value("SUPER", nged::ModKey::SUPER);

  py::class_<nged::Shortcut>(m, "Shortcut")
    .def(py::init<>())
    .def(py::init(&nged::Shortcut::parse), py::arg("keyBinding"))
    .def_readwrite("key", &nged::Shortcut::key)
    .def_readwrite("mod", &nged::Shortcut::mod);

  py::class_<nged::CommandManager::Command, PyCommand, std::shared_ptr<nged::CommandManager::Command>>(m, "Command")
    .def(py::init<nged::String, nged::String, nged::String, nged::Shortcut, bool>(),
        py::arg("name"), py::arg("description"), py::arg("view"), py::arg("shortcut") = nged::Shortcut{}, py::arg("hiddenInMenu") = false)
    .def_property("mayModifyGraph", &nged::CommandManager::Command::mayModifyGraph, &nged::CommandManager::Command::setMayModifyGraph)
    .def_property_readonly("name", &nged::CommandManager::Command::name)
    .def_property_readonly("description", &nged::CommandManager::Command::description)
    .def_property_readonly("view", &nged::CommandManager::Command::view)
    .def_property_readonly("shortcut", &nged::CommandManager::Command::shortcut)
    .def_property_readonly("hiddenInMenu", &nged::CommandManager::Command::hiddenInMenu);

  editor
    .def("open", &nged::NodeGraphEditor::openDoc, py::arg("path"))
    .def("save", &nged::NodeGraphEditor::saveDoc, py::arg("doc"))
    .def("saveAs", &nged::NodeGraphEditor::saveDocAs, py::arg("doc"), py::arg("path"))
    .def("newDoc", &nged::NodeGraphEditor::createNewDocAndDefaultViews)
    .def("addView", &nged::NodeGraphEditor::addView, py::arg("doc"), py::arg("kind"))
    .def("closeView", &nged::NodeGraphEditor::closeView, py::arg("view"), py::arg("confirm")=true)
    .def("views", [](nged::EditorPtr editor)->py::tuple{
      py::tuple result(editor->views().size());
      int i = 0;
      for (auto v: editor->views()) {
        result[i] = v; ++i;
      }
      return result;
    })
    .def("update", &nged::NodeGraphEditor::update, py::arg("deltaTime"))
    .def("draw", &nged::NodeGraphEditor::draw)
    .def("addCommand", [](nged::EditorPtr editor, pybind11::object command){ std::static_pointer_cast<PyImGuiNodeGraphEditor>(editor)->addCommand(command); })
    .def("removeCommand", [](nged::EditorPtr editor, nged::String name){ std::static_pointer_cast<PyImGuiNodeGraphEditor>(editor)->removeCommand(name); })
    .def("agreeToQuit", &nged::NodeGraphEditor::agreeToQuit);

  py::class_<PyImGuiNodeGraphEditor, nged::NodeGraphEditor, std::shared_ptr<PyImGuiNodeGraphEditor>>(m, "Editor")
    .def(py::init(&createEditor), py::arg("docFactory"), py::arg("nodeFactory"), py::arg("itemFactory"))
    .def("registerView", [](PyImGuiNodeGraphEditor* editor, std::string const& name, py::function func) {
      auto viewFactory = editor->viewFactory();
      static_cast<PyViewFactory*>(viewFactory.get())->set(name, func);
    }, py::arg("name"), py::arg("factory"))
    .def_property("defaultLayoutDesc", &PyImGuiNodeGraphEditor::defaultLayoutDesc, &PyImGuiNodeGraphEditor::setDefaultLayoutDesc)
    .def("setOnInspectCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyOnInspectCallback = callback;
    })
    .def("setBeforeItemAddedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyBeforeItemAddedCallback = callback;
    })
    .def("setAfterItemAddedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyAfterItemAddedCallback = callback;
    })
    .def("setBeforeItemRemovedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyBeforeItemRemovedCallback = callback;
    })
    .def("setBeforeNodeRenamedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyBeforeNodeRenamedCallback = callback;
    })
    .def("setAfterNodeRenamedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyAfterNodeRenamedCallback = callback;
    })
    .def("setBeforeViewUpdateCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyBeforeViewUpdateCallback = callback;
    })
    .def("setAfterViewUpdateCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyAfterViewUpdateCallback = callback;
    })
    .def("setBeforeViewDrawCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyBeforeViewDrawCallback = callback;
    })
    .def("setAfterViewDrawCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyAfterViewDrawCallback = callback;
    })
    .def("setOnItemClickedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyOnItemClickedCallback = callback;
    })
    .def("setOnItemDoubleClickedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyOnItemDoubleClickedCallback = callback;
    })
    .def("setOnItemHoveredCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyOnItemHoveredCallback = callback;
    })
    .def("setOnSelectionChangedCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyOnSelectionChangedCallback = callback;
    })
    .def("setBeforeLinkSetCallback", [](nged::EditorPtr editor, pybind11::function callback){
      static_cast<PyResponser*>(editor->responser())->pyBeforeLinkSetCallback = callback;
    })
    .def("setOnLinkSetCallback", [](nged::EditorPtr editor, pybind11::function callback) {
      static_cast<PyResponser*>(editor->responser())->pyOnLinkSetCallback = callback;
    })
    .def("setOnLinkRemovedCallback", [](nged::EditorPtr editor, pybind11::function callback) {
      static_cast<PyResponser*>(editor->responser())->pyOnLinkRemovedCallback = callback;
    })
    .def("setAfterPasteCallback", [](nged::EditorPtr editor, pybind11::function callback) {
      static_cast<PyResponser*>(editor->responser())->pyAfterPasteCallback = callback;
    })
    .def("setParmModifiedCallback", [](PyImGuiNodeGraphEditor* editor, pybind11::function callback) {
      editor->pyParmModifiedCallback = callback;
    });
  // }}} Editor

  // App {{{
  py::class_<nged::App, PyApp>(m, "App")
    .def(py::init<>())
    .def("title", &nged::App::title)
    .def("agreeToQuit", &nged::App::agreeToQuit)
    .def("init", &nged::App::init)
    .def("update", &nged::App::update)
    .def("quit", &nged::App::quit);

  m.def_submodule("msghub", "Message Hub")
    .def("trace", &msghub::trace)
    .def("debug", &msghub::debug)
    .def("info", &msghub::info)
    .def("warn", &msghub::warn)
    .def("notice", &msghub::notice)
    .def("error", &msghub::error)
    .def("fatal", &msghub::fatal);

  m.def("startApp", &nged::startApp);
  // }}}

  // ImGui {{{
  auto module_imgui = m.def_submodule("ImGui", "ImGui bindings");
  bind_imgui_to_py(module_imgui);
  module_imgui
    .def("PushIconFont", [](){
        ImGui::PushFont(nged::ImGuiResource::instance().iconFont); })
    .def("PushMonoFont", [](){
        ImGui::PushFont(nged::ImGuiResource::instance().monoFont); })
    .def("PushSansFont", [](){
        ImGui::PushFont(nged::ImGuiResource::instance().sansSerifFont); })
    .def("PushFont", [](nged::Canvas::TextStyle const& style) {
        ImGui::PushFont(nged::ImGuiResource::instance().getBestMatchingFont(style, 1.f)); })
    .def("PopFont", &ImGui::PopFont);
  // }}}
}
