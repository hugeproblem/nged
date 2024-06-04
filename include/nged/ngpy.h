#pragma once
#pragma once
#include "ngdoc.h"
#include "nged.h"
#include "nged_imgui.h"
#include "entry/entry.h"
#include "parmscript.h"
#include "parminspector.h"
#include "nlohmann/json.hpp"
#include <pybind11/pybind11.h>

#include <map>
#include <stdexcept>

using nged::sint;

template<class PyHandle>
struct PyPtrHash
{
  size_t operator()(PyHandle const& handle) const { return std::hash<void*>()(handle.ptr()); }
};

template<class PyHandle>
struct PyPtrEqual
{
  bool operator()(PyHandle const& lhs, PyHandle const& rhs) const
  {
    return lhs.ptr() == rhs.ptr();
  }
};

// PyGraphItem {{{
template<class GraphItemBase = nged::GraphItem>
class PyGraphItemBase : public GraphItemBase
{
protected:
  std::string pyFactoryName_ = "";
  friend class PyGraphItemFactory;

public:
  using GraphItemBase::GraphItemBase;

  void draw(nged::Canvas* canvas, nged::GraphItemState state) const override
  {
    PYBIND11_OVERLOAD_PURE(void, nged::GraphItem, draw, canvas, state);
  }

  bool hitTest(nged::Vec2 point) const override
  {
    PYBIND11_OVERLOAD(bool, nged::GraphItem, hitTest, point);
  }

  bool hitTest(nged::AABB box) const override
  {
    PYBIND11_OVERLOAD(bool, nged::GraphItem, hitTest, box);
  }

  int zOrder() const override { PYBIND11_OVERLOAD(int, nged::GraphItem, zOrder, ); }

  bool canMove() const override { PYBIND11_OVERLOAD(bool, nged::GraphItem, canMove, ); }

  bool moveTo(nged::Vec2 point) override
  {
    PYBIND11_OVERLOAD(bool, nged::GraphItem, moveTo, point);
  }

  virtual bool serialize(nged::Json& json) const override
  {
    if (!GraphItemBase::serialize(json))
      return false;
    try {
      pybind11::gil_scoped_acquire gil;
      auto pyserialize = pybind11::get_override(this, "serialize");
      if (pyserialize) {
        auto pystr = pyserialize();
        if (pystr.is_none())
          return false;
        json["pydata"] = pystr.template cast<std::string>();
      }
      return true;
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed to serialize item: {}", e.what());
      return false;
    }
  }

  virtual bool deserialize(nged::Json const& json) override
  {
    if (!GraphItemBase::deserialize(json))
      return false;
    try {
      pybind11::gil_scoped_acquire gil;
      auto pydeserialize = pybind11::get_override(this, "deserialize");
      if (pydeserialize) {
        auto pystr = json.value("pydata", "");
        pydeserialize(pystr);
      }
      return true;
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed to deserialize item: {}", e.what());
      return false;
    }
  }
};

class PyGraphItem : public PyGraphItemBase<nged::GraphItem>
{
public:
  using PyGraphItemBase<nged::GraphItem>::PyGraphItemBase;
};
// }}}

// PyNode {{{
struct PyNodeDesc
{
  nged::String type;
  nged::String label;
  nged::String category;
  nged::String description;
  uint32_t color = 0xffffffff;
  nged::String iconData;
  nged::IconType iconType = nged::IconType::IconFont;
  nged::Vec2 size = {0, 0};
  nged::sint numMaxInputs = 4;
  nged::sint numRequiredInputs = 0;
  nged::sint numOutputs = 1;
  nged::String parms = "";
  bool hidden = false;

  nged::Vector<nged::String> inputDescriptions;
  nged::Vector<nged::String> outputDescriptions;
};

class PyNode : public PyGraphItemBase<nged::Node>
{
public:
  std::shared_ptr<PyNodeDesc> def;
  std::string extraParms;
  parmscript::ParmSetInspector parmInspector;

  PyNode(nged::Graph* graph, std::shared_ptr<PyNodeDesc> def)
      : PyGraphItemBase<nged::Node>(graph), def(def)
  {
    type_ = def->type;
    name_ = def->label;
    color_ = gmath::fromUint32sRGBA(def->color);
    parmInspector.loadParmScript(def->parms);
    if (length2(def->size) > 1) {
      aabb_ = {-def->size / 2, def->size / 2};
    }
  }

  void setExtraParms(std::string extraParms);

  nged::sint numMaxInputs() const override { return def->numMaxInputs; }

  nged::sint numFixedInputs() const override { return def->numRequiredInputs; }

  bool isRequiredInput(nged::sint pin) const override
  {
    return pin >= 0 && pin < def->numRequiredInputs;
  }

  nged::sint numOutputs() const override { return def->numOutputs; }

  bool getNodeDescription(nged::String& desc) const override
  {
    if (!def->description.empty()) {
      desc = def->description;
      return true;
    } else {
      return false;
    }
  }

  bool getInputDescription(sint pin, nged::String& desc) const override
  {
    if (pin >= 0 && pin < def->inputDescriptions.size()) {
      desc = def->inputDescriptions[pin];
      return true;
    } else {
      return false;
    }
  }

  bool getOutputDescription(sint pin, nged::String& desc) const override
  {
    if (pin >= 0 && pin < def->outputDescriptions.size()) {
      desc = def->outputDescriptions[pin];
      return true;
    } else {
      return false;
    }
  }

  bool getIcon(nged::IconType& type, nged::StringView& content) const override
  {
    if (!def->iconData.empty()) {
      type = def->iconType;
      content = def->iconData;
      return true;
    } else {
      return false;
    }
  }

  bool rename(nged::String const& desired, nged::String& accepted) override
  {
    try {
      pybind11::gil_scoped_acquire gil;
      pybind11::function pyrename = pybind11::get_override(this, "rename");
      if (pyrename) {
        pybind11::object pyaccepted = pyrename(desired);
        if (pyaccepted) {
          accepted = pyaccepted.cast<nged::String>();
          return true;
        } else {
          return false;
        }
      }
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed to call python rename callback: {}", e.what());
    }
    return nged::Node::rename(desired, accepted);
  }

  void draw(nged::Canvas* canvas, nged::GraphItemState state) const override
  {
    PYBIND11_OVERLOAD(void, nged::Node, draw, canvas, state);
  }

  nged::Graph* asGraph() override
  {
    try {
      pybind11::gil_scoped_acquire gil;
      pybind11::function pyAsGraph = pybind11::get_override(this, "asGraph");
      if (pyAsGraph) {
        auto* graph = pybind11::cast<nged::Graph*>(pyAsGraph());
        return graph;
      }
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed to call asGraph(): {}", e.what());
    }
    return nullptr;
  }

  nged::Graph const* asGraph() const override
  {
    try {
      pybind11::gil_scoped_acquire gil;
      pybind11::function pyAsGraph = pybind11::get_override(this, "asGraph");
      if (pyAsGraph) {
        auto* graph = pybind11::cast<nged::Graph const*>(pyAsGraph());
        return graph;
      }
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed to call asGraph(): {}", e.what());
    }
    return nullptr;
  }

  bool serialize(nged::Json& json) const override;
  bool deserialize(nged::Json const& json) override;
  void settled() override;
  size_t getExtraDependencies(nged::Vector<nged::ItemID>& deps) override;
};
// }}} PyNode

// Factory {{{
class PyGraphItemFactory : public nged::GraphItemFactory
{
public:
  PyGraphItemFactory() : GraphItemFactory() {}
  PyGraphItemFactory(nged::GraphItemFactory const& factory) : GraphItemFactory(factory) {}
  PyGraphItemFactory(PyGraphItemFactory const& that)
      : GraphItemFactory(that)
      , pyFactories_(that.pyFactories_)
      , pyUserCreatable_(that.pyUserCreatable_)
  {
  }

  nged::HashMap<nged::String, pybind11::function> pyFactories_;
  nged::HashMap<nged::String, bool> pyUserCreatable_;

  virtual nged::GraphItemPtr make(nged::Graph* parent, nged::String const& name) const override;
  virtual nged::String factoryName(nged::GraphItemPtr item) const override;
  virtual nged::Vector<nged::String> listNames(bool onlyUserCreatable = true) const override;
  virtual void discard(nged::Graph* graph, nged::GraphItem* item) const override;
};

class PyNodeFactory : public nged::NodeFactory
{
protected:
  std::map<nged::String, std::shared_ptr<PyNodeDesc>> pyDescs_;
  std::map<nged::String, pybind11::function> pyFactories_;

public:
  PyNodeFactory() = default;
  virtual ~PyNodeFactory() = default;

  auto const& descs() const { return pyDescs_; }
  void
  define(nged::String const& type, std::shared_ptr<PyNodeDesc> def, pybind11::function factory)
  {
    pyDescs_[type] = std::move(def);
    pyFactories_[type] = std::move(factory);
  }

  virtual nged::GraphPtr createRootGraph(nged::NodeGraphDoc* doc) const override;
  virtual nged::NodePtr createNode(nged::Graph* parent, nged::StringView type) const override;
  virtual void listNodeTypes(
    nged::Graph* parent,
    void* context,
    void (*callback)(
      void* context,
      nged::StringView category,
      nged::StringView type,
      nged::StringView name)) const override;
  virtual bool getNodeIcon(
    nged::StringView type,
    uint8_t const** iconDataPtr,
    size_t* iconSize,
    nged::IconType* iconType) const override
  {
    nged::String typestr(type);
    if (auto itr = pyDescs_.find(typestr); itr != pyDescs_.end()) {
      *iconDataPtr = reinterpret_cast<uint8_t const*>(itr->second->iconData.data());
      *iconSize = itr->second->iconData.size();
      *iconType = itr->second->iconType;
      return true;
    }
    return false;
  }
  virtual void discard(nged::Graph* graph, nged::Node* node) const override;
};
// }}} Factory

// Graph {{{
class PyGraph : public nged::Graph
{
public:
  using nged::Graph::Graph;

  bool serialize(nged::Json& json) const override;
  bool deserialize(nged::Json const& json) override;
};
// }}}

// Doc {{{
class PyNodeGraphDoc : public nged::NodeGraphDoc
{
  bool duringDestruction_ = false;

public:
  using nged::NodeGraphDoc::NodeGraphDoc;
  pybind11::object pyRootGraph;
  std::unordered_set<pybind11::object, PyPtrHash<pybind11::object>, PyPtrEqual<pybind11::object>>
    pyObjects; // to keep python objects alive

  virtual nged::String filterFileInput(nged::StringView fileContent) override
  {
    PYBIND11_OVERLOAD(nged::String, nged::NodeGraphDoc, filterFileInput, fileContent);
  }
  virtual nged::String filterFileOutput(nged::StringView fileContent) override
  {
    PYBIND11_OVERLOAD(nged::String, nged::NodeGraphDoc, filterFileOutput, fileContent);
  }

  virtual nged::ItemID addItem(nged::GraphItemPtr item) override
  {
    pybind11::handle pyhandle;
    if (auto* nodeptr = item->asNode())
      pyhandle = pybind11::detail::get_object_handle(
        nodeptr, pybind11::detail::get_type_info(typeid(PyNode)));
    else
      pyhandle = pybind11::detail::get_object_handle(
        item.get(), pybind11::detail::get_type_info(typeid(PyGraphItem)));
    if (pyhandle) {
      auto pyobj = pybind11::reinterpret_borrow<pybind11::object>(pyhandle);
      pyObjects.insert(std::move(pyobj));
    }
    return nged::NodeGraphDoc::addItem(item);
  }

  virtual void removeItem(nged::ItemID id) override
  {
    if (duringDestruction_)
      return;
    auto item = getItem(id);
    pybind11::handle pyhandle;
    if (auto* nodeptr = item->asNode())
      pyhandle = pybind11::detail::get_object_handle(
        nodeptr, pybind11::detail::get_type_info(typeid(PyNode)));
    else
      pyhandle = pybind11::detail::get_object_handle(
        item.get(), pybind11::detail::get_type_info(typeid(PyGraphItem)));
    if (pyhandle) {
      auto pyobj = pybind11::reinterpret_borrow<pybind11::object>(pyhandle);
      assert(pyObjects.find(pyobj) != pyObjects.end());
      pyObjects.erase(pyobj);
    }
    nged::NodeGraphDoc::removeItem(id);
  }

  virtual ~PyNodeGraphDoc()
  {
    duringDestruction_ = true;
    pyRootGraph = pybind11::none();
    root_.reset(); // ensures this happens before pyObjects destory
    pyObjects.clear();
  }
};
// }}}

// Editor Related {{{
// Command {{{
class PyCommand : public nged::CommandManager::Command
{
public:
  using nged::CommandManager::Command::Command;

  virtual void onConfirm(nged::GraphView* view) override
  {
    try {
      if (view && view->kind() == "network") {
        PYBIND11_OVERLOAD_PURE(
          void, nged::CommandManager::Command, onConfirm, static_cast<nged::NetworkView*>(view));
      } else {
        PYBIND11_OVERLOAD_PURE(void, nged::CommandManager::Command, onConfirm, view);
      }
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed calling onConfirm method: {}", e.what());
    }
  }
  virtual bool hasPrompt() const override
  {
    PYBIND11_OVERLOAD(bool, nged::CommandManager::Command, hasPrompt);
  }
  virtual void onOpenPrompt(nged::GraphView* view) override
  {
    PYBIND11_OVERLOAD(void, nged::CommandManager::Command, onOpenPrompt, view);
  }
  virtual bool onUpdatePrompt(nged::GraphView* view) override
  {
    PYBIND11_OVERLOAD(bool, nged::CommandManager::Command, onUpdatePrompt, view);
  }
  // virutal void draw(nged::NetworkView* view) {}
};
// }}}

// Responser {{{
class PyResponser : public nged::DefaultImGuiResponser
{
public:
  using Base = nged::DefaultImGuiResponser;
  pybind11::function pyOnInspectCallback, pyBeforeItemAddedCallback, pyAfterItemAddedCallback,
    pyBeforeItemRemovedCallback, pyBeforeNodeRenamedCallback, pyAfterNodeRenamedCallback,
    pyBeforeViewUpdateCallback, pyAfterViewUpdateCallback, pyBeforeViewDrawCallback,
    pyAfterViewDrawCallback, pyOnItemClickedCallback, pyOnItemDoubleClickedCallback,
    pyOnItemHoveredCallback, pyOnSelectionChangedCallback, pyBeforeLinkSetCallback,
    pyOnLinkSetCallback, pyOnLinkRemovedCallback, pyAfterPasteCallback;

  void onInspect(nged::InspectorView* view, nged::GraphItem** items, size_t count) override;
  bool beforeItemAdded(nged::Graph* graph, nged::GraphItem* item, nged::GraphItem** replacement)
    override;
  void afterItemAdded(nged::Graph* graph, nged::GraphItem* item) override;
  bool beforeItemRemoved(nged::Graph* graph, nged::GraphItem* item) override;
  bool beforeNodeRenamed(nged::Graph* graph, nged::Node* node) override;
  void afterNodeRenamed(nged::Graph* graph, nged::Node* node) override;
  void beforeViewUpdate(nged::GraphView* view) override;
  void afterViewUpdate(nged::GraphView* view) override;
  void beforeViewDraw(nged::GraphView* view) override;
  void afterViewDraw(nged::GraphView* view) override;
  void afterViewRemoved(nged::GraphView* view) override;
  void onItemClicked(nged::NetworkView* view, nged::GraphItem* item, int button) override;
  void onItemDoubleClicked(nged::NetworkView* view, nged::GraphItem* item, int button) override;
  void onItemHovered(nged::NetworkView* view, nged::GraphItem* item) override;
  void onSelectionChanged(nged::NetworkView* view) override;
  bool beforeLinkSet(nged::Graph* graph, nged::InputConnection src, nged::OutputConnection dst)
    override;
  void onLinkSet(nged::Link* link) override;
  void onLinkRemoved(nged::Link* link) override;
  void afterPaste(nged::Graph* graph, nged::GraphItem** items, size_t count) override;
};
// }}}

// GraphView {{{
class PyGraphView : public nged::ImGuiGraphView<PyGraphView, nged::GraphView>
{
public:
  PyGraphView(nged::NodeGraphEditor* editor, nged::NodeGraphDocPtr doc)
      : ImGuiGraphView(editor, doc)
  {
  }

  ImGuiWindowFlags windowFlags() const { return windowFlags_; }
  void setWindowFlags(ImGuiWindowFlags flags) { windowFlags_ = flags; }

  virtual void postInit() override { PYBIND11_OVERLOAD(void, nged::GraphView, postInit); }

  virtual nged::Vec2 defaultSize() const override
  {
    PYBIND11_OVERLOAD(nged::Vec2, nged::GraphView, defaultSize);
  }

  virtual void reset(nged::WeakGraphPtr graph) override
  {
    PYBIND11_OVERLOAD(void, nged::GraphView, reset, graph);
  }

  virtual void update(float dt) override { PYBIND11_OVERLOAD(void, nged::GraphView, update, dt); }

  virtual void drawContent()
  {
    pybind11::gil_scoped_acquire gil;
    auto draw = pybind11::get_override(this, "draw");
    try {
      draw();
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("failed to call draw() method: {}", e.what());
    }
  }

  virtual void onDocModified() override
  {
    PYBIND11_OVERLOAD_PURE(void, nged::GraphView, onDocModified);
  }

  virtual void onGraphModified() override
  {
    PYBIND11_OVERLOAD_PURE(void, nged::GraphView, onGraphModified);
  }

  virtual void onViewEvent(nged::GraphView* view, nged::StringView event) override
  {
    PYBIND11_OVERLOAD(void, nged::GraphView, onViewEvent, view, event);
  }

  virtual void please(nged::StringView request) override
  {
    PYBIND11_OVERLOAD(void, nged::GraphView, please, request);
  }

  virtual bool hasMenu() const override { PYBIND11_OVERLOAD(bool, nged::GraphView, hasMenu); }

  virtual void updateMenu() override { PYBIND11_OVERLOAD(void, nged::GraphView, updateMenu); }
};
// }}}

// ViewFactory {{{
class PyViewFactory : public nged::ViewFactory
{
  nged::ViewFactoryPtr fallback_ = nullptr;
  std::unordered_map<std::string, pybind11::function> pyFactories_;

public:
  PyViewFactory();

  void set(std::string name, pybind11::function func) { pyFactories_[name] = func; }
  nged::GraphViewPtr createView(
    nged::String const& kind,
    nged::NodeGraphEditor* editor,
    nged::NodeGraphDocPtr doc) const override;
};
// }}}

class PyImGuiNodeGraphEditor : public nged::ImGuiNodeGraphEditor
{
public:
  pybind11::object pyDocFactory;
  pybind11::object pyNodeFactory;
  pybind11::object pyItemFactory;
  pybind11::object pyParmModifiedCallback;
  std::unordered_set<pybind11::object, PyPtrHash<pybind11::object>, PyPtrEqual<pybind11::object>>
    pyDocs;
  std::unordered_set<pybind11::object, PyPtrHash<pybind11::object>, PyPtrEqual<pybind11::object>>
    pyViews;
  std::unordered_map<std::string, pybind11::object> pyCommands;

  PyImGuiNodeGraphEditor(
    pybind11::object docFactory,
    pybind11::object nodeFactory,
    pybind11::object itemFactory);
  ~PyImGuiNodeGraphEditor();

  virtual void beforeDocRemoved(DocPtr doc) override;

  void onParmModified(PyNode* node, parmscript::hashset<parmscript::string> const& modified);
  void addCommand(pybind11::object command);
  void removeCommand(nged::String const& name);
};

class PyApp : public nged::App
{
public:
  virtual char const* title() override { PYBIND11_OVERLOAD(char const*, nged::App, title); }
  bool agreeToQuit() override { PYBIND11_OVERLOAD(bool, nged::App, agreeToQuit); }
  void init() override;

  void update() override
  {
    try {
      PYBIND11_OVERLOAD_PURE(void, nged::App, update);
    } catch (std::exception const& e) {
      nged::MessageHub::errorf("python update caught error: {}", e.what());
    }
  }
  void quit() override { PYBIND11_OVERLOAD(void, nged::App, quit); }
};
// }}}
