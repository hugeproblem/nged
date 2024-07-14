#pragma once

#include "gmath.h"
#include "utils.h"
#include <nlohmann/json_fwd.hpp>
#include <spdlog/fmt/fmt.h> // TODO: maybe use std::format
#include <uuid.h>
#include <phmap.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

namespace nged { // {{{

using sint  = intptr_t;
using uint  = uintptr_t;
using Json  = nlohmann::json;
using Vec2  = gmath::Vec2;
using Mat3  = gmath::Mat3;
using AABB  = gmath::AABB;
using Color = gmath::sRGBColor;

template<class K, class V>
using HashMap = phmap::flat_hash_map<K, V>;
template<class K>
using HashSet = phmap::flat_hash_set<K>;
template<class T>
using Vector     = std::vector<T>;
using String     = std::string;
using StringView = std::string_view;
template<class T>
using Optional   = std::optional<T>;

// ItemID & Connection {{{
class ItemID
{
  union
  {
    uint64_t id_;
    struct
    {
      uint32_t random_;
      uint32_t index_;
    };
  };

public:
  constexpr ItemID(uint64_t id = -2) : id_{id} {}
  constexpr ItemID(uint32_t random, uint32_t index) : random_(random), index_(index) {}
  constexpr ItemID(ItemID const& that)            = default;
  constexpr ItemID& operator=(ItemID const& that) = default;

  // make it able to be map key
  bool operator<(ItemID const& that) const { return id_ < that.id_; }
  // make it able to be hash map key
  bool operator==(ItemID const& that) const { return id_ == that.id_; }
  bool operator!=(ItemID const& that) const { return id_ != that.id_; }

  size_t   hash() const { return std::hash<uint64_t>()(id_); }
  uint64_t value() const { return id_; }
  uint32_t index() const { return index_; }
};

static constexpr ItemID ID_None = {-1u, -1u};

/// Connection to an output port of source node
struct InputConnection
{
  ItemID sourceItem{ID_None};
  sint   sourcePort{-1};

  bool operator==(InputConnection const& that) const
  {
    return sourceItem == that.sourceItem && sourcePort == that.sourcePort;
  }
};

/// Connection to an input port of destiny node
struct OutputConnection
{
  ItemID destItem{ID_None};
  sint   destPort{-1};

  bool operator==(OutputConnection const& that) const
  {
    return destItem == that.destItem && destPort == that.destPort;
  }
};

struct NodePin
{
  ItemID node  = ID_None;
  sint   index = -1;
  enum class Type
  {
    None,
    In,
    Out
  } type = Type::None;
  bool operator==(NodePin const& that) const
  {
    return node == that.node && index == that.index && type == that.type;
  }
  bool operator!=(NodePin const& that) const { return !operator==(that); }
};

static constexpr NodePin PIN_None = {ID_None, -1, NodePin::Type::None};
// }}}

} // }}} namespace nged

// std::hash {{{
template<>
struct std::hash<nged::ItemID>
{
  size_t operator()(nged::ItemID id) const { return id.hash(); }
};

template<>
struct std::hash<nged::OutputConnection>
{
  size_t operator()(nged::OutputConnection const& c) const
  {
    return c.destItem.hash() ^ std::hash<nged::sint>()(c.destPort * 2021);
  }
};
// }}}

namespace nged {

class Graph;
class GraphItem;
class Node;
class Link;
class Router;
class ResizableBox;
class GroupBox;
class GraphItemFactory;
class NodeGraphDoc;
class Canvas;
class NodeFactory;

using GraphItemPtr        = std::shared_ptr<GraphItem>;
using NodePtr             = std::shared_ptr<Node>;
using LinkPtr             = std::shared_ptr<Link>;
using RouterPtr           = std::shared_ptr<Router>;
using GraphPtr            = std::shared_ptr<Graph>;
using WeakGraphPtr        = std::weak_ptr<Graph>;
using NodeGraphDocPtr     = std::shared_ptr<NodeGraphDoc>;
using NodeFactoryPtr      = std::shared_ptr<NodeFactory>;
using GraphItemFactoryPtr = std::shared_ptr<GraphItemFactory>;
using UID                 = uuids::uuid;

// UID Related {{{
UID generateUID();

inline UID uidFromString(StringView str)
{
  return uuids::uuid::from_string(str).value();
}

inline String uidToString(UID uid)
{
  return uuids::to_string(uid);
}
// }}}

// MessageHub {{{
class MessageHub
{
public:
  ~MessageHub() = default;
  enum class Category : int
  {
    Log = 0,
    Notice,
    Output,

    Count
  };
  enum class Verbosity
  {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,

    Text,

    Count
  };
  using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
  struct Message
  {
    Message(String s, Verbosity v, TimePoint t) : content(std::move(s)), verbosity(v), timestamp(t)
    {
    }
    Message(Message&& m) = default;

    String    content;
    Verbosity verbosity;
    TimePoint timestamp;
  };

  void addMessage(String message, Category category, Verbosity verbose);
  void clear(Category category);
  void clearAll();
  void setCountLimit(size_t count);

  template<class F>
  void foreach (Category category, F && func) const
  {
    std::shared_lock lock(mutex_);
    for (auto&& s : messageCategories_[static_cast<int>(category)]) {
      func(s);
    }
  }
  template<class F>
  void forrange(Category category, F&& func, size_t offset, size_t count = -1) const
  {
    std::shared_lock lock(mutex_);
    auto const&      queue = messageCategories_[static_cast<int>(category)];
    for (size_t i = offset,
                n = count == -1 ? queue.size() : std::min(offset + count, queue.size());
         i < n;
         ++i) {
      func(queue[i]);
    }
  }
  size_t count(Category category) const
  {
    std::shared_lock lock(mutex_);
    return messageCategories_[static_cast<int>(category)].size();
  }

  static MessageHub& instance() { return instance_; }

protected:
  std::deque<Message>       messageCategories_[static_cast<int>(Category::Count)];
  mutable std::shared_mutex mutex_;
  size_t                    countLimit_ = 4096;

  static MessageHub instance_;

private:
  MessageHub()                       = default;
  MessageHub(MessageHub const& that) = delete;

public: // helpers
#define EMIT_MESSAGE_(msg, cat, verb) \
  MessageHub::instance().addMessage((msg), Category::cat, Verbosity::verb)
#define DEFINE_MSG_VARIANT_(func, cat, verb)                                        \
  static inline void func(String msg) { EMIT_MESSAGE_(std::move(msg), cat, verb); } \
  template<class... T>                                                              \
  static inline void func##f(T... args)                                             \
  {                                                                                 \
    EMIT_MESSAGE_(fmt::format(std::forward<T>(args)...), cat, verb);                \
  }
  DEFINE_MSG_VARIANT_(trace, Log, Trace)
  DEFINE_MSG_VARIANT_(debug, Log, Debug)
  DEFINE_MSG_VARIANT_(info, Log, Info)
  DEFINE_MSG_VARIANT_(warn, Log, Warning)
  DEFINE_MSG_VARIANT_(error, Log, Error)
  DEFINE_MSG_VARIANT_(fatal, Log, Fatal)
  DEFINE_MSG_VARIANT_(notice, Notice, Text)
  DEFINE_MSG_VARIANT_(output, Output, Text)
#undef DEFINE_MSG_VARIANT_
#undef EMIT_MESSAGE_
};
// }}} MessageHub

// Graph Item & Registry {{{
enum class GraphItemState
{
  DEFAULT = 0,
  HOVERED,
  SELECTED,
  PRESSED,
  DISABLED,
  DESELECTED, // when de-selecting
};

class Dyeable // has color
{
public:
  virtual ~Dyeable() {}
  virtual Color color() const    = 0;
  virtual void  setColor(Color c) = 0;
  virtual bool  hasSetColor() const { return false; }
};

class GraphItem : public std::enable_shared_from_this<GraphItem>
{
private:
  friend class Graph;
  friend class GraphItemFactory;

  Graph* parent_;
  ItemID id_        = ID_None;
  UID    uid_       = {};
  UID    sourceUID_ = {}; // the uid read from deserialize(); if not deserialize inplace, sourceUid
                          // will not be equal to uid

protected:
  size_t factory_ = -1;

  AABB aabb_ = {{0, 0}, {0, 0}}; // local aabb
  Vec2 pos_  = {0, 0};           // position

  void resetID(ItemID id) { id_ = id; }

  GraphItem(GraphItem const&) = delete;
  GraphItem(GraphItem&&)      = delete;

public:
  GraphItem(Graph* parent);
  virtual ~GraphItem() = default;

  virtual bool serialize(Json&) const;
  virtual bool deserialize(Json const&);

  virtual void settled() {} // deserialized and added to graph, and id is known

  /// draw myself
  virtual void draw(Canvas*, GraphItemState state) const {}

  /// hit test
  virtual bool hitTest(Vec2 point) const { return localBound().contains(point - pos_); }
  virtual bool hitTest(AABB box) const { return box.intersects(aabb()); }
  /// hit test order for single selection
  virtual int zOrder() const { return 0; }

  /// return: moved or not
  virtual bool moveTo(Vec2 to)
  {
    pos_ = to; // Vec2(std::round(to.x), std::round(to.y));
    return true;
  }

  /// return: can be moved or not
  virtual bool canMove() const { return true; }

  /// inside a graph everything have an unique id
  ItemID id() const { return id_; }

  UID  uid() const { return uid_; }
  void setUID(UID const& uid);
  UID  sourceUID() const { return sourceUID_; }

  /// for culling and _broad phase_ hit test
  virtual AABB localBound() const { return aabb_; }
  AABB         aabb() const { return localBound().moved(pos_); }
  Vec2         pos() const { return pos_; }

  /// parent graph, can be nullptr for root node
  Graph* parent() const { return parent_; }

  /// special cast, to avoid dynamic_cast
  virtual Node*          asNode() { return nullptr; }
  virtual Node const*    asNode() const { return nullptr; }
  virtual Link*          asLink() { return nullptr; }
  virtual Link const*    asLink() const { return nullptr; }
  virtual Router*        asRouter() { return nullptr; }
  virtual Router const*  asRouter() const { return nullptr; }
  virtual Dyeable*       asDyeable() { return nullptr; }
  virtual Dyeable const* asDyeable() const { return nullptr; }
  virtual ResizableBox*  asResizable() { return nullptr; }
  virtual GroupBox*      asGroupBox() { return nullptr; }
};

/// This class handles creation of graph items like nodes / comments / links and so on
/// you may also register custom graph items
class GraphItemFactory
{
  struct Factory
  {
    std::function<GraphItemPtr(Graph*)> creator;
    String                              name;
    bool                                userCreatable;
  };

  Vector<Factory>         factories_;
  HashMap<String, size_t> factoryIDs_;
  size_t                  nextFactoryId_ = 0;

public:
  virtual ~GraphItemFactory()               = default;
  GraphItemFactory()                        = default;
  GraphItemFactory(GraphItemFactory const&) = default;

  /// add or reset
  /// best to be called before anything wos created by any factory
  /// when there already exist items created with old factory method
  /// then will be created with new factory method during serialization (e.g., undo, redo, paste)
  virtual void
  set(String const& name, bool userCreatable, std::function<GraphItemPtr(Graph*)> factory)
  {
    if (auto existing = factoryIDs_.find(name); existing != factoryIDs_.end()) {
      factories_[existing->second] = {factory, name, userCreatable};
    } else {
      factoryIDs_[name] = nextFactoryId_;
      factories_.push_back({factory, name, userCreatable});
      assert(factories_.size() == nextFactoryId_ + 1);
      ++nextFactoryId_;
    }
  }
  virtual GraphItemPtr make(Graph* parent, String const& name) const
  {
    if (auto itr = factoryIDs_.find(name); itr != factoryIDs_.end()) {
      auto id = itr->second;
      if (id >= factories_.size())
        return nullptr;
      auto factory = factories_[id].creator;
      if (!factory)
        return nullptr;
      auto ptr = factory(parent);
      if (ptr)
        ptr->factory_ = id;
      return ptr;
    } else {
      return nullptr;
    }
  }
  virtual Vector<String> listNames(bool onlyUserCreatable = true) const
  {
    Vector<String> names;
    for (auto& f : factories_)
      if (f.userCreatable || !onlyUserCreatable)
        names.push_back(f.name);
    return names;
  }
  virtual String factoryName(GraphItemPtr item) const
  {
    if (item && item->factory_ < factories_.size())
      return factories_[item->factory_].name;
    else if (item && item->asNode())
      return "node";
    return "";
  }
  virtual void discard(Graph* graph, GraphItem* item) const {
  } // called when item was created, but not added to graph, and not needed anymore
};

GraphItemFactoryPtr defaultGraphItemFactory();
// }}} Graph Item & Registry

// Node {{{
enum class IconType
{
  IconFont,
  Text,
  // TODO:
  // SVG,
  // BitMap,
};

class TypedNode;

static constexpr uint64_t NODEFLAG_BYPASS = 1ull;

/// Node Data Model
class Node
    : public GraphItem
    , public Dyeable
{
protected:
  String   type_;
  String   name_;
  Color    color_;
  uint64_t flags_ = 0;

public:
  Node(Graph* parent, String type = "", String name = "");
  virtual ~Node() {}

  // For UI:
  virtual Vec2 inputPinPos(sint i) const;
  virtual Vec2 inputPinDir(sint i) const { return Vec2{0, -1}; } // hints the link direction
  virtual Vec2 outputPinPos(sint i) const;
  virtual Vec2 outputPinDir(sint i) const { return Vec2{0, 1}; } // hints the link direction
  virtual Color inputPinColor(sint i) const;
  virtual Color outputPinColor(sint i) const;
  // if there are too many input pins or unlimited number of input pins, they will be merged
  // returns true if the node has merged input pin, the `bound` argument will be hold the bounding
  // box else  return false, the `bound` will be untouched
  virtual bool mergedInputBound(AABB& bound) const;
  virtual bool getNodeDescription(String& desc) const { return false; }
  virtual bool getInputDescription(sint port, String& desc) const { return false; }
  virtual bool getOutputDescription(sint port, String& desc) const { return false; }
  virtual bool getIcon(IconType& type, StringView& content) const { return false; }

  virtual void draw(Canvas* canvas, GraphItemState state) const override;

  // Data Model:
  /// numMaxInputs: returning negative value means unlimited number of inputs
  virtual sint numMaxInputs() const { return 1; }
  /// numFixedInputs: how many input pins are fixed
  ///                 for nodes who have unlimited number of inputs, first `numFixedInputs` pins
  ///                 will be independent, while following pins while be merged
  virtual sint numFixedInputs() const { return 0; }
  /// if input `port` requires to be connected
  virtual bool isRequiredInput(sint port) const { return false; }
  /// number of outputs
  virtual sint numOutputs() const { return 1; }
  /// if my input port `port` accept output from `sourceNode`.`sourcePort`
  virtual bool acceptInput(sint port, Node const* sourceNode, sint sourcePort) const
  {
    return true;
  }
  virtual sint getPinForIncomingLink(ItemID sourceItem, sint sourcePin) const { return numMaxInputs() > 0? 0 : -1; }

  /// try to rename this node to `desired`
  /// if `desired` is not acceptable but the renaming can be done nevertheless, `accepted` should
  /// be set to the new accepted name if renaming was done, return true otherwise return false, and
  /// the name is unchanged
  virtual bool rename(String const& desired, String& accepted)
  {
    name_ = accepted = desired;
    return true;
  }

  /// resize the node, if `numMaxInputs()<0 and varPinWidth > 0 and varPinWidth > 0`, the width
  /// of this node will be calculated by formula
  /// `max(width, numConnectedInputs * varPinWidth + varMarginWidth)`,
  /// otherwise the size is fixed as {width, height}
  virtual void
  resize(float width, float height, float varPinWidth = 0.f, float varMarginWidth = 0.f);

  /// the text to be drawn beside this node
  virtual StringView label() const { return name(); }

  /// get dependencies beside input connections, return number of extra dependencies
  virtual size_t getExtraDependencies(Vector<ItemID>& deps) { return 0; }

  virtual bool serialize(Json& json) const override;
  virtual bool deserialize(Json const& json) override;

  virtual Color color() const override { return color_; }
  virtual void  setColor(Color c) override { color_ = c; }
  virtual bool  hasSetColor() const override;

  virtual uint64_t flags() const { return flags_; }
  virtual void     setFlags(uint64_t flags) { flags_ = flags; }
  virtual bool     isFlagApplicatable(uint64_t flag, String* outReason = nullptr) const { return true; }

  virtual Node const*      asNode() const override { return this; }
  virtual Node*            asNode() override { return this; }
  virtual Dyeable const*   asDyeable() const override { return this; }
  virtual Dyeable*         asDyeable() override { return this; }
  virtual Graph const*     asGraph() const { return nullptr; }
  virtual Graph*           asGraph() { return nullptr; }
  virtual TypedNode const* asTypedNode() const { return nullptr; }
  virtual TypedNode*       asTypedNode() { return nullptr; }

  // Interface:
  String const& type() const { return type_; }
  String const& name() const { return name_; }
  sint          getLastConnectedInputPort() const;
  bool          getInput(sint inPort, NodePtr& nodeptr, sint& outPort) const;
};

class TypeSystem
{
  Vector<String>        types_;
  sint                  nextTypeIndex_ = 0;
  HashMap<String, sint> typeIndex_;
  HashMap<String, sint> typeBaseType_;
  HashMap<std::pair<sint, sint>, bool> typeConvertable_;
  HashMap<sint, Color>  typeColorHints_;

  TypeSystem() = default;
  TypeSystem(TypeSystem const&) = delete;

public:
  ~TypeSystem() = default;
  using TypeIndex = sint;
  static constexpr TypeIndex InvalidTypeIndex = -1;
  static TypeSystem& instance();

  TypeIndex       registerType(StringView type, StringView baseType="", Color hintColor=Color{0,0,0,0});
  void            setConvertable(StringView from, StringView to, bool convertable = true);
  bool            isConvertable(StringView from, StringView to) const;
  bool            isType(StringView type) const;
  TypeIndex       typeIndex(StringView type) const;
  sint            typeCount() const;
  StringView      typeName(TypeIndex index) const;
  StringView      typeBaseType(TypeIndex index) const;
  void            setColorHint(TypeIndex index, Color hint);
  Optional<Color> colorHint(TypeIndex index) const;
  void            setColorHint(StringView type, Color hint) { setColorHint(typeIndex(type), hint); }
  Optional<Color> colorHint(StringView type) const { return colorHint(typeIndex(type)); }
};

/// Node with type checking, accept input only if `typeConvertable(sourceNode->outputType(sourcePort), inputType(port))` returns true
class TypedNode : public Node
{
protected:
  Vector<String> inputTypes_;
  Vector<String> outputTypes_;

public:
  TypedNode(Graph* parent, String type, String name, Vector<String> inputTypes, Vector<String> outputTypes):
    Node(parent, type, name)
  {
    inputTypes_ = std::move(inputTypes);
    outputTypes_ = std::move(outputTypes);
  }

  StringView inputType(sint i) const;
  StringView outputType(sint i) const;

  virtual TypedNode const* asTypedNode() const override { return this; }
  virtual TypedNode*       asTypedNode() override { return this; }

  virtual Color inputPinColor(sint i) const override;
  virtual Color outputPinColor(sint i) const override;

  virtual bool acceptInput(sint port, Node const* sourceNode, sint sourcePort) const override;
  virtual sint getPinForIncomingLink(ItemID sourceItem, sint sourcePin) const override;
};

/// The factory that makes root graph and nodes
class NodeFactory
{
public:
  virtual ~NodeFactory() {}

  // node model
  virtual GraphPtr createRootGraph(NodeGraphDoc* doc) const         = 0;
  virtual NodePtr  createNode(Graph* parent, StringView type) const = 0;

  // list of available node types
  virtual void listNodeTypes(
    Graph* parent,
    void*  context,
    void (*)(void* context, StringView category, StringView type, StringView name)) const = 0;

  // meta info
  virtual bool getNodeIcon(
    StringView      type,
    uint8_t const** iconDataPtr,
    size_t*         iconSize,
    IconType*       iconType) const
  {
    return false;
  }
  virtual bool getCategoryIcon(
    StringView      category,
    uint8_t const** iconDataPtr,
    size_t*         iconSize,
    IconType*       iconType) const
  {
    return false;
  }
  // if the icon data is not dynamically allocated, this function can be left as null
  virtual void freeIconData(uint8_t const** iconDataPtr) const {}

  virtual void discard(Graph* graph, Node* node) const {
  } // called when node was created, but not added to graph, and not needed anymore
};
// }}} Node

// Link {{{
class Link : public GraphItem
{
  OutputConnection output_;
  InputConnection  input_;
  Vector<Vec2>     path_;

public:
  Link(Graph* parent, InputConnection input, OutputConnection output)
      : GraphItem(parent), output_(output), input_(input)
  {
    calculatePath();
  }
  ~Link() = default;

  OutputConnection const& output() const { return output_; }
  InputConnection const&  input() const { return input_; }
  auto const&             path() const { return path_; }

  virtual bool hitTest(Vec2 pt) const override;
  virtual bool hitTest(AABB bb) const override;
  virtual bool canMove() const override { return false; }
  virtual bool moveTo(Vec2 to) override { return false; }

  virtual void draw(Canvas* canvas, GraphItemState state) const override;
  virtual void calculatePath();
  virtual bool serialize(Json& json) const override;
  virtual bool deserialize(Json const& json) override;

  virtual Link*       asLink() override { return this; }
  virtual Link const* asLink() const override { return this; }
};
// }}} Link

// Router {{{
class Router
    : public GraphItem
    , public Dyeable
{
  Color color_;
  Optional<Color> linkColor_;

public:
  Router(Graph* parent);

  virtual Color color() const override { return color_; }
  virtual void  setColor(Color c) override { color_ = c; }
  
  Color linkColor() const { return linkColor_.value_or(color_); } // for typed nodes, link color be the hint for type
  void  setLinkColor(Color c) { linkColor_ = c; }

  virtual bool hitTest(Vec2 point) const override;
  virtual void draw(Canvas* canvas, GraphItemState state) const override;

  virtual Router const*  asRouter() const override { return this; }
  virtual Router*        asRouter() override { return this; }
  virtual Dyeable const* asDyeable() const override { return this; }
  virtual Dyeable*       asDyeable() override { return this; }

  virtual bool serialize(Json& json) const override;
  virtual bool deserialize(Json const& json) override;

  bool getNodeSource(Node*& node, sint& pin) const;
};
// }}} Router

// ResizableBox {{{
class ResizableBox : public GraphItem
{
public:
  ResizableBox(Graph* parent) : GraphItem(parent) {}

  virtual void setBounds(AABB absoluteBounds)
  {
    moveTo(absoluteBounds.center());
    aabb_ = absoluteBounds.moved(-pos_);
  }

  virtual ResizableBox* asResizable() override { return this; }
};
// }}} ResizableBox

// GroupBox {{{
// Group of items
class GroupBox
    : public ResizableBox
    , public Dyeable
{
  Color           backgroundColor_;
  HashSet<ItemID> containingItems_;

  void rescanContainingItems();

public:
  GroupBox(Graph* parent);

  auto const& containingItems() const { return containingItems_; }
  void        setContainingItems(HashSet<ItemID> ids) { containingItems_ = std::move(ids); }
  void        remapItems(HashMap<size_t, ItemID> const& idmap); // idmap: old id -> new id, used mainly for pasting
  void        insertItem(ItemID id);
  void        eraseItem(ItemID id);

  virtual bool  hitTest(AABB box) const override;
  virtual bool  hitTest(Vec2 point) const override;
  virtual void  setBounds(AABB absoluteBounds) override;
  virtual int   zOrder() const override { return -2; }
  virtual bool  moveTo(Vec2 to) override;
  virtual void  draw(Canvas* canvas, GraphItemState state) const override;
  virtual Color color() const override { return backgroundColor_; }
  virtual void  setColor(Color c) override { backgroundColor_ = c; }
  virtual bool  serialize(Json& json) const override;
  virtual bool  deserialize(Json const& json) override;

  virtual Dyeable const* asDyeable() const override { return this; }
  virtual Dyeable*       asDyeable() override { return this; }
  virtual GroupBox*      asGroupBox() override { return this; }
};
// }}} GroupBox

// CommentBox {{{
class CommentBox
    : public ResizableBox
    , public Dyeable
{
  Color        color_;
  Color        backgroundColor_;
  mutable Vec2 textSize_ = {0, 0};

protected:
  String text_;

public:
  CommentBox(Graph* parent);

  Color const& backgroundColor() const { return backgroundColor_; }
  void         setBackgroundColor(Color const& c) { backgroundColor_ = c; }

  auto const& text() const { return text_; }
  void        setText(String text);

  virtual Color color() const override { return color_; }
  virtual void  setColor(Color c) override
  {
    color_           = c;
    backgroundColor_ = c;
    backgroundColor_.r /= 2;
    backgroundColor_.g /= 2;
    backgroundColor_.b /= 2;
    backgroundColor_.a /= 3;
  }

  virtual Dyeable const* asDyeable() const override { return this; }
  virtual Dyeable*       asDyeable() override { return this; }

  virtual AABB localBound() const override;
  virtual int  zOrder() const override { return -1; }
  virtual void draw(Canvas* canvas, GraphItemState state) const override;

  virtual bool serialize(Json& json) const override;
  virtual bool deserialize(Json const& json) override;
};
// }}} Comment

// Arrow {{{
class Arrow
    : public GraphItem
    , public Dyeable
{
  Color color_;
  Vec2  start_;
  Vec2  end_;
  float thickness_ = 2.0f;
  float tipSize_   = 10.0f;

public:
  Arrow(Graph* parent);

  virtual bool hitTest(Vec2 pt) const override;
  virtual bool hitTest(AABB bb) const override;
  virtual AABB localBound() const override;
  virtual int  zOrder() const override { return -1; }

  virtual void draw(Canvas* canvas, GraphItemState state) const override;
  virtual bool serialize(Json& json) const override;
  virtual bool deserialize(Json const& json) override;

  virtual Color color() const override { return color_; }
  virtual void  setColor(Color c) override { color_ = c; }

  virtual Dyeable const* asDyeable() const override { return this; }
  virtual Dyeable*       asDyeable() override { return this; }

  Vec2  start() const { return start_ + pos_; }
  Vec2  end() const { return end_ + pos_; }
  void  setStart(Vec2 p) { start_ = p - pos_; }
  void  setEnd(Vec2 p) { end_ = p - pos_; }
  float thickness() const { return thickness_; }
  void  setThickness(float t) { thickness_ = t; }
  float tipSize() const { return tipSize_; }
  void  setTipSize(float s) { tipSize_ = s; }
};
// }}} Arrow

// Graph {{{
// GraphTraverseResult {{{
class GraphTraverseResult
{
  friend class Graph;

  struct Range
  {
    size_t begin = -1, end = -1;
  };
  struct NodeClosure
  {
    size_t node;
    Range  inputs;
    Range  outputs;
  };

protected:
  Vector<size_t> inputs_;  // index into nodes_,
                           // e.g., for inputs_ of value [1,2,4,12,1]
                           // if closures_[0].inputs == {0,3} then the inputs of nodes_[0] are
                           // nodes_[1], nodes_[2] and nodes_[4]
  Vector<size_t> outputs_; // same as above, if outputs_ == [4,2,1,3] and
                           // closures_[0].outputs == {2,3} then the output of nodes_[0] is
                           // nodes_[1]
  Vector<NodePtr>         nodes_;
  Vector<NodeClosure>     closures_;
  HashMap<ItemID, size_t> idmap_; // map id to accessor

public:
  size_t size() const { return closures_.size(); }
  size_t count() const { return closures_.size(); }
  Node*  node(size_t nthNode) const { return nodes_[nthNode].get(); }

  int inputCount(size_t nthNode) const
  {
    auto const& irange = closures_[nthNode].inputs;
    return static_cast<int>(irange.end - irange.begin);
  }
  int outputCount(size_t nthNode) const
  {
    auto const& orange = closures_[nthNode].outputs;
    return static_cast<int>(orange.end - orange.begin);
  }
  sint inputIndexOf(size_t nthNode, int nthInput) const
  {
    auto const& irange = closures_[nthNode].inputs;
    auto        icnt   = irange.end - irange.begin;
    if (nthInput < 0)
      nthInput += icnt;
    if (nthInput < 0 || nthInput >= icnt)
      return -1;
    return sint(inputs_[irange.begin + nthInput]);
  }
  Node* inputOf(size_t nthNode, int nthInput) const
  {
    auto idx = inputIndexOf(nthNode, nthInput);
    if (idx < 0 || idx >= nodes_.size())
      return nullptr;
    return nodes_[idx].get();
  }
  sint outputIndexOf(size_t nthNode, int nthOutput) const
  {
    auto const& orange = closures_[nthNode].outputs;
    auto        ocnt   = orange.end - orange.begin;
    if (nthOutput < 0)
      nthOutput += ocnt;
    if (nthOutput < 0 || nthOutput >= ocnt)
      return -1;
    return sint(outputs_[orange.begin + nthOutput]);
  }
  Node* outputOf(size_t nthNode, int nthOutput) const
  {
    auto idx = outputIndexOf(nthNode, nthOutput);
    if (idx < 0 || idx >= nodes_.size())
      return nullptr;
    return nodes_[idx].get();
  }

  class Accessor
  {
  protected:
    GraphTraverseResult const& container_;
    size_t                     idx_;

  public:
    Accessor(GraphTraverseResult const* container, size_t idx) : container_(*container), idx_(idx)
    {
    }
    Accessor(Accessor&&)      = default;
    Accessor(Accessor const&) = default;
    bool operator==(Accessor const& that) const
    {
      return idx_ == that.idx_ && &container_ == &that.container_;
    }

    bool   valid() const { return idx_ != size_t(-1) && idx_ < container_.size(); }
    size_t index() const { return idx_; }
    Node*  operator->() const { return container_.node(idx_); }
    Node*  node() const { return container_.node(idx_); }
    int    inputCount() const { return container_.inputCount(idx_); }
    int    outputCount() const { return container_.outputCount(idx_); }
    Node*  input(int n) const { return container_.inputOf(idx_, n); }
    Node*  output(int n) const { return container_.outputOf(idx_, n); }
    sint   inputIndex(int n) const { return container_.inputIndexOf(idx_, n); }
    sint   outputIndex(int n) const { return container_.outputIndexOf(idx_, n); }
  };

  class Iterator : private Accessor
  {
  public:
    Iterator(GraphTraverseResult const* container, size_t idx) : Accessor(container, idx) {}
    Iterator(Iterator&&)      = default;
    Iterator(Iterator const&) = default;
    bool      operator==(Iterator const& that) const { return this->Accessor::operator==(*that); }
    bool      operator!=(Iterator const& that) const { return !this->Accessor::operator==(*that); }
    Iterator& operator++()
    {
      ++idx_;
      return *this;
    }
    Accessor const& operator*() const { return *this; }
    Accessor const* operator->() const { return this; }
  };
  Iterator begin() const { return Iterator{this, 0}; }
  Iterator end() const { return Iterator{this, closures_.size()}; }
  Accessor operator[](size_t i) const { return Accessor{this, i}; }
  Accessor find(ItemID id) const { return Accessor{this, utils::get_or(idmap_, id, size_t(-1))}; }
};
// }}} GraphTraverseResult

class NodeGraphDoc;
class Graph : public std::enable_shared_from_this<Graph>
{
protected:
  HashSet<ItemID>                            items_;
  HashMap<OutputConnection, InputConnection> links_; // OutputConnection -> InputConnection
  HashMap<OutputConnection, ItemID>          linkIDs_;
  NodeGraphDoc*                              docRoot_ = nullptr;
  Graph*                                     parent_;
  bool                                       readonly_ = false;
  String                                     name_;

  friend class NodeGraphEditor;
  friend class NodeGraphDoc;

protected:
  // making sure that inputs into node with variable input count always takes index [0, n)
  virtual void regulateVariableInput(Node* node);

  void doRemoveNoCheck(ItemID item);

public:
  Graph(NodeGraphDoc* root, Graph* parent, String name)
      : docRoot_(root), parent_(parent), name_(std::move(name))
  {
  }
  virtual ~Graph();

  NodeGraphDoc* docRoot() const { return docRoot_; }
  void          setRootless() { docRoot_ = nullptr; }
  Graph*        parent() const { return parent_; }
  String const& name() const { return name_; }
  void          rename(String newname) { name_ = std::move(newname); }
  auto const&   items() const { return items_; }
  auto const&   allLinks() const { return links_; }
  bool          readonly() const;
  bool          selfReadonly() const { return readonly_; }
  void          setSelfReadonly(bool ro) { readonly_ = ro; }

  Vec2    pinPos(NodePin pin) const;
  Vec2    pinDir(NodePin pin) const;
  Color   pinColor(NodePin pin) const;
  LinkPtr getLink(ItemID destItem, sint destPort);
  bool    getLinkSource(ItemID destItem, sint destPort, InputConnection& inConnection);
  bool
  getLinkDestiny(ItemID sourceItem, sint sourcePort, Vector<OutputConnection>& outConnections);
  bool    linksOnNode(ItemID nodeID, Vector<ItemID>& relatedLinks);
  void    updateLinkPaths(HashSet<ItemID> const& items); // re-calculate link paths
  NodePtr createNode(StringView type);                   // friendly API to create a node

  virtual ItemID       add(GraphItemPtr item);
  virtual GraphItemPtr get(ItemID id) const;
  virtual GraphItemPtr tryGet(ItemID id) const;
  virtual bool    move(HashSet<ItemID> const& items, Vec2 const& delta); // return : anything moved
  virtual void    remove(HashSet<ItemID> const& items);
  virtual void    clear();
  virtual bool    checkLinkIsAllowed(ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort, NodePin* errorPin = nullptr); // check if sourceItem, sourcePort can connect to destItem, destPort; if not, the refusing-to-connect pin will be returned via last argument
  virtual LinkPtr setLink(ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort);
  virtual void    removeLink(ItemID destItem, sint destPort);

  bool checkLoopBottomUp(
    ItemID           target,
    Vector<ItemID>&  loop,
    HashSet<ItemID>* visitedWithNoLoop = nullptr);

  // travel the graph, if `topdown` then go from source to dests, otherwise tracing from dests to
  // sources
  bool traverse(
    GraphTraverseResult&  result,
    Vector<ItemID> const& startPoints,
    bool                  topdown,
    bool                  allowLoop = false);

  /// top-down, BFS, source nodes are guaranteed to be in front of their destinies:
  bool travelTopDown(GraphTraverseResult& result, ItemID sourceItem, bool allowLoop = false);
  bool travelTopDown(
    GraphTraverseResult&  result,
    Vector<ItemID> const& destItems,
    bool                  allowLoop = false);

  /// bottom-up, BFS, destiny nodes are guaranteed to be in front of their sources:
  bool travelBottomUp(GraphTraverseResult& result, ItemID destItem, bool allowLoop = false);
  bool travelBottomUp(
    GraphTraverseResult&  result,
    Vector<ItemID> const& destItems,
    bool                  allowLoop = false);

  template<class F>
  inline void forEachItem(F&& func) const
  {
    for (auto id : items_) {
      func(get(id));
    }
  }
  template<class F>
  inline void forEachLink(F&& func) const
  {
    for (auto linkpair : linkIDs_) {
      auto linkitem = get(linkpair.second);
      func(std::static_pointer_cast<Link>(linkitem));
    }
  }

  virtual NodeFactory const* nodeFactory() const;
  /// calculates a path from start to end
  virtual Vector<Vec2> calculatePath(
    Vec2 start,
    Vec2 end,
    Vec2 startDir   = {0, 0},
    Vec2 endDir     = {0, 0},
    AABB startBound = {{0, 0}},
    AABB endBound   = {{0, 0}});
  virtual bool serialize(Json& json) const;
  virtual bool deserialize(Json const& json);
};
// }}} Graph

// Pool {{{
class GraphItemPool
{
  Vector<GraphItemPtr> items_;
  Vector<uint32_t>     freeList_;
  HashMap<UID, ItemID> uidMap_;
  std::mt19937         randGenerator_;

  GraphItemPool(GraphItemPool const&) = delete;
  GraphItemPool(GraphItemPool&&) = delete;

public:
  ~GraphItemPool() = default;
  GraphItemPool();

  /// insert a new item, return its id
  ItemID add(GraphItemPtr item);
  void release(ItemID id)
  {
    auto index = id.index();
    assert(index < items_.size() && items_[index]);
    auto item = items_[index];
    assert(item->id() == id);
    uidMap_.erase(item->uid());
    freeList_.push_back(index);
    items_[index] = nullptr;
  }
  GraphItemPtr get(ItemID id)
  {
    auto index = id.index();
    if (id == ID_None)
      return nullptr;
    assert(index < items_.size());
    auto itemptr = items_[index];
    if (!itemptr || itemptr->id() != id) // if id mismatch but index matches, means someone is
                                         // trying to get a dead item
      return nullptr;
    return itemptr;
  }
  GraphItemPtr get(UID const& uid)
  {
    if (auto itr = uidMap_.find(uid); itr != uidMap_.end())
      return get(itr->second);
    return nullptr;
  }
  void moveUID(UID const& oldUID, UID const& newUID);
  template<class F>
  void foreach (F f) const
  {
    for (auto&& item : items_) {
      if (item) {
        f(item);
      }
    }
  }

  size_t count() const { return items_.size() - freeList_.size(); }
};
// }}} Pool

// Doc {{{
class NodeGraphEditor;
class NodeGraphDocHistory
{
  struct Version
  {
    Vector<uint8_t> data; // TODO: store patch(diff) only(?)
    String          message;
    size_t          uncompressedSize = 0;
  };
  NodeGraphDoc*   doc_ = nullptr;
  Vector<Version> versions_; // full version, every commit creates a version, thus that a
                             // history tree can be obtained
  Vector<size_t> undoStack_; // linear edit versions, the working branch of histroy tree
  size_t         fileVersion_      = -1; // the version saved to file
  int32_t        indexAtUndoStack_ = -1;
  int32_t        atEditGroupLevel_ = 0; // current editing group - when leaving group of level 0,

  class EditGroup
  {
    NodeGraphDocHistory* history_;
    String               message_;

  public:
    EditGroup(NodeGraphDocHistory* history, String message)
        : history_(history), message_(std::move(message))
    {
      ++history_->atEditGroupLevel_;
    }
    ~EditGroup()
    {
      if (--history_->atEditGroupLevel_ == 0)
        history_->commit(std::move(message_));
    }
  }; // the whole group of editing while be commited

public:
  NodeGraphDocHistory(NodeGraphDoc* doc) : doc_(doc) { reset(false); }
  NodeGraphDocHistory(NodeGraphDocHistory const&) = delete;
  NodeGraphDocHistory(NodeGraphDocHistory&&) = delete;

public:
  ~NodeGraphDocHistory() = default;

  void   reset(bool createInitialCommit);
  size_t commit(String message); // return: version number
  size_t numCommits() const { return versions_.size(); }
  size_t memoryBytesUsed() const;
  bool   checkout(size_t version);
  bool   undo();
  bool   redo();
  void   prune();     // remove data from none-working branches
  void   markSaved(); // mark current version as saved

  size_t beginEditGroup() { return ++atEditGroupLevel_; }
  size_t endEditGroup(String message)
  {
    --atEditGroupLevel_;
    assert(atEditGroupLevel_ >= 0);
    return commitIfAppropriate(std::move(message));
  }
  size_t commitIfAppropriate(String message)
  {
    if (atEditGroupLevel_ == 0) {
      return commit(std::move(message));
    } else {
      return -1;
    }
  }
  EditGroup editGroup(String msg) { return EditGroup(this, std::move(msg)); }
};

/// NodeGraphDoc: the document containing root graph,
///               can be saved to / loaded from disk.
class NodeGraphDoc : public std::enable_shared_from_this<NodeGraphDoc>
{
  GraphItemPool       pool_;
  NodeGraphDocHistory history_;
  String              savePath_ = "";
  String              title_    = "untitled";
  bool                dirty_    = false;
  bool                readonly_ = false;
  bool                deserializeInplace_ =
    true; // if true, uid will be used for match items in place, otherwise, uids will be new
  GraphItemFactory const* itemFactory_ = nullptr;
  NodeFactoryPtr          nodeFactory_;

  std::function<void(Graph*)> graphModifiedNotifier_;
  
protected:
  GraphPtr       root_ = nullptr;
  GraphItemPool& itemPool() { return pool_; }
  NodeGraphDoc(NodeGraphDoc&&) = delete;
  NodeGraphDoc(NodeGraphDoc const&) = delete;
  NodeGraphDoc& operator=(NodeGraphDoc&&) = delete;

public:
  // before loading / saving content into file, do these transforms
  // filterFileInput expects to return a valid JSON string
  // filterFileOutput will recieve JSON string as input
  virtual String filterFileInput(StringView fileContent) { return String(fileContent); }
  virtual String filterFileOutput(StringView fileContent) { return String(fileContent); }

  virtual ItemID       addItem(GraphItemPtr item) { return pool_.add(std::move(item)); }
  virtual GraphItemPtr getItem(ItemID id) { return pool_.get(id); }
  virtual void         removeItem(ItemID id) { pool_.release(id); }
  virtual size_t       numItems() const { return pool_.count(); }
  virtual void moveUID(UID const& oldUID, UID const& newUID) { pool_.moveUID(oldUID, newUID); }
  virtual void makeRoot();

  NodeGraphDoc(NodeFactoryPtr nodeFactory, GraphItemFactory const* itemFactory);
  virtual ~NodeGraphDoc();

  StringView title() const;
  StringView savePath() const { return savePath_; }
  GraphPtr   root() const { return root_; }
  bool       open(String path);
  void       close();
  bool       save();              /// save to `savePath_`
  bool       saveAs(String path); /// save to `path` and remember `savePath_`
  bool       saveTo(String path); /// save to `path` without rembering `savePath_`
  bool       dirty() const { return dirty_; }
  bool       readonly() const { return readonly_; }
  void       setReadonly(bool readonly) { readonly_ = readonly; }
  bool       empty() const { return pool_.count() == 0; }
  bool       everEdited() const
  {
    return history_.numCommits() > 1;
  } // new / load doc will create an initial commit
  void                    touch() { dirty_ = true; }
  void                    untouch() { dirty_ = false; }
  void                    undo() { history_.undo(); }
  void                    redo() { history_.redo(); }
  NodeFactory const*      nodeFactory() const { return nodeFactory_.get(); }
  GraphItemFactory const* itemFactory() const { return itemFactory_; }
  auto&                   history() { return history_; }
  GraphItemPtr            findItemByUID(UID const& uid) { return pool_.get(uid); }

  void setDeserializeInplace(bool dsi) { deserializeInplace_ = dsi; }
  bool deserializeInplace() const { return deserializeInplace_; }
  auto editGroup(String message) { return history_.editGroup(std::move(message)); }

  void setModifiedNotifier(std::function<void(Graph*)> func)
  {
    graphModifiedNotifier_ = std::move(func);
  }
  void notifyGraphModified(Graph* graph);
};
// }}} Doc

// Canvas {{{
class Canvas
{
public:
  enum class TextAlign
  {
    Left,
    Center,
    Right
  };
  enum class TextVerticalAlign
  {
    Top,
    Center,
    Bottom
  };
  enum class FontFamily
  {
    Serif,
    SansSerif,
    Mono,
    Icon
  };
  enum class FontStyle
  {
    Regular,
    Italic,
    Strong
  };
  enum class FontSize
  {
    Normal,
    Small,
    Large
  };
  enum class Layer : int
  { // z-order
    Lower = 0,
    Low,
    Standard,
    High,
    Higher,

    Count
  };

  struct ShapeStyle
  {
    bool     filled;
    uint32_t fillColor; // RGBA
    float    strokeWidth;
    uint32_t strokeColor; // RGBA
  };
  static constexpr ShapeStyle defaultShapeStyle = {true, 0xff0000ff, 0.f, 0xffffffff};
  struct TextStyle
  {
    TextAlign         align;
    TextVerticalAlign valign;
    FontFamily        font;
    FontStyle         style;
    FontSize          size;
    uint32_t          color;
  };

  class Image
  {
  public:
    virtual ~Image() = default;
  };
  using ImagePtr = std::shared_ptr<Image>;

  static constexpr TextStyle defaultTextStyle = {
    TextAlign::Left,
    TextVerticalAlign::Center,
    FontFamily::SansSerif,
    FontStyle::Regular,
    FontSize::Normal,
    0xffffffff};

protected:
  // states
  Vec2  viewPos_        = {0, 0};
  Vec2  viewSize_       = {800, 600};
  float viewScale_      = 1.0f;
  Mat3  canvasToScreen_ = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  Mat3  screenToCanvas_ = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  Layer layer_          = Layer::Standard;

  // display options
  bool displayTypeHint_ = false;

  Vector<Layer> layerStack_ = {};

  virtual void updateMatrix()
  {
    canvasToScreen_ = Mat3::fromSRT(Vec2(viewScale_, viewScale_), 1.f, -viewPos_) *
                      Mat3::fromRTS(Vec2(1, 1), 0, viewSize_ * 0.5f);
    screenToCanvas_ = canvasToScreen_.inverse();
  }

public:
  Vec2  viewSize() const { return viewSize_; }
  Vec2  viewPos() const { return viewPos_; }
  float viewScale() const { return viewScale_; }
  Mat3  canvasToScreen() const { return canvasToScreen_; }
  Mat3  screenToCanvas() const { return screenToCanvas_; }
  bool  displayTypeHint() const { return displayTypeHint_; }
  void  setDisplayTypeHint(bool b) { displayTypeHint_ = b; }

  void setViewSize(Vec2 size) { viewSize_ = size; }
  void setViewPos(Vec2 pos)
  {
    viewPos_ = pos;
    updateMatrix();
  }
  void setViewScale(float scale)
  {
    viewScale_ = scale;
    updateMatrix();
  }
  void pushLayer(Layer layer)
  {
    layerStack_.push_back(layer_);
    setCurrentLayer(layer);
  }
  void popLayer()
  {
    assert(layerStack_.size() > 0);
    setCurrentLayer(layerStack_.back());
    layerStack_.pop_back();
  }
  static float floatFontSize(FontSize enumsize);

  virtual ~Canvas() = default;
  virtual AABB viewport() const
  {
    return AABB(screenToCanvas_.transformPoint({0, 0}), screenToCanvas_.transformPoint(viewSize_));
  }
  virtual Vec2 measureTextSize(StringView text, TextStyle const& style = defaultTextStyle)
    const                                                                                     = 0;
  virtual void setCurrentLayer(Layer layer)                                                   = 0;
  virtual void drawLine(Vec2 a, Vec2 b, uint32_t color = 0x000000ff, float width = 1.f) const = 0;
  virtual void drawRect(
    Vec2       topleft,
    Vec2       bottomright,
    float      cornerradius = 0,
    ShapeStyle style        = defaultShapeStyle) const = 0;
  virtual void drawCircle(
    Vec2       center,
    float      radius,
    int        nsegments = 0,
    ShapeStyle style     = defaultShapeStyle) const = 0;
  virtual void drawPoly(
    Vec2 const* pts,
    sint        numpt,
    bool        closed = true,
    ShapeStyle  style  = defaultShapeStyle) const = 0;
  virtual void drawText(Vec2 pos, StringView text, TextStyle const& style = defaultTextStyle)
    const = 0;
  virtual void drawTextUntransformed(
    Vec2             pos,
    StringView       text,
    TextStyle const& style = defaultTextStyle,
    float            scale = 1.f) const = 0;

  // `data` assumed to be 32-bit RGBA, with 8-bit per channel, and `width` x `height` in size
  static ImagePtr createImage(uint8_t const* data, int width, int height);
  // this function is implemented in the-canvas-you-are-going-to-use, e.g. ImGuiCanvas
  // TODO: maybe we should put it inside a polymorphic Resource class, so that
  //       we could support more than one type of Canvas at once

  // draws a rect at (pmin to pmax) with given image
  virtual void drawImage(ImagePtr image, Vec2 pmin, Vec2 pmax, Vec2 uvmin={0,0}, Vec2 uvmax={1,1}) const = 0;
};
// }}} Canvas

} // namespace nged
