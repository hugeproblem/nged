#include <nged/ngdoc.h>
#include <nged/style.h>
#include <nged/utils.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <miniz.h>

#include <charconv>
#include <deque>
#include <filesystem>
#include <fstream>
#include <random>

namespace nged {

using msghub = MessageHub;

// json {{{
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vec2, x, y);
void from_json(nlohmann::json const& j, Color& c)
{
  if (j.type() == Json::value_t::array) {
    Vector<float> vals = j;
    if (vals.size() != 4) {
      throw std::length_error("vals should have 4 values");
    }
    gmath::LinearColor lc = {vals[0], vals[1], vals[2], vals[3]};
    c                     = toSRGB(lc);
  } else if (j.type() == Json::value_t::string) {
    String code = j;
    if (code.size() != 9 || code[0] != '#') {
      throw std::length_error("color string should be formated like #RRGGBBAA");
    }
    // auto result =
    uint32_t r, g, b, a;
    std::from_chars(code.data() + 1, code.data() + 3, r, 16);
    std::from_chars(code.data() + 3, code.data() + 5, g, 16);
    std::from_chars(code.data() + 5, code.data() + 7, b, 16);
    std::from_chars(code.data() + 7, code.data() + 9, a, 16);
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
  } else {
    throw std::invalid_argument("bad color format");
  }
}
void to_json(nlohmann::json& j, Color const& c)
{
  j = fmt::format("#{:02x}{:02x}{:02x}{:02x}", c.r, c.g, c.b, c.a);
}
void from_json(nlohmann::json const& j, AABB& aabb)
{
  from_json(j["min"], aabb.min);
  from_json(j["max"], aabb.max);
}

void to_json(nlohmann::json& j, AABB const& aabb)
{
  to_json(j["min"], aabb.min);
  to_json(j["max"], aabb.max);
}
// }}} json

// GraphItem {{{
UID generateUID()
{
  static auto mt = std::mt19937(std::random_device()());
  static uuids::uuid_random_generator generator{mt};
  return generator();
}

GraphItem::GraphItem(Graph* parent) : parent_(parent), uid_(generateUID()), sourceUID_{} {}

bool GraphItem::serialize(Json& json) const
{
  to_json(json["aabb"], aabb_);
  std::array<int64_t, 2> intpos = {int64_t(std::round(pos_.x)), int64_t(std::round(pos_.y))};
  json["pos"]                   = intpos;
  json["uid"]                   = uidToString(uid_);
  return true;
}

bool GraphItem::deserialize(Json const& json)
{
  if (auto aabbitr = json.find("aabb"); aabbitr != json.end())
    from_json(*aabbitr, aabb_);
  else
    return false;

  if (auto positr = json.find("pos"); positr != json.end()) {
    if (positr->is_object()) {
      from_json(*positr, pos_);
    } else {
      std::array<int64_t, 2> intpos = *positr;
      pos_                          = {float(intpos[0]), float(intpos[1])};
    }
  } else
    return false;

  if (auto uidstr = json.value("uid", ""); !uidstr.empty())
    sourceUID_ = uidFromString(uidstr);
  if (parent_ && parent_->docRoot() && parent_->docRoot()->deserializeInplace()) {
    parent_->docRoot()->moveUID(uid_, sourceUID_);
    uid_ = sourceUID_;
  }

  return true;
}

void GraphItem::setUID(UID const& uid)
{
  sourceUID_ = uid_;
  uid_       = uid;
  if (parent_ && parent_->docRoot())
    parent_->docRoot()->moveUID(sourceUID_, uid_);
}
// }}}

// Node {{{
Node::Node(Graph* parent, String type, String name)
    : GraphItem(parent), type_(std::move(type)), name_(std::move(name))
{
  aabb_  = {{-25, -10}, {25, 10}};
  color_ = gmath::fromUint32sRGBA(UIStyle::instance().nodeDefaultColor);
  String newname;
  if (rename(name_, newname))
    name_ = std::move(newname);
}

Vec2 Node::inputPinPos(sint i) const
{
  auto sz    = aabb_.size();
  auto count = numMaxInputs();
  if (count < 0) {
    if (auto graph = parent()) {
      auto const& links = parent()->allLinks();
      count = sint(std::count_if(links.begin(), links.end(), [id = id()](auto const& pair) {
        return pair.first.destItem == id;
      }));
      if (count == 0) { // error
        count = 1;
        i     = 0;
      }
      if (i < 0) { // add to the last
        i = count;
      }
    } else { // not added to graph yet
      count = 1;
      i     = 0;
    }
  }
  float idx = float(i);
  if (i >= count)
    idx = i - 0.5f;
  return Vec2{(sz.x * 0.9f) * (idx + 1) / (count + 1) - sz.x * 0.45f, -sz.y / 2.f - 4} + pos_;
}

Vec2 Node::outputPinPos(sint i) const
{
  auto sz = aabb_.size();
  return Vec2{(sz.x * 0.9f) * float(i + 1) / (numOutputs() + 1) - sz.x * 0.45f, sz.y / 2.f + 4} +
         pos_;
}

Color Node::inputPinColor(sint i) const
{
  auto parent = this->parent();
  if (InputConnection ic; parent && parent->getLinkSource(id(), i, ic))
    if (auto item = parent->get(ic.sourceItem); item && item->asDyeable())
      return item->asDyeable()->color();
  return color_;
}

Color Node::outputPinColor(sint i) const
{
  return color_;
}

bool Node::mergedInputBound(AABB& bound) const
{
  auto const n = numMaxInputs();
  if (n > 8 || n < 0) {
    auto sz     = aabb_.size();
    auto center = Vec2{0, -sz.y / 2.0f - 4} + pos_;
    bound.min   = center + Vec2{-sz.x / 2 + 6, -3};
    bound.max   = center + Vec2{sz.x / 2 - 6, 3};
    return true;
  } else {
    return false;
  }
  // TODO: support fixed inputs
}

sint Node::getLastConnectedInputPort() const
{
  auto g = parent();
  assert(g);
  sint port = -1;
  for (auto const& link : g->allLinks()) {
    if (link.first.destItem == id()) {
      port = std::max(port, link.first.destPort);
    }
  }
  return port;
}

bool Node::getInput(sint inPort, NodePtr& nodeptr, sint& outPort) const
{
  auto g = parent();
  assert(g);
  auto itemid = id();
  auto port   = inPort;

  std::set<ItemID> visited;
  for (InputConnection link; g->getLinkSource(itemid, port, link);) {
    if (auto itemptr = g->get(link.sourceItem)) {
      if (visited.find(itemptr->id()) != visited.end()) {
        msghub::errorf("found loop on node {}[{}]", name(), inPort);
        return false;
      }
      visited.insert(itemptr->id());
      if (auto* router = itemptr->asRouter()) {
        itemid = itemptr->id();
        port   = 0;
        continue;
      } else if (itemptr->asNode()) {
        nodeptr = std::static_pointer_cast<Node>(itemptr);
        outPort = link.sourcePort;
        return true;
      } else {
        msghub::errorf("unknown thing was connected to node {}[{}]", name(), inPort);
        return false;
      }
    } else {
      msghub::trace("link to dead end");
      return false;
    }
  }
  return false;
}

void Node::resize(float width, float height, float varPinWidth, float varMarginWidth)
{
  aabb_.min = {width / -2.f, height / -2.f};
  aabb_.max = {width / 2.f, height / 2.f};

  if (numMaxInputs() < 0 && varPinWidth > 0.f && varMarginWidth > 0.f) {
    Vector<Link*> linksIntoThis;
    if (id() != ID_None && parent()) {
      if (Vector<ItemID> links; parent()->linksOnNode(id(), links)) {
        for (auto linkid : links) {
          if (auto* link = parent()->get(linkid)->asLink();
              link && link->output().destItem == id())
            linksIntoThis.push_back(link);
        }
      }
      width = std::max(width, linksIntoThis.size() * varPinWidth + varMarginWidth);
    }
    aabb_.min.x = -width / 2.f;
    aabb_.max.x = width / 2.f;

    for (auto* link : linksIntoThis)
      link->calculatePath();
  }
}

bool Node::serialize(Json& json) const
{
  json["type"] = type_;
  json["name"] = name_;
  to_json(json["color"], color_);
  return GraphItem::serialize(json);
}

bool Node::deserialize(Json const& json)
{
  if (!GraphItem::deserialize(json))
    return false;
  if (auto typeitr = json.find("type"); typeitr != json.end())
    type_ = *typeitr;
  else {
    msghub::error("node has no type");
    return false;
  }
  if (auto nameitr = json.find("name"); nameitr != json.end())
    name_ = *nameitr;
  else {
    msghub::error("node has no name");
    return false;
  }
  if (auto coloritr = json.find("color"); coloritr != json.end())
    from_json(*coloritr, color_);
  return true;
}
// }}} Node

// Type System {{{
TypeSystem& TypeSystem::instance()
{
  static TypeSystem instance;
  return instance;
}

TypeSystem::TypeIndex TypeSystem::registerType(StringView name, StringView baseType, Color hintColor)
{
  auto existingItr = typeIndex_.find(name);
  if (existingItr != typeIndex_.end())
    return existingItr->second;
  sint baseindex = baseType==""? -1: registerType(baseType);
  auto index = nextTypeIndex_++;
  auto strname = String(name);
  typeIndex_[strname] = index;
  types_.emplace_back(strname);
  assert(index+1 == types_.size());
  if (baseindex != -1) {
    typeBaseType_[strname] = baseindex;
    typeConvertable_[std::make_pair(index, baseindex)] = true;
  }
  static auto constexpr noColor = Color{0,0,0,0};
  if (hintColor != noColor)
    setColorHint(index, hintColor);
  return index;
}

void TypeSystem::setConvertable(StringView from, StringView to, bool convertable)
{
  auto fromindex = registerType(from);
  auto toindex = registerType(to);
  typeConvertable_[std::make_pair(fromindex, toindex)] = convertable;
}

bool TypeSystem::isConvertable(StringView from, StringView to) const
{
  if (from == to)
    return true;
  if (to == "any" || to == "*")
    return true;
  auto fromindex = typeIndex(from);
  auto toindex = typeIndex(to);
  auto itr = typeConvertable_.find(std::make_pair(fromindex, toindex));
  // TODO: if a -> b and b -> c, find a way to convert a -> c
  if (itr != typeConvertable_.end())
    return itr->second;
  else
    return false;
}

bool TypeSystem::isType(StringView type) const
{
  return typeIndex_.find(type) != typeIndex_.end();
}

TypeSystem::TypeIndex TypeSystem::typeIndex(StringView type) const
{
  auto itr = typeIndex_.find(type);
  if (itr != typeIndex_.end())
    return itr->second;
  else
    return InvalidTypeIndex;
}

sint TypeSystem::typeCount() const
{
  return types_.size();
}

StringView TypeSystem::typeName(TypeIndex index) const
{
  if (index < 0 || index >= types_.size())
    return "";
  else
    return types_[index];
}

StringView TypeSystem::typeBaseType(TypeIndex index) const
{
  auto itr = typeBaseType_.find(typeName(index));
  if (itr != typeBaseType_.end())
    return typeName(itr->second);
  else
    return "";
}

Optional<Color> TypeSystem::colorHint(TypeIndex index) const
{
  if (auto itr = typeColorHints_.find(index); itr != typeColorHints_.end() && index != InvalidTypeIndex)
    return Optional<Color>(itr->second);
  else
    return Optional<Color>{};
}

void TypeSystem::setColorHint(TypeIndex index, Color color)
{
  if (index == InvalidTypeIndex)
    return;
  typeColorHints_[index] = color;
}
// }}} Type System

// Typed Node {{{
StringView TypedNode::inputType(sint i) const
{
  if (i < 0 || i >= numMaxInputs())
    return "";
  else
    return inputTypes_[i];
}

StringView TypedNode::outputType(sint i) const
{
  if (i < 0 || i >= numOutputs())
    return "";
  else
    return outputTypes_[i];
}

Color TypedNode::inputPinColor(sint i) const
{
  if (i < 0 || i >= numMaxInputs())
    return Node::color();
  else
    return TypeSystem::instance().colorHint(inputType(i)).value_or(
      Node::inputPinColor(i));
}

Color TypedNode::outputPinColor(sint i) const
{
  if (i < 0 || i >= numOutputs())
    return Node::color();
  else
    return TypeSystem::instance().colorHint(outputType(i)).value_or(
      Node::outputPinColor(i));
}

bool TypedNode::acceptInput(sint port, Node const* sourceNode, sint sourcePort) const
{
  auto const* typedSource = sourceNode->asTypedNode();
  assert(typedSource);
  auto const& typeSystem = TypeSystem::instance();
  auto const  srcType    = typedSource->outputType(sourcePort);
  auto const  dstType    = inputType(port);
  if (typeSystem.isConvertable(srcType, dstType))
    return true;
  else 
    return false;
}

sint TypedNode::getPinForIncomingLink(ItemID sourceItem, sint sourcePin) const
{
  if (numMaxInputs() <= 0)
    return -1;
  Node* sourceNode = nullptr;
  if (auto* node = parent()->get(sourceItem)->asNode()) {
    sourceNode = node;
  } else if (auto* router = parent()->get(sourceItem)->asRouter()) {
    if (!router->getNodeSource(sourceNode, sourcePin))
      return -1;
  }
  if (!sourceNode)
    return -1;
  auto const* typedSource = sourceNode->asTypedNode();
  assert(typedSource);
  auto const& typeSystem = TypeSystem::instance();
  auto const  srcType    = typedSource->outputType(sourcePin);
  for (sint i = 0, n = numMaxInputs(); i < n; ++i) {
    auto const dstType = inputType(i);
    if (typeSystem.isConvertable(srcType, dstType))
      return i;
  }
  return -1;
}
// }}} Typed Node

// Link {{{
void Link::calculatePath()
{
  auto const g         = parent();
  auto const srcitem   = g->get(input_.sourceItem);
  auto const dstitem   = g->get(output_.destItem);
  auto const srcnode   = srcitem->asNode();
  auto const dstnode   = dstitem->asNode();
  auto const srcbounds = srcitem->aabb();
  auto const dstbounds = dstitem->aabb();
  auto const srcpos    = srcnode ? srcnode->outputPinPos(input_.sourcePort) : srcitem->pos();
  auto const dstpos    = dstnode ? dstnode->inputPinPos(output_.destPort) : dstitem->pos();
  auto const srcdir    = srcnode ? srcnode->outputPinDir(input_.sourcePort) : Vec2(0, 1);
  auto const dstdir    = dstnode ? dstnode->inputPinDir(output_.destPort) : Vec2(0, -1);

  path_ = parent()->calculatePath(srcpos, dstpos, srcdir, dstdir, srcbounds, dstbounds);
  aabb_ = AABB(path_.front());
  for (auto const& pt : path_)
    aabb_.merge(pt);
  aabb_.expand(2.f);
}

bool Link::hitTest(Vec2 pt) const
{
  if (aabb_.contains(pt)) {
    for (size_t i = 1, n = path_.size(); i < n; ++i) {
      if (gmath::pointSegmentDistance(pt, path_[i - 1], path_[i]) < 2.5)
        return true;
    }
  }
  return false;
}

bool Link::hitTest(AABB bb) const
{
  if (aabb().intersects(bb)) {
    for (size_t i = 1, n = path_.size(); i < n; ++i) {
      if (bb.intersects(path_[i - 1], path_[i]))
        return true;
    }
  }
  return false;
}

bool Link::serialize(Json& json) const
{
  json["from"]["id"]   = input_.sourceItem.value();
  json["from"]["port"] = input_.sourcePort;
  json["to"]["id"]     = output_.destItem.value();
  json["to"]["port"]   = output_.destPort;
  return true;
}

bool Link::deserialize(Json const& json)
{
  if (!GraphItem::deserialize(json))
    return false;
  InputConnection  ic;
  OutputConnection oc;
  if (auto fromitr = json.find("from"); fromitr != json.end()) {
    if (auto nodeitr = fromitr->find("id"); nodeitr != fromitr->end())
      ic.sourceItem = {nodeitr->operator uint64_t()};
    else
      return false;
    if (auto portitr = fromitr->find("port"); portitr != fromitr->end())
      ic.sourcePort = *portitr;
    else
      return false;
  } else {
    return false;
  }
  if (auto toitr = json.find("to"); toitr != json.end()) {
    if (auto nodeitr = toitr->find("id"); nodeitr != toitr->end())
      ic.sourceItem = {nodeitr->operator uint64_t()};
    else
      return false;
    if (auto portitr = toitr->find("port"); portitr != toitr->end())
      ic.sourcePort = *portitr;
    else
      return false;
  } else {
    return false;
  }
  input_  = ic;
  output_ = oc;
  return true;
}
// Link }}}

// Router {{{
Router::Router(Graph* parent) : GraphItem(parent)
{
  float const r = UIStyle::instance().routerRadius;
  aabb_         = AABB({-r, -r}, {r, r});
  color_        = gmath::fromUint32sRGBA(UIStyle::instance().nodeDefaultColor);
}

bool Router::hitTest(Vec2 point) const
{
  float const r = UIStyle::instance().routerRadius;
  return distance2(pos_, point) <= r * r;
}

bool Router::serialize(Json& json) const
{
  if (!GraphItem::serialize(json))
    return false;
  to_json(json["color"], color_);
  return true;
}

bool Router::deserialize(Json const& json)
{
  if (!GraphItem::deserialize(json))
    return false;
  if (auto coloritr = json.find("color"); coloritr != json.end())
    from_json(*coloritr, color_);
  return true;
}

bool Router::getNodeSource(Node*& node, sint& pin) const
{
  if (auto g = parent()) {
    InputConnection ic;
    if (!g->getLinkSource(id(), 0, ic))
      return false;
    while (auto* router = g->get(ic.sourceItem)->asRouter()) {
      if (!g->getLinkSource(ic.sourceItem, 0, ic))
        return false;
    }
    if (auto* asnode = g->get(ic.sourceItem)->asNode()) {
      node = asnode;
      pin  = ic.sourcePort;
      return true;
    }
  }
  return false;
}
// }}} Router

// GroupBox {{{
GroupBox::GroupBox(Graph* parent) : ResizableBox(parent)
{
  aabb_            = {{-100, -100}, {100, 100}};
  backgroundColor_ = gmath::fromUint32sRGBA(UIStyle::instance().groupBoxBackground);
}

bool GroupBox::serialize(Json& json) const
{
  if (!ResizableBox::serialize(json))
    return false;
  to_json(json["bgcolor"], backgroundColor_);
  Vector<size_t> values;
  values.reserve(containingItems_.size());
  for (auto id: containingItems())
    values.push_back(id.value());
  json["contains"] = values;
  return true;
}

bool GroupBox::deserialize(Json const& json)
{
  if (!ResizableBox::deserialize(json))
    return false;
  from_json(json.at("bgcolor"), backgroundColor_);
  if (auto contains = json.find("contains"); contains != json.end()) {
    Vector<size_t> values = *contains;
    containingItems_.clear();
    for (auto v: values)
      containingItems_.insert(ItemID(v));
  }
  return true;
}

void GroupBox::remapItems(HashMap<size_t, ItemID> const& idmap)
{
  HashSet<ItemID> remapedItems;
  for (auto&& id: containingItems_) {
    if (auto itr = idmap.find(id.value()); itr != idmap.end()) {
      remapedItems.insert(itr->second);
    } else {
      msghub::warnf("{} is not in id map", id.value());
    }
  }
  containingItems_ = std::move(remapedItems);
}

void GroupBox::insertItem(ItemID id)
{
  containingItems_.insert(id);
}

void GroupBox::eraseItem(ItemID id)
{
  containingItems_.erase(id);
}

void GroupBox::rescanContainingItems()
{
  containingItems_.clear();
  auto bounds = aabb();
  if (auto* graph = parent()) {
    for (auto id : graph->items()) {
      if (auto item = graph->get(id)) {
        if (bounds.contains(item->aabb()))
          containingItems_.insert(id);
      }
    }
    containingItems_.erase(this->id());
  }
}

bool GroupBox::hitTest(AABB box) const
{
  auto bb = aabb();
  bb.max.y = bb.min.y + UIStyle::instance().groupboxHeaderHeight;
  return bb.intersects(box);
}

bool GroupBox::hitTest(Vec2 point) const
{
  auto bb = aabb();
  bb.max.y = bb.min.y + UIStyle::instance().groupboxHeaderHeight;
  return bb.contains(point);
}

void GroupBox::setBounds(AABB absoluteBounds)
{
  ResizableBox::setBounds(absoluteBounds);
  rescanContainingItems();
}

bool GroupBox::moveTo(Vec2 to) { return ResizableBox::moveTo(to); }
// }}} GroupBox

// CommentBox {{{
CommentBox::CommentBox(Graph* parent) : ResizableBox(parent)
{
  auto const s     = UIStyle::instance().commentBoxMargin;
  aabb_            = AABB(-s, s);
  color_           = gmath::fromUint32sRGBA(UIStyle::instance().commentColor);
  backgroundColor_ = gmath::fromUint32sRGBA(UIStyle::instance().commentBackground);
  text_            = "// some comment";
}

AABB CommentBox::localBound() const
{
  auto const s        = UIStyle::instance().commentBoxMargin;
  auto const halfSize = textSize_ / 2 + s;
  auto       bb       = ResizableBox::localBound();
  if (bb.width() < halfSize.x * 2) {
    bb.min.x = -halfSize.x;
    bb.max.x = halfSize.x;
  }
  if (bb.height() < halfSize.y * 2) {
    bb.min.y = -halfSize.y;
    bb.max.y = halfSize.y;
  }
  return bb;
}

void CommentBox::setText(String text)
{
  // TODO: re-calc aabb
  text_ = std::move(text);
}

bool CommentBox::serialize(Json& json) const
{
  if (!GraphItem::serialize(json))
    return false;
  to_json(json["color"], color_);
  to_json(json["bgcolor"], backgroundColor_);
  json["text"] = text_;
  return true;
}

bool CommentBox::deserialize(Json const& json)
{
  if (!GraphItem::deserialize(json))
    return false;
  if (auto coloritr = json.find("color"); coloritr != json.end())
    from_json(*coloritr, color_);
  if (auto coloritr = json.find("bgcolor"); coloritr != json.end())
    from_json(*coloritr, backgroundColor_);
  setText(json["text"]);
  return true;
}
// }}} CommentBox

// Arrow {{{
Arrow::Arrow(Graph* parent)
    : GraphItem(parent)
    , color_(gmath::fromUint32sRGBA(UIStyle::instance().arrowDefaultColor))
    , start_{0, 0}
    , end_{100, 0}
{
}

bool Arrow::hitTest(Vec2 pt) const
{
  return gmath::pointSegmentDistance(pt, start_ + pos_, end_ + pos_) < thickness_ * 1.2f + 1.f;
}

bool Arrow::hitTest(AABB bb) const { return bb.intersects(start(), end()); }

gmath::AABB Arrow::localBound() const { return AABB(start_, end_); }

bool Arrow::serialize(Json& json) const
{
  if (!GraphItem::serialize(json))
    return false;
  to_json(json["color"], color_);
  to_json(json["start"], start_);
  to_json(json["end"], end_);
  json["thickness"] = thickness_;
  json["size"]      = tipSize_;
  return true;
}

bool Arrow::deserialize(Json const& json)
try {
  if (!GraphItem::deserialize(json))
    return false;
  from_json(json.at("color"), color_);
  from_json(json.at("start"), start_);
  from_json(json.at("end"), end_);
  thickness_ = json.at("thickness");
  tipSize_   = json.at("size");
  return true;
} catch (Json::exception const& err) {
  return false;
}
// }}} Arrow

// Graph {{{
Graph::~Graph()
{
  if (auto doc = docRoot())
    for (auto id : items_)
      doc->removeItem(id);
}

bool Graph::readonly() const
{
  if (readonly_)
    return true;
  if (parent_)
    return parent_->readonly();
  if (docRoot_)
    return docRoot_->readonly();
  return false;
}

ItemID Graph::add(GraphItemPtr item)
{
  if (readonly()) {
    msghub::info("graph is read-only, cannot add any item");
    return ID_None;
  }
  if (item->id() != ID_None) {
    if (auto ptr = tryGet(item->id()); ptr && ptr == item) {
      msghub::warn("item {} is already there, do not add again");
      return item->id();
    } else {
      msghub::error("item is already added elsewhere, cannot be added again");
      return ID_None;
    }
  }
  auto doc = docRoot();
  assert(item->parent() == this);
  auto newid = doc->addItem(item);
  item->id_  = newid;
  items_.insert(newid);
  doc->notifyGraphModified(this);
  item->settled();
  return newid;
}

NodeFactory const* Graph::nodeFactory() const
{
  if (docRoot_)
    return docRoot_->nodeFactory();
  else
    return nullptr;
}

NodePtr Graph::createNode(StringView type)
{
  auto factory = nodeFactory();
  assert(factory);
  auto nodeptr = factory->createNode(this, type);
  if (nodeptr && add(nodeptr) != ID_None)
    return nodeptr;
  return nullptr;
}

GraphItemPtr Graph::get(ItemID id) const
{
  auto doc = docRoot();
  return doc->getItem(id);
}

GraphItemPtr Graph::tryGet(ItemID id) const
{
  if (id == ID_None)
    return nullptr;
  if (items_.find(id) == items_.end())
    return nullptr;
  return docRoot()->getItem(id);
}

void Graph::doRemoveNoCheck(ItemID id)
{
  items_.erase(id);
  docRoot()->removeItem(id);
}

void Graph::regulateVariableInput(Node* node)
{
  std::set<std::pair<sint, ItemID>> connectedPorts;
  for (auto const& linkid : linkIDs_) {
    if (linkid.first.destItem == node->id())
      connectedPorts.insert({linkid.first.destPort, linkid.second});
  }
  sint next = 0;
  for (auto itr = connectedPorts.begin(); itr != connectedPorts.end(); ++itr, ++next) {
    if (itr->first != next) {
      auto oldoutput = OutputConnection{node->id(), itr->first};
      auto oldinput  = links_.at(oldoutput);
      if (auto linkptr = get(itr->second)) {
        doRemoveNoCheck(itr->second);
        links_.erase(oldoutput);
        linkIDs_.erase(oldoutput);
      }
      auto newoutput      = OutputConnection{node->id(), next};
      auto newlink        = std::make_shared<Link>(this, oldinput, newoutput);
      auto newid          = add(newlink);
      links_[newoutput]   = oldinput;
      linkIDs_[newoutput] = newid;
    }
  }
}

void Graph::remove(HashSet<ItemID> const& items)
{
  if (readonly()) {
    msghub::info("graph is read-only, cannot remove any item");
    return;
  }
  auto doc = docRoot();

  HashSet<LinkPtr> affectedLinks;

  for (auto id : items) {
    if (auto item = get(id); item && item->asLink())
      affectedLinks.insert(std::static_pointer_cast<Link>(item));
    else {
      doRemoveNoCheck(id);
    }
  }

  for (auto const& linkpair : links_) {
    if (
      utils::contains(items, linkpair.first.destItem) ||
      utils::contains(items, linkpair.second.sourceItem))
      if (auto iditr = linkIDs_.find(linkpair.first); iditr != linkIDs_.end())
        if (auto linkptr = doc->getItem(iditr->second); linkptr && linkptr->asLink())
          affectedLinks.insert(std::static_pointer_cast<Link>(linkptr));
  }
  for (auto linkptr : affectedLinks) {
    links_.erase(linkptr->output());
    linkIDs_.erase(linkptr->output());
    doRemoveNoCheck(linkptr->id());
  }

  // update connections to nodes with variable input count, make sure the inputs are 0 based and
  // dense packed
  std::set<Node*> varInputNodes;
  for (auto link : affectedLinks) {
    if (auto item = get(link->output().destItem)) {
      if (auto* node = item->asNode()) {
        if (node->numMaxInputs() < 0) {
          varInputNodes.insert(node);
        }
      }
    }
  }

  for (auto* node : varInputNodes) {
    regulateVariableInput(node);
  }

  // update link pathes
  for (auto* node : varInputNodes) {
    for (auto const& linkitr : linkIDs_) {
      if (linkitr.first.destItem == node->id())
        if (auto* link = get(linkitr.second)->asLink())
          link->calculatePath();
    }
  }

  doc->notifyGraphModified(this);
}

void Graph::updateLinkPaths(HashSet<ItemID> const& items)
{
  std::set<LinkPtr> affectedLinks;
  for (auto id : items) {
    Vector<ItemID> linkIDs;
    if (linksOnNode(id, linkIDs)) {
      for (auto linkid : linkIDs) {
        affectedLinks.insert(std::static_pointer_cast<Link>(get(linkid)));
      }
    }
  }
  for (auto link : affectedLinks) {
    link->calculatePath();
  }
}

bool Graph::move(HashSet<ItemID> const& items, Vec2 const& delta)
{
  if (readonly()) {
    msghub::info("graph is read-only, cannot move any item");
    return false;
  }
  bool anythingMoved = false;
  auto doc           = docRoot();
  for (auto id : items) {
    assert(items_.find(id) != items_.end());
    if (auto itemptr = doc->getItem(id))
      if (itemptr->moveTo(itemptr->pos() + delta))
        anythingMoved = true;
  }
  if (!anythingMoved)
    return false;

  updateLinkPaths(items);
  return true;
}

LinkPtr Graph::getLink(ItemID destItem, sint destPort)
{
  if (auto itr = linkIDs_.find(OutputConnection{destItem, destPort}); itr != linkIDs_.end())
    return std::static_pointer_cast<Link>(get(itr->second));
  return nullptr;
}

bool Graph::checkLinkIsAllowed(ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort, NodePin* errorPin)
{
  auto srcitem    = get(sourceItem);
  auto dstitem    = get(destItem);
  auto srcnodeptr = srcitem->asNode();
  auto dstnodeptr = dstitem->asNode();
  auto srcrouter  = srcitem->asRouter();
  auto dstrouter  = dstitem->asRouter();
  if (srcrouter) {
    for (InputConnection ic = {sourceItem, 0}; getLinkSource(ic.sourceItem, 0, ic);) {
      auto item = get(ic.sourceItem);
      if (item->asRouter())
        continue;
      else if (item->asNode()) {
        srcnodeptr = item->asNode();
        sourcePort = ic.sourcePort;
        break;
      }
    }
  }
  if (srcnodeptr) {
    if (dstnodeptr && !dstnodeptr->acceptInput(destPort, srcnodeptr, sourcePort)) {
      if (errorPin)
        *errorPin = NodePin{destItem, destPort, NodePin::Type::In};
      return false;
    } else if (dstrouter) {
      Vector<ItemID> tovisit;
      HashSet<ItemID> visited;
      Vector<OutputConnection> ocs;
      for (tovisit.push_back(dstrouter->id());
          !tovisit.empty();) {
        auto routerid = tovisit.back();
        tovisit.pop_back();
        if (getLinkDestiny(routerid, 0, ocs)) {
          for (auto&& oc: ocs) {
            auto item = get(oc.destItem);
            if (item->asRouter())
              tovisit.push_back(item->id());
            else if (auto* node = item->asNode()) {
              if (!node->acceptInput(oc.destPort, srcnodeptr, sourcePort)) {
                if (errorPin)
                  *errorPin = NodePin{oc.destItem, oc.destPort, NodePin::Type::In};
                return false;
              }
            }
          }
        }
      }
    }
  }
  return true;
}

LinkPtr Graph::setLink(ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort)
{
  auto doc        = docRoot();
  auto srcitem    = get(sourceItem);
  auto dstitem    = get(destItem);
  auto srcnodeptr = srcitem->asNode();
  auto dstnodeptr = dstitem->asNode();
  auto srcrouter  = srcitem->asRouter();
  auto dstrouter  = dstitem->asRouter();
  msghub::tracef(
    "trying to set link from {:x}({})[{}] to {:x}({})[{}]",
    sourceItem.value(),
    srcnodeptr ? srcnodeptr->name() : ".",
    sourcePort,
    destItem.value(),
    dstnodeptr ? dstnodeptr->name() : ".",
    destPort);
  assert((srcnodeptr || srcrouter) && (dstnodeptr || dstrouter));
  Vector<std::weak_ptr<Link>> affectedLinks;
  if (dstnodeptr && dstnodeptr->numMaxInputs() < 0) {
    sint lastPort = -1;
    for (auto const& link : links_) {
      if (link.first.destItem == destItem) {
        lastPort     = std::max(lastPort, link.first.destPort);
        auto linkptr = get(linkIDs_.at(link.first));
        assert(linkptr && linkptr->asLink());
        affectedLinks.push_back(std::static_pointer_cast<Link>(linkptr));
      }
    }
    if (destPort < 0)
      destPort = lastPort + 1;
  } else if (destPort < 0) {
    msghub::error("trying to set mutable input on node with fixed input count");
    return nullptr;
  }
  // trace through the routers to check real source and dest(s)
  if (!checkLinkIsAllowed(sourceItem, sourcePort, destItem, destPort))
    return nullptr;
  InputConnection  ic = {sourceItem, sourcePort};
  OutputConnection oc = {destItem, destPort};
  if (dstrouter || dstnodeptr) {
    if (auto existing = linkIDs_.find(oc); existing != linkIDs_.end()) {
      doRemoveNoCheck(existing->second);
    }
    links_[oc]   = ic;
    auto linkptr = std::make_shared<Link>(this, ic, oc);
    linkIDs_[oc] = add(linkptr);
    // affectedLink can be invalid after above operations
    for (auto link : affectedLinks) {
      if (auto ptr = link.lock()) {
        ptr->calculatePath();
      }
    }
    if (auto* dstdye = dstitem->asDyeable()) {
      if (auto* srcdye = srcitem->asDyeable())
        dstdye->setColor(srcdye->color());
    }
    if (dstrouter) {
      if (srcnodeptr)
        dstrouter->setLinkColor(srcnodeptr->outputPinColor(sourcePort));
      else if (srcrouter)
        dstrouter->setLinkColor(srcrouter->linkColor());
    }
    doc->notifyGraphModified(this);
    return linkptr;
  }
  return nullptr;
}

void Graph::removeLink(ItemID destNodeID, sint destPort)
{
  if (readonly()) {
    msghub::info("graph is read-only, cannot remove link");
    return;
  }
  auto             doc         = docRoot();
  OutputConnection oc          = {destNodeID, destPort};
  bool             isVarInput  = false;
  auto*            destNodePtr = get(destNodeID)->asNode();
  msghub::tracef(
    "trying to remove link to {:x}({})[{}]",
    destNodeID.value(),
    destNodePtr ? destNodePtr->name() : ".",
    destPort);
  if (destNodePtr) {
    if (destNodePtr->numMaxInputs() < 0)
      isVarInput = true;
  }
  if (auto iditr = linkIDs_.find(oc); iditr != linkIDs_.end()) {
    links_.erase(oc);
    auto id = linkIDs_[oc];
    linkIDs_.erase(oc);
    doRemoveNoCheck(id);

    if (isVarInput) {
      regulateVariableInput(destNodePtr);
      for (auto const& linkid : linkIDs_) {
        if (linkid.first.destItem == destNodeID) {
          if (auto* link = get(linkid.second)->asLink()) {
            link->calculatePath();
          }
        }
      }
    }
    doc->notifyGraphModified(this);
  }
}

bool Graph::getLinkSource(ItemID destItem, sint destPort, InputConnection& inConnection)
{
  OutputConnection oc = {destItem, destPort};
  if (auto itr = links_.find(oc); itr != links_.end()) {
    inConnection = itr->second;
    return true;
  }
  return false;
}

bool Graph::getLinkDestiny(
  ItemID                    sourceItem,
  sint                      sourcePort,
  Vector<OutputConnection>& outConnections)
{
  outConnections.clear();
  InputConnection ic = {sourceItem, sourcePort};
  for (auto const& link : links_) {
    if (link.second == ic) {
      outConnections.push_back(link.first);
    }
  }
  return !outConnections.empty();
}

bool Graph::linksOnNode(ItemID node, Vector<ItemID>& relatedLinks)
{
  auto doc = docRoot();
  relatedLinks.clear();
  for (auto const& linkid : linkIDs_) {
    if (auto iditr = items_.find(linkid.second); iditr != items_.end()) {
      if (auto link = std::static_pointer_cast<Link>(doc->getItem(*iditr))) {
        if (link->input().sourceItem == node || link->output().destItem == node)
          relatedLinks.push_back(link->id());
      }
    }
  }
  return !relatedLinks.empty();
}

Vec2 Graph::pinPos(NodePin pin) const
{
  Vec2 pos     = {0, 0};
  bool located = false;
  assert(items_.find(pin.node) != items_.end());
  auto itemptr = docRoot()->getItem(pin.node);
  if (auto* node = itemptr->asNode()) {
    if (pin.type == NodePin::Type::In) {
      pos     = node->inputPinPos(pin.index);
      located = true;
    } else {
      pos     = node->outputPinPos(pin.index);
      located = true;
    }
  } else if (auto* router = itemptr->asRouter()) {
    pos     = router->pos();
    located = true;
  }
  if (!located)
    msghub::errorf("can\'t locate pin {} on node {:x}", pin.index, pin.node.value());
  return pos;
}

Vec2 Graph::pinDir(NodePin pin) const
{
  Vec2 dir     = {1, 0};
  bool located = false;
  assert(items_.find(pin.node) != items_.end());
  auto itemptr = docRoot()->getItem(pin.node);
  if (auto* node = itemptr->asNode()) {
    if (pin.type == NodePin::Type::In) {
      dir     = node->inputPinDir(pin.index);
      located = true;
    } else {
      dir     = node->outputPinDir(pin.index);
      located = true;
    }
  }
  if (!located)
    msghub::errorf("can\'t locate pin {} on node {:x}", pin.index, pin.node.value());
  return dir;
}

Color Graph::pinColor(NodePin pin) const
{
  Color color;
  bool  located = false;
  auto  itemptr = docRoot()->getItem(pin.node);
  if (auto* node = itemptr->asNode()) {
    if (pin.type == NodePin::Type::In) {
      color   = node->inputPinColor(pin.index);
      located = true;
    } else {
      color   = node->outputPinColor(pin.index);
      located = true;
    }
  }
  if (!located)
    msghub::errorf("can\'t locate pin {} on node {:x}", pin.index, pin.node.value());
  return color;
}

Vector<Vec2> Graph::calculatePath(
  Vec2 start,
  Vec2 end,
  Vec2 startDir,
  Vec2 endDir,
  AABB startBound,
  AABB endBound)
{
  // TODO: caclulate the path better
  // TODO: get rid of the magic numbers
  // TODO: respect dirs
  Vector<Vec2> path;
  const float  LOOP_CORNER_SIZE = 8.f;
  const float  EXTEND           = 16.f;

  float       xcenter = (start.x + end.x) * 0.5f;
  float       ycenter = (start.y + end.y) * 0.5f;
  float const dx      = end.x - start.x;
  float const dy      = end.y - start.y;
  auto        sign    = [](float x) { return x > 0 ? 1 : x < 0 ? -1 : 0; };

  if (dy > 0 && abs(dx) / dy < 0.01f) {
    path.push_back(start);
    path.push_back(end);
  } else if (
    abs(dx) < std::max(startBound.width(), LOOP_CORNER_SIZE * 4) &&
    abs(dx) < std::max(endBound.width(), LOOP_CORNER_SIZE * 4) &&
    (dy - EXTEND * 2 < LOOP_CORNER_SIZE * 2) && dy >= 0) {
    path = utils::bezierPath(start, start + startDir * EXTEND, end + endDir * EXTEND, end, 8);
  } else if (dy < EXTEND * 2 + LOOP_CORNER_SIZE * 2) {
    if (fabs(dx) <= fabs(dy) * 2) {
      xcenter = start.x - sign(dx) * std::max(startBound.width(), endBound.width());
    }
    auto        endextend = end + Vec2{0, -EXTEND};
    float const restdy    = dy - EXTEND * 2;

    path.push_back(start);
    path.push_back(start + Vec2{0, EXTEND});
    if (fabs(dx) > fabs(restdy) * 2 + LOOP_CORNER_SIZE * 8) {
      path.emplace_back(start.x + sign(dx) * LOOP_CORNER_SIZE, path.back().y + LOOP_CORNER_SIZE);
      path.emplace_back(
        xcenter - sign(dx * restdy) * restdy / 2 - sign(dx) * LOOP_CORNER_SIZE, path.back().y);
      path.emplace_back(
        xcenter + sign(dx * restdy) * restdy / 2 + sign(dx) * LOOP_CORNER_SIZE,
        endextend.y - LOOP_CORNER_SIZE);
      path.emplace_back(end.x - sign(dx) * LOOP_CORNER_SIZE, endextend.y - LOOP_CORNER_SIZE);
    } else if (abs(restdy) > EXTEND * 2 && abs(dy) > EXTEND) {
      path.emplace_back(
        start.x + sign(xcenter - start.x) * LOOP_CORNER_SIZE, start.y + EXTEND + LOOP_CORNER_SIZE);
      path.emplace_back(xcenter - sign(xcenter - start.x) * LOOP_CORNER_SIZE, path.back().y);
      path.emplace_back(xcenter, path.back().y - LOOP_CORNER_SIZE);
      path.emplace_back(xcenter, endextend.y);
      path.emplace_back(
        xcenter + sign(end.x - xcenter) * LOOP_CORNER_SIZE, endextend.y - LOOP_CORNER_SIZE);
      path.emplace_back(
        end.x - sign(end.x - xcenter) * LOOP_CORNER_SIZE, end.y - EXTEND - LOOP_CORNER_SIZE);
    } else {
      auto curve = utils::bezierPath(
        path.back(), path.back() + startDir * EXTEND, endextend + endDir * EXTEND, endextend, 14);
      for (size_t i = 1, n = curve.size() - 1; i < n; ++i)
        path.push_back(curve[i]);
    }
    path.push_back(endextend);
    path.push_back(end);
  } else {
    path.push_back(start);
    if (fabs(dx) >= 0.33f) {
      if (dy > fabs(dx) + 42) {
        if (dy < 80) {
          path.emplace_back(start.x, ycenter - fabs(dx) / 2);
          path.emplace_back(end.x, ycenter + fabs(dx) / 2);
        } else {
          path.emplace_back(start.x, end.y - fabs(dx) - 20);
          path.emplace_back(end.x, end.y - 20);
        }
      } else if (dy > 40) {
        path.emplace_back(start.x, start.y + 20);
        if (dy < fabs(dx) + 40) {
          path.emplace_back(start.x + sign(dx) * (dy - 40) / 2, ycenter);
          path.emplace_back(end.x - sign(dx) * (dy - 40) / 2, ycenter);
        }
        path.emplace_back(end.x, end.y - 20);
      }
    }
    path.push_back(end);
  }
  return path;
}

bool Graph::serialize(Json& json) const
{
  // if (!Node::serialize(json))
  //   return false;
  Vector<Link*> links;
  auto&         itemsection = json["items"];
  auto&         linksection = json["links"];
  for (auto id : items_) {
    auto itemptr = docRoot()->getItem(id);
    if (auto* link = itemptr->asLink())
      links.push_back(link);
    else {
      Json itemdata;
      itemdata["id"] = id.value();
      itemdata["f"]  = docRoot()->itemFactory()->factoryName(itemptr);
      if (!itemptr->serialize(itemdata)) {
        msghub::errorf(
          "failed to serialize item {:x} ({})",
          id.value(),
          docRoot()->itemFactory()->factoryName(itemptr));
        return false;
      }
      itemsection.push_back(itemdata);
    }
  }
  for (auto* link : links) {
    Json linkdata;
    link->serialize(linkdata);
    linksection.push_back(linkdata);
  }
  return true;
}

void Graph::clear()
{
  for (auto id : items_)
    docRoot_->removeItem(id);
  items_.clear();
  links_.clear();
  linkIDs_.clear();
}

bool Graph::deserialize(Json const& json)
{
  auto doc = docRoot();

  HashMap<UID, ItemID>    uidmap;    // uid to id
  HashMap<size_t, ItemID> idmap;     // old id to new id
  HashMap<UID, ItemID>    uidoldmap; // uid to old id
  for (auto&& itemdata : json["items"])
    if (itemdata.contains("uid"))
      uidoldmap[uidFromString(String(itemdata["uid"]))] = ItemID(itemdata["id"].get<size_t>());
  HashSet<ItemID> redundantItems;
  for (auto id : items()) {
    auto item = get(id);
    if (uidoldmap.find(item->uid()) == uidoldmap.end())
      redundantItems.insert(id);
    else {
      uidmap[item->uid()]                   = item->id();
      idmap[uidoldmap[item->uid()].value()] = item->id();
    }
  }
  remove(redundantItems);
  for (auto&& itemdata : json["items"]) {
    auto uid = itemdata.contains("uid") ? uidFromString(String(itemdata["uid"])) : UID();
    if (auto itr = uidmap.find(uid); itr != uidmap.end()) {
      if (!get(itr->second)->deserialize(itemdata)) {
        msghub::errorf("failed to import item {}", itemdata.dump(2));
        return false;
      }
    } else {
      String       factory = itemdata["f"];
      GraphItemPtr newitem;
      if (factory.empty() || factory == "node") {
        String type = itemdata["type"];
        newitem     = nodeFactory()->createNode(this, type);
      } else {
        newitem = docRoot()->itemFactory()->make(this, factory);
      }
      if (!newitem || !newitem->deserialize(itemdata)) {
        msghub::errorf("failed to import item {}", itemdata.dump(2));
        return false;
      }
      auto newid             = add(newitem);
      idmap[itemdata["id"]]  = newid;
      uidmap[newitem->uid()] = newid;
    }
  }

  HashSet<OutputConnection> newlinks;
  for (auto&& linkdata : json["links"]) {
    auto const&      from   = linkdata["from"];
    auto const&      to     = linkdata["to"];
    InputConnection  incon  = {idmap.at(from["id"]), sint(from["port"])};
    OutputConnection outcon = {idmap.at(to["id"]), sint(to["port"])};
    newlinks.insert(outcon);
    auto linkitr = links_.find(outcon);
    if (linkitr != links_.end()) {
      if (linkitr->second == incon)
        continue;
      else
        msghub::errorf(
          "link from {}({}) to {}({}) has already been set",
          incon.sourceItem.value(), incon.sourcePort,
          outcon.destItem.value(), outcon.destPort);
    }

    links_[outcon] = incon;
    auto linkptr = std::make_shared<Link>(this, incon, outcon);
    linkIDs_[outcon] = add(linkptr);
  }
  HashSet<OutputConnection> redundantLinks;
  for (auto&& pair : links_)
    if (newlinks.find(pair.first) == newlinks.end())
      redundantLinks.insert(pair.first);
  if (!redundantLinks.empty()) {
    msghub::error("have redundant link");
  }
  // for (auto&& outcon : redundantLinks)
  //  removeLink(outcon.destItem, outcon.destPort);

  for (auto id : items_) {
    if (auto* group = get(id)->asGroupBox())
      group->remapItems(idmap);
  }

  doc->notifyGraphModified(this);
  return true;
}

bool Graph::checkLoopBottomUp(ItemID target, Vector<ItemID>& loop, HashSet<ItemID>* visited)
{
  class LoopChecker
  {
    Graph* g;

  public:
    LoopChecker(Graph* graph) : g(graph) {}

    HashSet<ItemID> visited;
    HashSet<ItemID> stack;
    Vector<ItemID>  loop;

    bool isVisited(ItemID id) const { return visited.find(id) != visited.end(); }
    bool isInStack(ItemID id) const { return stack.find(id) != stack.end(); }

    bool visit(ItemID itemid)
    {
      loop.push_back(itemid);
      if (!isVisited(itemid)) {
        visited.insert(itemid);
        stack.insert(itemid);
        if (auto item = g->docRoot()->getItem(itemid)) {
          auto* gr = item->parent();
          if (auto* node = item->asNode()) {
            if (node->numMaxInputs() > 0) {
              for (sint i = 0; i < node->numMaxInputs(); ++i) {
                if (auto itr = gr->allLinks().find({itemid, i}); itr != gr->allLinks().end()) {
                  if (!isVisited(itr->second.sourceItem)) {
                    if (visit(itr->second.sourceItem))
                      return true;
                  }
                  if (isInStack(itr->second.sourceItem))
                    return true;
                }
              }
            } else if (node->numMaxInputs() < 0) {
              for (auto&& link : gr->allLinks()) {
                if (link.first.destItem == itemid) {
                  if (!isVisited(link.second.sourceItem)) {
                    if (visit(link.second.sourceItem))
                      return true;
                  }
                  if (isInStack(link.second.sourceItem))
                    return true;
                }
              }
            }
            if (Vector<ItemID> deps; node->getExtraDependencies(deps)) {
              for (auto dep : deps) {
                if (!isVisited(dep)) {
                  if (visit(dep))
                    return true;
                }
                if (isInStack(dep))
                  return true;
              }
            }
          } else if (item->asRouter()) {
            if (auto itr = gr->allLinks().find({itemid, 0}); itr != gr->allLinks().end()) {
              if (!isVisited(itr->second.sourceItem)) {
                if (visit(itr->second.sourceItem))
                  return true;
              }
              if (isInStack(itr->second.sourceItem))
                return true;
            }
          }
        }
      }
      stack.erase(itemid);
      loop.pop_back();
      return false;
    }
  };

  LoopChecker checker(this);
  if (checker.visit(target)) {
    loop = checker.loop;
    return true;
  }
  if (visited)
    for (auto&& id : checker.visited)
      visited->insert(id);
  return false;
}

bool Graph::traverse(
  GraphTraverseResult&  result,
  Vector<ItemID> const& startPoints,
  bool                  topdown,
  bool                  allowLoop)
{
  auto& nodes    = result.nodes_;
  auto& inputs   = result.inputs_;
  auto& outputs  = result.outputs_;
  auto& closures = result.closures_;
  auto& idmap    = result.idmap_;
  using Range    = GraphTraverseResult::Range;

  nodes.clear();
  inputs.clear();
  outputs.clear();
  closures.clear();

  HashMap<NodePtr, size_t> nodeIndex; // nodeptr -> index in result.nodes_
  HashSet<ItemID>          visited;
  std::deque<ItemID>       toVisit;

  auto indexofnode = [&nodeIndex](GraphItemPtr item) -> size_t {
    if (!item || !item->asNode())
      return -1;
    if (auto itr = nodeIndex.find(std::static_pointer_cast<Node>(item)); itr != nodeIndex.end())
      return itr->second;
    else
      return -1;
  };

  std::unordered_multimap<ItemID, ItemID> linkUp;
  std::unordered_multimap<ItemID, ItemID> linkDown;
  HashSet<Graph*> referencedGraphs; // extraDependencies can reference to other graphs
  HashSet<Graph*> visitedGraphs;
  auto            traceLinks = [&](Graph* graph) {
    for (auto const& link : graph->links_) {
      linkDown.emplace(link.second.sourceItem, link.first.destItem);
      linkUp.emplace(link.first.destItem, link.second.sourceItem);
    }
    Vector<ItemID> deps;
    for (auto id : graph->items_) {
      auto itemptr = graph->get(id);
      if (auto* nodeptr = itemptr->asNode()) {
        if (nodeptr->getExtraDependencies(deps)) {
          for (auto depid : deps) {
            linkDown.emplace(depid, id);
            linkUp.emplace(id, depid);
            auto depitem = graph->docRoot_->getItem(depid);
            referencedGraphs.insert(depitem->parent());
          }
        }
      }
    }
  };
  for (Graph* nextGraphToVisit = this; nextGraphToVisit;) {
    traceLinks(nextGraphToVisit);
    visitedGraphs.insert(nextGraphToVisit);
    nextGraphToVisit = nullptr;
    for (auto* graph : referencedGraphs) {
      if (!utils::contains(visitedGraphs, graph)) {
        nextGraphToVisit = graph;
        break;
      }
    }
  }

  auto& linkToFollow = topdown ? linkDown : linkUp;
  for (auto id : startPoints)
    toVisit.push_back(id);

  HashSet<ItemID> visitedWithNoLoop;
  while (!toVisit.empty()) {
    auto    id      = toVisit.front();
    auto    itemptr = get(id);
    NodePtr nodeptr = nullptr;
    toVisit.pop_front();

    if (!itemptr) {
      msghub::warnf("item {:x} is not a valid target now", id.value());
      continue;
    }
    if (itemptr->asNode())
      nodeptr = std::static_pointer_cast<Node>(itemptr);

    if (auto itr = visited.find(id); itr != visited.end()) {
      if (!allowLoop) {
        if (visitedWithNoLoop.find(id) != visitedWithNoLoop.end()) {
          // pass;
        } else if (Vector<ItemID> loopPath; checkLoopBottomUp(id, loopPath, &visitedWithNoLoop)) {
          msghub::error("loop detected, which is not allowed:");
          msghub::error("loop path: {");
          String name;
          loopPath.push_back(loopPath.front());
          for (auto id : loopPath) {
            auto item = docRoot_->getItem(id);
            if (auto* node = item->asNode())
              name = node->name();
            else if (item->asRouter())
              name = "router";
            else
              name = "GraphItem";
            msghub::errorf("  {}({:x})", name, id.value());
          }
          msghub::error("} // loop path");
          return false;
        }
      }
      // move to the back
      if (nodeptr) {
        if (auto itr = nodeIndex.find(nodeptr); itr != nodeIndex.end()) {
          nodes.push_back(nodeptr);
          nodes[itr->second] = nullptr;
          itr->second        = nodes.size() - 1;
        } else {
          msghub::error("visited node should have a index");
          assert(false);
        }
      }
    } else {
      if (nodeptr) {
        nodes.push_back(nodeptr);
        nodeIndex[nodeptr] = nodes.size() - 1;
      }
    }
    visited.insert(id);
    for (auto range = linkToFollow.equal_range(id); range.first != range.second; ++range.first) {
      toVisit.push_back(range.first->second);
    }
    // if (nodeptr) {
    //   if (Vector<ItemID> deps; nodeptr->getExtraDependencies(deps)) {
    //     for (auto depid : deps) {
    //       toVisit.push_back(depid);
    //     }
    //   }
    // }
  }
  // remove nullptrs in nodes array
  size_t denseSize = 0;
  for (size_t r = 0, w = 0, n = nodes.size();; ++r, ++w, ++denseSize) {
    while (r < n && nodes[r] == nullptr)
      ++r;
    if (r >= n)
      break;
    if (r == w)
      continue;
    nodes[w]            = nodes[r];
    nodeIndex[nodes[w]] = w;
  }
  nodes.resize(denseSize);

  for (size_t i = 0, n = nodes.size(); i < n; ++i) {
    auto  id          = nodes[i]->id();
    auto  inputbegin  = inputs.size();
    int   ninput      = 0;
    auto  outputbegin = outputs.size();
    int   noutput     = 0;
    auto* graph       = nodes[i]->parent();
    if (Vector<ItemID> links; graph->linksOnNode(id, links)) {
      for (auto linkid : links) {
        if (auto link = graph->get(linkid)->asLink()) {
          if (link->output().destItem == id) { // I am the dest
            NodePtr inNode    = nullptr;
            auto    inputItem = graph->get(link->input().sourceItem);
            while (!inputItem->asNode()) {
              if (InputConnection ic; graph->getLinkSource(inputItem->id(), 0, ic)) {
                inputItem = graph->get(ic.sourceItem);
              } else {
                break;
              }
            }
            sint const port = link->output().destPort;
            if (port >= ninput)
              ninput = port + 1;
            auto writeindex = inputbegin + port;
            if (writeindex >= inputs.size())
              inputs.resize(writeindex + 1, -1);
            inputs[writeindex] = indexofnode(inputItem);
          } else if (link->input().sourceItem == id) {
            // TODO: put outputs into their ports
            for (std::vector<ItemID> idsToResolve = {id}; !idsToResolve.empty();) {
              auto iid = idsToResolve.back();
              idsToResolve.pop_back();
              for (auto range = linkDown.equal_range(iid); range.first != range.second;
                   ++range.first) {
                auto item = graph->get(range.first->second);
                if (item->asNode()) {
                  auto idx = indexofnode(item);
                  if (idx == -1)
                    continue;
                  outputs.push_back(idx);
                  ++noutput;
                } else if (item->asRouter()) {
                  idsToResolve.push_back(item->id());
                }
              }
            }
          }
        }
      }
    }
    closures.push_back(
      {i, Range{inputbegin, inputbegin + ninput}, Range{outputbegin, outputbegin + noutput}});
  }

  idmap.clear();
  for (size_t i = 0, n = nodes.size(); i < n; ++i)
    idmap[nodes[i]->id()] = i;
  return true;
}

bool Graph::travelTopDown(
  GraphTraverseResult&  result,
  Vector<ItemID> const& sourceItems,
  bool                  allowLoop)
{
  return traverse(result, sourceItems, true, allowLoop);
}

bool Graph::travelTopDown(GraphTraverseResult& result, ItemID sourceItem, bool allowLoop)
{
  Vector<ItemID> ids = {sourceItem};
  return travelTopDown(result, ids, allowLoop);
}

bool Graph::travelBottomUp(GraphTraverseResult& result, ItemID destItem, bool allowLoop)
{
  Vector<ItemID> ids = {destItem};
  return travelBottomUp(result, ids, allowLoop);
}

bool Graph::travelBottomUp(
  GraphTraverseResult&  result,
  Vector<ItemID> const& destItems,
  bool                  allowLoop)
{
  return traverse(result, destItems, false, allowLoop);
}
// }}} Graph

// GraphItemPool {{{
GraphItemPool::GraphItemPool()
{
  auto seed = std::random_device()();
  randGenerator_ = std::mt19937(seed);
}

ItemID GraphItemPool::add(GraphItemPtr item)
{
  ItemID iid = ID_None;
  if (!freeList_.empty()) {
    uint32_t index = freeList_.back();
    freeList_.pop_back();
    items_[index] = item;
    iid = {uint32_t(randGenerator_()), index};
  } else {
    size_t id = items_.size();
    items_.push_back(item);
    iid = {uint32_t(randGenerator_()), uint32_t(id)};
  }
  if (uidMap_.find(item->uid()) != uidMap_.end()) {
    throw std::runtime_error("got duplicated uid");
  }
  uidMap_[item->uid()] = iid;
  return iid;
}

void GraphItemPool::moveUID(UID const& oldUID, UID const& newUID)
{
  if (oldUID == newUID)
    return;
  if (auto itr = uidMap_.find(oldUID); itr != uidMap_.end()) {
    if (uidMap_.find(newUID) != uidMap_.end())
      throw std::runtime_error("got duplicated uid");
    auto iid = itr->second;
    uidMap_.erase(itr);
    uidMap_[newUID] = iid;
  }
}
// }}} GraphItemPool

// History {{{
void NodeGraphDocHistory::reset(bool createInitialCommit)
{
  versions_.clear();
  undoStack_.clear();
  indexAtUndoStack_ = -1;
  assert(atEditGroupLevel_ == 0);

  if (createInitialCommit) {
    commit("initalize");
  }
  doc_->untouch();
}

size_t NodeGraphDocHistory::commit(String msg)
{
  Json json;
  if (doc_ && doc_->root()->serialize(json)) {
    size_t          versionNumber = versions_.size();
    String          data          = json.dump();
    auto            size          = data.size();
    Vector<uint8_t> compressedData(mz_compressBound(size));
    mz_ulong        compressedLen = compressedData.size();
    mz_compress(
      compressedData.data(),
      &compressedLen,
      reinterpret_cast<uint8_t const*>(data.data()),
      data.size());
    compressedData.resize(compressedLen);
    versions_.push_back({std::move(compressedData), std::move(msg), size});
    assert(versionNumber + 1 == versions_.size());
    if (undoStack_.empty()) {
      undoStack_.push_back(versionNumber);
      indexAtUndoStack_ = 0;
    } else {
      assert(indexAtUndoStack_ >= 0 && indexAtUndoStack_ < undoStack_.size());
      undoStack_.resize(indexAtUndoStack_ + 2);
      undoStack_[++indexAtUndoStack_] = versionNumber;
    }

    doc_->touch();
    return versionNumber;
  } else {
    return -1;
  }
}

bool NodeGraphDocHistory::checkout(size_t version)
{
  if (version == -1 || version >= versions_.size()) {
    msghub::errorf("trying to checkout a bad version: {}", version);
    return false;
  }
  if (versions_[version].data.empty()) {
    msghub::errorf("version {} has been pruned out", version);
    return false;
  }
  ++atEditGroupLevel_; // suspend auto commit from editGroups
  String   uncompressedData;
  mz_ulong uncompressedSize = versions_[version].uncompressedSize;
  uncompressedData.resize(uncompressedSize);
  auto result = mz_uncompress(
    reinterpret_cast<uint8_t*>(uncompressedData.data()),
    &uncompressedSize,
    versions_[version].data.data(),
    versions_[version].data.size());
  if (result != MZ_OK)
    throw std::runtime_error("failed to decompress history data");
  Json json    = Json::parse(uncompressedData);
  bool succeed = doc_->root()->deserialize(json);
  --atEditGroupLevel_;

  if (version == fileVersion_) {
    doc_->untouch();
  } else {
    doc_->touch();
  }
  return succeed;
}

bool NodeGraphDocHistory::undo()
{
  if (indexAtUndoStack_ > 0) {
    return checkout(undoStack_[--indexAtUndoStack_]);
  } else {
    msghub::info("undo: already at oldest version");
    return false;
  }
}

bool NodeGraphDocHistory::redo()
{
  if (indexAtUndoStack_ >= 0 && indexAtUndoStack_ + 1 < undoStack_.size()) {
    return checkout(undoStack_[++indexAtUndoStack_]);
  } else {
    msghub::info("undo: already at newest version");
    return false;
  }
}

void NodeGraphDocHistory::markSaved()
{
  if (indexAtUndoStack_ >= 0 && indexAtUndoStack_ < undoStack_.size()) {
    fileVersion_ = undoStack_[indexAtUndoStack_];
  } else {
    msghub::error("cannot mark saved, undo stack is corrupted");
  }
}

size_t NodeGraphDocHistory::memoryBytesUsed() const
{
  size_t sum = sizeof(*this);
  for (auto&& ver : versions_) {
    sum += ver.data.size();
    sum += ver.message.size();
    sum += sizeof(ver);
  }
  sum += undoStack_.size() * sizeof(size_t);
  return sum;
}
// }}} History

// NodeGraphDoc {{{
NodeGraphDoc::NodeGraphDoc(NodeFactoryPtr nodeFactory, GraphItemFactory const* itemFactory)
    : history_(this), nodeFactory_(nodeFactory), itemFactory_(itemFactory)
{
  root_ = nullptr;
}

NodeGraphDoc::~NodeGraphDoc()
{
  // set graphs as rootless to pervent it from accessing (maybe already destroyed) pool in
  // destructor
  pool_.foreach ([](auto itemptr) {
    if (auto* node = itemptr->asNode()) {
      if (auto* graph = node->asGraph()) {
        graph->setRootless();
      }
    }
  });
  root_.reset();
}

void NodeGraphDoc::makeRoot() { root_ = GraphPtr(nodeFactory_->createRootGraph(this)); }

StringView NodeGraphDoc::title() const { return title_; }

bool NodeGraphDoc::open(String path)
try {
  std::ifstream infile(path);
  if (!infile.good()) {
    msghub::errorf("failed to open \"{}\"", path);
    return false;
  }

  auto content = filterFileInput(String{std::istreambuf_iterator<char>(infile), {}});
  Json injson  = Json::parse(content);

  auto newgraph = GraphPtr(nodeFactory_->createRootGraph(this));
  if (!newgraph->deserialize(injson["root"])) {
    msghub::errorf("failed to deserialize content from {}", path);
    return false;
  }
  // TODO: update viewers
  root_     = newgraph;
  savePath_ = path;
  title_    = std::filesystem::path(savePath_).stem().u8string();
  history_.reset(false);
  history_.commit("load " + path);
  history_.markSaved();
  dirty_ = false;
  return true;
} catch (Json::exception const& err) {
  msghub::errorf("failed to parse \"{}\": {}", path, err.what());
  return false;
}

bool NodeGraphDoc::save()
{
  if (readonly()) {
    msghub::errorf("document {} is read-only, cannot save", savePath_);
    return false;
  }
  if (saveTo(savePath_)) {
    history_.markSaved();
    dirty_ = false;
    return true;
  } else {
    return false;
  }
}

void NodeGraphDoc::close()
{
  root_.reset();
}

bool NodeGraphDoc::saveAs(String path)
{
  if (saveTo(path)) {
    savePath_ = std::move(path);
    title_    = std::filesystem::path(savePath_).stem().u8string();
    history_.markSaved();
    dirty_    = false;
    readonly_ = false;
    return true;
  } else {
    return false;
  }
}

bool NodeGraphDoc::saveTo(String path)
{
  Json outjson;
  if (!root_->serialize(outjson["root"])) {
    msghub::error("failed to serialize graph");
    return false;
  }

  std::ofstream outfile(path);
  if (!outfile.good()) {
    msghub::errorf("can\'t open {} for writing", path);
    return false;
  }

  auto dumpstr = filterFileOutput(outjson.dump());
  return outfile.write(dumpstr.c_str(), dumpstr.size()).good();
}

void NodeGraphDoc::notifyGraphModified(Graph* graph)
{
  if (graphModifiedNotifier_)
    graphModifiedNotifier_(graph);
}
// }}} NodeGraphDoc

// MessageHub {{{
MessageHub MessageHub::instance_;

void MessageHub::addMessage(
  String                message,
  MessageHub::Category  category,
  MessageHub::Verbosity verbosity)
{
  std::unique_lock lock(mutex_);
  auto             time  = std::chrono::system_clock::now();
  auto&            queue = messageCategories_[static_cast<int>(category)];
  if (category == Category::Log)
    spdlog::log(static_cast<spdlog::level::level_enum>(verbosity), message);

  if (queue.size() > countLimit_)
    queue.pop_front();
  queue.emplace_back(std::move(message), verbosity, time);
}

void MessageHub::clear(Category category)
{
  std::unique_lock lock(mutex_);
  auto&            queue = messageCategories_[static_cast<int>(category)];
  queue.clear();
}

void MessageHub::clearAll()
{
  for (int i = 0; i < static_cast<int>(Category::Count); ++i)
    clear(static_cast<Category>(i));
}

void MessageHub::setCountLimit(size_t count)
{
  std::unique_lock lock(mutex_);
  countLimit_ = count;
  for (int i = 0; i < static_cast<int>(Category::Count); ++i)
    while (messageCategories_[i].size() > count)
      messageCategories_[i].pop_front();
}
// }}} MessageHub

// Default Items  {{{

GraphItemFactoryPtr defaultGraphItemFactory()
{
  auto factory = std::make_shared<GraphItemFactory>();
  // nodes are seprately implemented
  factory->set("link", false, [](Graph* parent) -> GraphItemPtr {
    return GraphItemPtr(new Link{parent, InputConnection{}, OutputConnection{}});
  });
  factory->set("router", true, [](Graph* parent) -> GraphItemPtr {
    return std::make_shared<Router>(parent);
  });
  factory->set("comment", true, [](Graph* parent) -> GraphItemPtr {
    return std::make_shared<CommentBox>(parent);
  });
  factory->set(
    "arrow", true, [](Graph* parent) -> GraphItemPtr { return GraphItemPtr(new Arrow(parent)); });
  factory->set("group", true, [](Graph* parent) -> GraphItemPtr {
    return GraphItemPtr(new GroupBox(parent));
  });

  // TODO: group boxes &etc.
  return factory;
}

// }}} Default Items

// Canvas {{{
float Canvas::floatFontSize(Canvas::FontSize enumsize)
{
  float size = 12.f;
  switch (enumsize) {
  case FontSize::Small: size = UIStyle::instance().smallFontSize; break;
  case FontSize::Large: size = UIStyle::instance().bigFontSize; break;
  case FontSize::Normal:
  default: size = UIStyle::instance().normalFontSize;
  }
  return size;
}
// }}} Canvas

} // namespace nged
