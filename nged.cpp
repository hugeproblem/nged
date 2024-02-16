#include "nged.h"
#include "utils.h"
#include "style.h"
#include "boxer/boxer.h"
#include "nfd.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>

// string format support {{{
template<>
struct fmt::formatter<gmath::Vec2>
{
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
  {
    auto it = ctx.begin(), end = ctx.end();
    while (it != end && *it != '}')
      ++it;
    return it;
  }
  template<typename FormatContext>
  auto format(const gmath::Vec2& v, FormatContext& ctx) const -> decltype(ctx.out())
  {
    return fmt::format_to(ctx.out(), "({:.4f}, {:.4f})", v.x, v.y);
  }
};
// }}}

namespace nged {

using msghub = MessageHub;

// GraphView {{{
GraphView::GraphView(NodeGraphEditor* editor, NodeGraphDocPtr doc)
    : editor_(editor), doc_(doc), graph_()
{
  static size_t nextId = 0;

  id_    = ++nextId;
  title_ = "untitled";
  reset(doc);
}

void GraphView::reset(NodeGraphDocPtr doc)
{
  doc_ = doc;
  reset(doc ? doc->root() : nullptr);
}

void GraphView::reset(WeakGraphPtr graph) { graph_ = graph; }

void GraphView::update(float dt)
{
  if (isFocused_)
    editor()->commandManager().checkShortcut(this);
  editor()->commandManager().update(this);
}

bool GraphView::readonly() const
{
  if (auto graph = graph_.lock())
    return graph->readonly();
  return false;
}
// }}} GraphView

// Interaction States {{{
// AnimationState {{{
void AnimationState::setViewPos(Canvas* canvas, Vec2 pos)
{
  viewPosStart_ = pos;
  viewPosDest_  = pos;
  duration_     = 0;
  t_            = 0;
  easingOrder_  = 0;
  canvas->setViewPos(pos);
}

void AnimationState::setViewScale(Canvas* canvas, float scale)
{
  viewScaleStart_ = scale;
  viewScaleDest_  = scale;
  duration_       = 0;
  t_              = 0;
  easingOrder_    = 0;
  canvas->setViewScale(scale);
}

void AnimationState::animateToPos(Canvas* canvas, Vec2 pos, float duration, int order)
{
  viewPosStart_   = canvas->viewPos();
  viewPosDest_    = pos;
  viewScaleStart_ = viewScaleDest_;
  duration_       = duration;
  t_              = 0;
  easingOrder_    = order;
}

void AnimationState::animateToScale(Canvas* canvas, float scale, float duration, int order)
{
  viewScaleStart_ = canvas->viewScale();
  viewScaleDest_  = scale;
  viewPosStart_   = viewPosDest_;
  duration_       = duration;
  t_              = 0;
  easingOrder_    = order;
}

void AnimationState::animateTo(Canvas* canvas, Vec2 pos, float scale, float duration, int order)
{
  viewPosStart_   = canvas->viewPos();
  viewScaleStart_ = canvas->viewScale();
  viewPosDest_    = pos;
  viewScaleDest_  = scale;
  duration_       = duration;
  t_              = 0;
  easingOrder_    = order;
}

void AnimationState::tick(NetworkView* view, float dt)
{
  if (duration_ <= 0)
    return;
  if (t_ > duration_)
    return;

  using namespace utils::ease;
  bool finished = false;
  t_ += dt;
  if (t_ > duration_) {
    t_       = duration_;
    finished = true;
  }
  constexpr float (*easeFunctions[])(float) = {inOutLinear, inOutLinear, inOutQuad, inOutCubic, inOutExpo};
  float (*ease)(float) = inOutLinear;
  if (easingOrder_ >= 0 && easingOrder_ <= 4)
    ease = easeFunctions[easingOrder_];

  auto pos   = viewPosStart_ + (viewPosDest_ - viewPosStart_) * ease(t_ / duration_);
  auto scale = viewScaleStart_ + (viewScaleDest_ - viewScaleStart_) * ease(t_ / duration_);

  view->canvas()->setViewPos(pos);
  view->canvas()->setViewScale(scale);

  if (finished) {
    t_              = 0;
    duration_       = 0;
    viewPosStart_   = viewPosDest_;
    viewScaleStart_ = viewScaleDest_;
    easingOrder_    = 3;
  }
}
// }}} AnimationState
// }}} Interaction States

// NetworkView {{{
Vector<NetworkView::InteractionStateFactory> NetworkView::stateFactories_;

NetworkView::NetworkView(
  NodeGraphEditor*        editor,
  NodeGraphDocPtr         doc,
  std::unique_ptr<Canvas> canvas)
    : GraphView(editor, doc), canvas_(std::move(canvas))
{
}

void NetworkView::initInteractionStates()
{
  for (auto&& factory : stateFactories_) {
    addState(InteractionStatePtr(factory.creator(factory.arg)));
  }
}

void NetworkView::setSelectedItems(HashSet<ItemID> items)
{
  // selecting single item moves it to top
  if (
    items.size() == 1 &&
    !(selectedItems_.size() == 1 && *selectedItems_.begin() == *items.begin())) {
    zOrder_[*items.begin()] = ++highZ_;
  }
  selectedItems_.swap(items);
  editor()->boardcastViewEvent(this, "selectionChanged");
  if (auto* resp = editor()->responser())
    resp->onSelectionChanged(this);
}

Node* NetworkView::solySelectedNode() const
{
  Node* solyNode = nullptr;
  for (auto id: selectedItems_) {
    if (auto* node = graph()->get(id)->asNode()) {
      if (solyNode) {
        solyNode = nullptr;
        break;
      } else {
        solyNode = node;
      }
    }
  }
  return solyNode;
}

int NetworkView::zCompare(GraphItem* lhs, GraphItem* rhs) const
{
  if (!lhs)
    return -1;
  if (!rhs)
    return 1;

  int zzl = lhs->zOrder(), zzr = rhs->zOrder();
  if (zzl < zzr)
    return -1;
  if (zzl > zzr)
    return 1;

  size_t     zl = 0, zr = 0;
  auto const litr = zOrder_.find(lhs->id()), ritr = zOrder_.find(rhs->id());
  if (litr != zOrder_.end())
    zl = litr->second;
  if (ritr != zOrder_.end())
    zr = ritr->second;

  return zl < zr ? -1 : zl > zr ? 1 : 0;
}

void NetworkView::update(float dt)
{
  hiddenOnceItems_.clear();
  // GraphView::update(dt); don't use default logic
  for (auto state : states_) {
    if (!state->active() && state->shouldEnter(this)) {
      state->onEnter(this);
      state->active_ = true;
    }
    if (state->active())
      state->tick(this, dt);
  }
  bool hasBlockingState = false;
  for (auto state : states_) {
    try {
      if (state->active() && state->update(this)) {
        hasBlockingState = true;
        break;
      }
    } catch (std::exception const& e) {
      msghub::errorf("failed to update state {}: {}", state->name(), e.what());
    }
  }
  if (isFocused_ && !hasBlockingState) {
    editor()->commandManager().checkShortcut(this);
  }
  editor()->commandManager().update(this);
  for (auto state : states_) {
    if (state->active() && state->shouldExit(this)) {
      state->onExit(this);
      state->active_ = false;
    }
  }
}

void NetworkView::updateAndDrawEffects(float dt)
{
  for (sint i=sint(effects_.size())-1; i>=0; --i) {
    if (!effects_[i]->alive())
      effects_.erase(effects_.begin()+i);
    else
      effects_[i]->updateAndDraw(canvas(), dt);
  }
}

void NetworkView::zoomToSelected(float time, int order, Vec2 offset)
{
  using gmath::clamp;
  AABB bb;
  auto graphptr = graph();
  if (selectedItems_.empty()) {
    if (graphptr->items().empty())
      bb.merge(Vec2{0, 0});
    else
      graphptr->forEachItem([&bb](GraphItemPtr item) { bb.merge(item->aabb()); });
  } else {
    for (auto id : selectedItems_) {
      bb.merge(graphptr->get(id)->aabb());
    }
  }
  bb.expand(42);
  auto const viewSize = canvas()->viewSize();
  auto const viewScale =
    clamp(std::min(viewSize.x / bb.width(), viewSize.y / bb.height()), 0.02f, dpiScale());
  auto const destViewPos = bb.center() * viewScale;
  auto       anim        = getState<AnimationState>();
  if (anim && time > 0.01f) {
    anim->animateTo(canvas(), destViewPos, viewScale, time, order);
  } else {
    canvas()->setViewPos(destViewPos);
    canvas()->setViewScale(viewScale);
  }
}

void NetworkView::draw()
{
  auto vp       = canvas()->viewport().expanded(50);
  auto drawItem = [this, &vp](GraphItem* item) {
    auto state = GraphItemState::DEFAULT;
    if (!vp.intersects(item->aabb()))
      return;
    if (hiddenItems_.find(item->id()) != hiddenItems_.end())
      return;
    if (hiddenOnceItems_.find(item->id()) != hiddenOnceItems_.end())
      return;
    if (selectedItems_.find(item->id()) != selectedItems_.end())
      state = GraphItemState::SELECTED;
    else if (hoveringItem_ == item->id())
      state = GraphItemState::HOVERED;
    item->draw(canvas(), state);
  };
  // TODO: move ordered items into class member
  Vector<GraphItem*> orderedItems(graph()->items().size());
  std::transform(
    graph()->items().begin(),
    graph()->items().end(),
    orderedItems.begin(),
    [this, graph = graph()](ItemID id) { return graph->get(id).get(); });
  std::stable_sort(orderedItems.begin(), orderedItems.end(), [this](auto lhs, auto rhs) {
    return zCompare(lhs, rhs) < 0;
  });

  for (auto itemptr : orderedItems) {
    drawItem(itemptr);
  }

  for (auto state : states_) {
    if (state->active())
      state->draw(this);
  }

  if (readonly()) {
    auto gr = graph();
    auto pos = canvas()->viewSize() - Vec2(16, 16);
    auto style = Canvas::defaultTextStyle;
    char const * text = "READ ONLY";
    style.align = Canvas::TextAlign::Right;
    style.valign = Canvas::TextVerticalAlign::Bottom;
    style.size = Canvas::FontSize::Large;
    style.color = 0xAAAAAAff;
    if (gr->readonly()) {
      style.color = 0x888888ff;
      //tex        = "READ ONLY GRAPH";
    }
    if (gr->docRoot()->readonly()) {
      style.color = 0xBBBBBBff;
      //text      = "READ ONLY DOCUMENT";
    }
    canvas()->pushLayer(Canvas::Layer::Lower);
    canvas()->drawTextUntransformed(pos, text, style, dpiScale() * 1.3f);
    canvas()->popLayer();
  }
}

void NetworkView::onDocModified()
{
  if (graph_.expired()) {
    msghub::debug("graph is expired now, reset view to root");
    GraphView::reset(doc());
  }
}

void NetworkView::onGraphModified()
{
  HashSet<ItemID> validSelection;
  for (auto id : selectedItems_) {
    if (graph()->tryGet(id))
      validSelection.insert(id);
  }
  selectedItems_.swap(validSelection);
  validSelection.clear();
  for (auto id : hiddenItems_) {
    if (graph()->tryGet(id))
      validSelection.insert(id);
  }
  hiddenItems_.swap(validSelection);
  if (!graph()->tryGet(hoveringItem_))
    hoveringItem_ = ID_None;
  if (!graph()->tryGet(hoveringPin_.node))
    hoveringPin_ = PIN_None;
  for (auto state : states_) {
    if (state->active())
      state->onGraphModified(this);
  }
}

void NetworkView::reset(WeakGraphPtr graph)
{
  for (auto state : states_) {
    if (state->active()) {
      state->onExit(this);
      state->active_ = false;
    }
  }
  selectedItems_.clear();
  hiddenItems_.clear();
  zOrder_.clear();
  highZ_        = 0;
  hoveringItem_ = ID_None;
  hoveringPin_  = PIN_None;
  GraphView::reset(graph);
  update(0);
  zoomToSelected(0);
}

bool NetworkView::copyTo(Json& json)
{
  json = Json();
  if (selectedItems_.empty())
    return false;
  auto& itemsection = json["items"];
  auto& linksection = json["links"];
  auto  graphptr    = graph();
  for (auto id : selectedItems_) {
    auto item = graphptr->get(id);
    assert(item);
    // skip links, links within selected items will be serialized later
    if (!item->asLink()) {
      Json itemdata;
      itemdata["id"] = id.value();
      itemdata["f"]  = editor_->itemFactory()->factoryName(item);
      if (!item->serialize(itemdata)) {
        msghub::errorf("failed to serialize item {}", id.value());
        return false;
      }
      itemsection.push_back(itemdata);
      msghub::debugf("serialized {}", id.value());
    }
  }
  for (auto const& link : graphptr->allLinks()) {
    if (auto src = selectedItems_.find(link.second.sourceItem); src != selectedItems_.end()) {
      if (auto dst = selectedItems_.find(link.first.destItem); dst != selectedItems_.end()) {
        Json  linkdata;
        auto& from   = linkdata["from"];
        auto& to     = linkdata["to"];
        from["id"]   = link.second.sourceItem.value();
        from["port"] = link.second.sourcePort;
        to["id"]     = link.first.destItem.value();
        to["port"]   = link.first.destPort;
        linksection.push_back(linkdata);
        msghub::debugf("serialized link from {} to {}", src->value(), dst->value());
      }
    }
  }
  return true;
}

bool NetworkView::pasteFrom(Json const& json)
{
  auto                   edgroup = doc_->editGroup("paste");
  HashMap<size_t, ItemID> idmap; // from old id to new id
  HashSet<ItemID>        newitems;
  auto                   graphSharedPtr = graph();
  auto                   graphRawPtr    = graphSharedPtr.get();
  AABB                   bb;
  auto                   responser = editor()->responser();
  auto                   nodeFactory = graphRawPtr->nodeFactory();
  auto                   itemFactory = graphRawPtr->docRoot()->itemFactory();

  struct InplaceDeserializeScope {
    NodeGraphDoc* doc;
    bool inplacePreviously = true;

    InplaceDeserializeScope(NodeGraphDoc* doc):doc(doc), inplacePreviously(doc->deserializeInplace())
    {
      doc->setDeserializeInplace(false);
    }
    ~InplaceDeserializeScope()
    {
      doc->setDeserializeInplace(inplacePreviously);
    }
  } scope(graphRawPtr->docRoot());

  for (auto& itemdata : json["items"]) {
    String       factory = itemdata["f"];
    GraphItemPtr newitem;
    if (factory.empty() || factory == "node") {
      String type = itemdata["type"];
      newitem     = GraphItemPtr(nodeFactory->createNode(graphRawPtr, type));
    } else {
      newitem = itemFactory->make(graphRawPtr, factory);
    }
    if (!newitem || !newitem->deserialize(itemdata)) {
      msghub::errorf("failed to import item {}", itemdata.dump(2));
      return false;
    }
    GraphItem* replacedItem = nullptr;
    if (responser && !responser->beforeItemAdded(graphRawPtr, newitem.get(), &replacedItem)) {
      if (auto* newnode = newitem->asNode()) {
        msghub::infof("node {}({}) cannot be added", newnode->type(), newnode->name());
        graphRawPtr->docRoot()->nodeFactory()->discard(graphRawPtr, newnode);
      } else {
        msghub::infof("item {} cannot be added", factory);
        graphRawPtr->docRoot()->itemFactory()->discard(graphRawPtr, newitem.get());
      }
      continue;
    }
    if (replacedItem != nullptr) {
      idmap[itemdata["id"]] = replacedItem->id();
    } else {
      // newitem->setUID(generateUID()); // make new UID
      bb.merge(newitem->aabb());
      auto newid            = graphRawPtr->add(newitem);
      idmap[itemdata["id"]] = newid;
      newitems.insert(newid);
    }
    graphRawPtr->docRoot()->history().commitIfAppropriate("add item");
  }
  Vec2 center = canvas()->viewPos(); // view center in canvas space
  graphRawPtr->move(newitems, (center / canvas()->viewScale() - bb.center()));

  for (auto& linkdata : json["links"]) {
    auto& from = linkdata["from"];
    auto& to   = linkdata["to"];
    auto srcid = utils::get_or(idmap, from["id"], ID_None);
    auto dstid = utils::get_or(idmap, to["id"], ID_None);
    if (srcid == ID_None || dstid == ID_None)
      continue;
    if (!editor()->setLink(graphRawPtr, nullptr,
          srcid, sint(from["port"]), dstid, sint(to["port"]))) {
      msghub::errorf("failed to deserialize link {}", linkdata.dump(2));
      return false;
    }
  }

  for (auto id : newitems) {
    if (auto* group = graphRawPtr->get(id)->asGroupBox())
      group->remapItems(idmap);
  }
  selectedItems_ = std::move(newitems);
  for (auto id: selectedItems_)
    zOrder_[id] = ++highZ_;
  zoomToSelected(0.2f);

  if (responser) {
    std::vector<GraphItem*> vecNewItems;
    vecNewItems.reserve(selectedItems_.size());
    for (auto id: selectedItems_)
      vecNewItems.push_back(graphRawPtr->get(id).get());
    responser->afterPaste(graphRawPtr, vecNewItems.data(), vecNewItems.size());
  }

  return true;
}

static void copyToClipboard(GraphView* view, StringView)
{
  Json json;
  assert(view->kind() == "network");
  auto* nv = static_cast<NetworkView*>(view);
  if (nv->copyTo(json)) {
    view->editor()->setClipboardText(json.dump());
  }
}
/*
static void tryToEnterSubnet(NetworkView* view, ItemID id, bool inNewWindow)
  if (auto item = view->graph()->get(id)) {
    if (auto* node = item->asNode()) {
      if (auto* subgraph = node->asGraph()) {
        msghub::debugf("entering subgraph {}", static_cast<void*>(subgraph));
        if (inNewWindow) {
          if (auto network = view->editor()->addView(view->doc(), "network")) {
            network->reset(subgraph);
          }
        } else {
          view->reset(subgraph);
        }
      }
    }
  }
}
*/
void NetworkView::addCommands(CommandManager* mgr)
{
  mgr->add(new CommandManager::SimpleCommand{
    "View/FocusSelection",
    "Focus Selected ...",
    [](GraphView* view, StringView args) {
      auto* netview = static_cast<NetworkView*>(view);
      netview->zoomToSelected(0.2);
    },
    Shortcut{'F'},
    "network"}).setMayModifyGraph(false);

  mgr->add(new CommandManager::SimpleCommand{
    "Edit/VerticalAlign",
    "Vertical Align",
    [](GraphView* view, StringView args) {
      assert(view->kind() == "network");
      auto* nv  = static_cast<NetworkView*>(view);
      float x   = 0;
      float cnt = 0;

      Vector<GraphItemPtr> items;
      for (auto id : nv->selectedItems()) {
        if (auto item = view->graph()->get(id)) {
          if (item->canMove()) {
            x += item->pos().x;
            ++cnt;
            items.push_back(item);
          }
        }
      }
      if (cnt > 0) {
        auto edgroup = view->doc()->editGroup("vertical align");
        x /= cnt;
        for (auto item : items)
          item->moveTo({x, item->pos().y});
        view->graph()->updateLinkPaths(nv->selectedItems());
      }
    },
    Shortcut{'\\', ModKey::SHIFT},
    "network"});

  mgr->add(new CommandManager::SimpleCommand{
    "Edit/HorizontalAlign",
    "Horizontal Align",
    [](GraphView* view, StringView args) {
      assert(view->kind() == "network");
      auto* nv  = static_cast<NetworkView*>(view);
      float y   = 0;
      float cnt = 0;

      Vector<GraphItemPtr> items;
      for (auto id : nv->selectedItems()) {
        if (auto item = view->graph()->get(id)) {
          if (item->canMove()) {
            y += item->pos().y;
            ++cnt;
            items.push_back(item);
          }
        }
      }
      if (cnt > 0) {
        auto edgroup = view->doc()->editGroup("horizontal align");
        y /= cnt;
        for (auto item : items)
          item->moveTo({item->pos().x, y});
        view->graph()->updateLinkPaths(nv->selectedItems());
      }
    },
    Shortcut{'-', ModKey::SHIFT},
    "network"});

  mgr->add(new CommandManager::SimpleCommand{
    "Edit/SelectAll",
    "Select All",
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      auto*       nv  = static_cast<NetworkView*>(view);
      auto const& all = view->graph()->items();
      if (nv->selectedItems().size() >= all.size()) // toggle select all
        nv->setSelectedItems({});
      else
        nv->setSelectedItems(all);
    },
    Shortcut{'A', ModKey::CTRL},
    "network"}).setMayModifyGraph(false);

  mgr->add(new CommandManager::SimpleCommand{
    "Edit/Copy", "Copy", copyToClipboard, Shortcut{'C', ModKey::CTRL}, "network"}).setMayModifyGraph(false);
  mgr->add(new CommandManager::SimpleCommand{
    "Edit/Cut",
    "Cut",
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      copyToClipboard(view, "");
      auto&& selection = static_cast<NetworkView*>(view)->selectedItems();
      if (selection.empty())
        return;
      auto edit = view->graph()->docRoot()->editGroup("Cut");
      view->editor()->removeItems(view->graph().get(), selection);
    },
    Shortcut{'X', ModKey::CTRL},
    "network"});
  mgr->add(new CommandManager::SimpleCommand{
    "Edit/Paste",
    "Paste",
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      if (auto text = view->editor()->getClipboardText(); !text.empty()) {
        try {
          if (auto json = Json::parse(text); json.is_object()) {
            static_cast<NetworkView*>(view)->pasteFrom(json);
          }
        } catch (Json::parse_error&) {
          msghub::warn("not valid node graph data");
        }
      }
    },
    Shortcut{'V', ModKey::CTRL},
    "network"});

  mgr->add(new CommandManager::SimpleCommand{
    "Edit/Delete",
    "Delete Selection",
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      auto&& selection = static_cast<NetworkView*>(view)->selectedItems();
      if (selection.empty())
        return;
      view->editor()->removeItems(view->graph().get(), selection);
    },
    Shortcut{'\x7f', ModKey::NONE},
    "network"});

  mgr->add(new CommandManager::SimpleCommand{
    "Edit/GoToParent",
    "Go To Parent Graph",
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      if (view->graph() != view->doc()->root() && view->graph()->parent()) {
        auto subgraph = view->graph();
        auto parent = subgraph->parent();
        view->reset(parent?parent->shared_from_this():view->doc()->root());
        static_cast<NetworkView*>(view)->setSelectedItems({/*subgraph->id()*/});
      }
    },
    Shortcut{'U', ModKey::NONE},
    "network"}).setMayModifyGraph(false);

  mgr->add(new CommandManager::SimpleCommand{
    "View/ToggleDisplayTypeHint",
    "Toggle Display Type Hint",
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      auto* nv = static_cast<NetworkView*>(view);
      nv->canvas()->setDisplayTypeHint(!nv->canvas()->displayTypeHint());
    },
    Shortcut{'T', ModKey::ALT},
    "network"}).setMayModifyGraph(false);

  /* currently still handled in HandleShortcut state
  mgr->add(CommandManager::Command{
    "Edit/EnterSubnet", "Enter Subnet",
    "network", "",
    Shortcut{'\r', ModKey::NONE},
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      tryToEnterSubnet(static_cast<NetworkView*>(view), false);
    }
  };
  mgr->add(CommandManager::Command{
    "Edit/EnterSubnetNewWindow", "Enter Subnet In New Window",
    "network", "",
    Shortcut{'\r', ModKey::SHIFT},
    [](GraphView* view, StringView) {
      assert(view->kind() == "network");
      tryToEnterSubnet(static_cast<NetworkView*>(view), true);
    }
  };
  */
}
// }}} NetworkView

// InspectorView {{{
InspectorView::InspectorView(NodeGraphEditor* editor) : GraphView(editor, nullptr)
{
  setTitle("Inspector");
  for (auto&& view : editor->views()) {
    if (view->isFocused() && view->kind() == "network") {
      auto* netview = static_cast<NetworkView*>(view.get());
      if (netview->selectedItems().size() == 1) {
        setInspectingItems(netview->selectedItems());
        break;
      }
    }
  }
}

void InspectorView::removeExpiredItems()
{
  Vector<ItemID> expired;
  for (auto&& id : inspectingItems_) {
    if (!graph()->tryGet(id))
      expired.push_back(id);
  }
  for (auto id : expired) {
    inspectingItems_.erase(id);
  }
}

void InspectorView::setInspectingItems(HashSet<ItemID> const& ids) { inspectingItems_ = ids; }

void InspectorView::onDocModified()
{
  if (linkedView_.expired())
    linkedView_.reset();
  removeExpiredItems();
}

void InspectorView::onGraphModified()
{
  removeExpiredItems();
  if (inspectingItems_.empty()) {
    lockOnItem_ = false;
  }
}

void InspectorView::onViewEvent(GraphView* view, StringView eventType)
{
  if (lockOnItem_)
    return;
  if (view && view->kind() == "network" && eventType == "selectionChanged") {
    if (!lockOnView_)
      linkedView_ = view->weak_from_this();

    auto linkview = linkedView_.lock();
    auto netview  = static_cast<NetworkView*>(view);
    auto cnt      = netview->selectedItems().size();
    msghub::debugf(
      "network view ({}) selection changed, {} selected", static_cast<void*>(view), cnt);
    if (!linkview || (linkview && linkview.get() == view)) {
      reset(view->graph());
      setInspectingItems(netview->selectedItems());
    }
  }
}
// }}} InspectorView

// NodeGraphEditor {{{
// Shortcut {{{
Shortcut Shortcut::parse(String shortcutStr)
{
  // convert all to upper case
  for(auto& c: shortcutStr)
    c=std::toupper(c);
  auto parts = utils::strsplit(shortcutStr, "+");
  Shortcut result;
  auto assignKey = [&result](uint8_t newkey){
    if(result.key!=0){
      msghub::warnf("key already assigned with '{}', will be replaced by '{}'", result.key, newkey);
    }
    result.key = newkey;
  };
  for(auto part: parts) {
    part = utils::strstrip(part);
    if (part=="CTRL")
      result.mod = utils::eor(result.mod, ModKey::CTRL);
    else if (part=="SHIFT")
      result.mod = utils::eor(result.mod, ModKey::SHIFT);
    else if (part=="ALT" || part=="META")
      result.mod = utils::eor(result.mod, ModKey::ALT);
    else if (part=="SUPER" || part=="WIN")
      result.mod = utils::eor(result.mod, ModKey::SUPER);
    else if (part.size() == 2 && part[0]=='F' && part[1]>='1' && part[1]<='9')
      assignKey(0xf0+part[1]-'0');
    else if (part == "F10" || part=="F11" || part=="F12")
      assignKey(0xFA+part[2]-'0');
    else if (part == "ESC" || part=="ESCAPE")
      assignKey('\x1b');
    else if (part == "TAB")
      assignKey('\t');
    else if (part == "ENTER")
      assignKey('\r');
    else if (part == "BACK" || part == "BACKSPACE")
      assignKey('\b');
    else if (part == "DEL" || part == "DELETE")
      assignKey('\x7f');
    else if (part.size() == 1)
      assignKey(part[0]);
    else
      msghub::warnf("Cannot translate \"{}\" inside key sequence \"{}\" into valid shortcut", part, shortcutStr);
  }
  return result;
}

static inline StringView asciiToName(uint8_t ch)
{
  char const* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char const* num   = "0123456789";
  if (ch >= '0' && ch <= '9') {
    return {num + ch - '0', 1};
  } else if (ch >= 'A' && ch <= 'Z') {
    return {alpha + ch - 'A', 1};
  } else if (ch >= 'a' && ch <= 'z') {
    return {alpha + ch - 'a', 1};
  } else if (ch >= 0xF1 && ch <= 0xFC) {
    static constexpr char const* fKeys[] = {
      "F1", "F2", "F3", "F4", "F5", "F6", 
      "F7", "F8", "F9", "F10", "F11", "F12", 
    };
    return fKeys[ch-0xF1];
  } else {
    switch (ch) {
    case '\t': return "Tab";
    case '\r': return "Enter";
    case '\b': return "Backspace";
    case '\x7f': return "Delete";
    case '\x1b': return "Escape";
    case '`': return "`";
    case ' ': return "Space";
    case ',': return ",";
    case '-': return "-";
    case '.': return ".";
    case '/': return "/";
    case '\\': return "\\";
    case ';': return ";";
    case '=': return "=";
    case '[': return "[";
    case ']': return "]";
    default: return "";
    }
  }
  return "";
}

String Shortcut::describ(Shortcut shortcut)
{
  Vector<StringView> shortcutKeys;
  if (shortcut.key) {
    if (ModKey::NONE != utils::eand(shortcut.mod, ModKey::SUPER))
      shortcutKeys.push_back("Super");
    if (ModKey::NONE != utils::eand(shortcut.mod, ModKey::CTRL))
      shortcutKeys.push_back("Ctrl");
    if (ModKey::NONE != utils::eand(shortcut.mod, ModKey::SHIFT))
      shortcutKeys.push_back("Shift");
    if (ModKey::NONE != utils::eand(shortcut.mod, ModKey::ALT))
      shortcutKeys.push_back("Alt");
    shortcutKeys.push_back(asciiToName(shortcut.key));
    return fmt::format("{}", fmt::join(shortcutKeys, " + "));
  } else {
    return "";
  }
}

// }}}
// Builtin Commands  {{{
void NodeGraphEditor::initCommands()
{
  auto& mgr           = commandManager_;
  using SimpleCommand = CommandManager::SimpleCommand;
  mgr.add(new SimpleCommand{
    "File/Save",
    "Save current document",
    [](GraphView* view, StringView args) { view->editor()->saveDoc(view->doc()); },
    Shortcut{'S', ModKey::CTRL},
    "*",
    "",
    [](GraphView* view) -> String { return String(view->doc()->savePath()); }}).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "File/SaveAs",
    "Save current document as ...",
    [](GraphView* view, StringView args) { view->editor()->saveDocAs(view->doc(), args); },
    Shortcut{'S', utils::eor(ModKey::CTRL, ModKey::SHIFT)},
    "*",
    "File Path",
    [](GraphView* view) -> String { return String(view->doc()->savePath()); }}).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "File/Open",
    "Open document ...",
    [](GraphView* view, StringView args) {
      if (view->doc() && !view->doc()->everEdited() && !view->doc()->dirty())
        view->editor()->loadDocInto(args, view->doc());
      else
        view->editor()->openDoc(args);
    },
    Shortcut{'O', ModKey::CTRL},
  }).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "Edit/Undo",
    "Undo",
    [](GraphView* view, StringView args) {
      if (auto doc = view->doc())
        doc->undo();
      else if (auto graph = view->graph())
        graph->docRoot()->undo();
      else
        msghub::error("cannot undo, this view has no related doc object");
    },
    Shortcut{'Z', ModKey::CTRL},
    "network|inspector"
  });
  mgr.add(new SimpleCommand{
    "Edit/Redo",
    "Redo",
    [](GraphView* view, StringView args) {
      if (auto doc = view->doc())
        doc->redo();
      else if (auto graph = view->graph())
        graph->docRoot()->redo();
      else
        msghub::error("cannot redo, this view has no related doc object");
    },
    Shortcut{'R', ModKey::CTRL},
  });
  mgr.add(new SimpleCommand{
    "Edit/Rename",
    "Rename",
    [](GraphView* view, StringView args) {
      if (view->kind() == "network") {
        auto* netview = static_cast<NetworkView*>(view);
        if (auto node = netview->solySelectedNode()) {
          String newname(args);
          String oldname = node->name();
          if (!node->rename(String(args), newname))
            msghub::warnf("cannot rename node to {}", args);
          else {
            msghub::debugf("rename node {} to {}", oldname, newname);
            view->graph()->docRoot()->history().commitIfAppropriate("rename node");
          }
          return;
        } else {
          msghub::warn("select one node to rename");
        }
      }
      msghub::warnf("cannot rename this item");
    },
    Shortcut{0xF2},
    "network",
    "New Name",
    [](GraphView* view)->String {
      if (view->kind() == "network") {
        auto* netview = static_cast<NetworkView*>(view);
        if (auto node = netview->solySelectedNode()) {
          return String(node->name());
        } else {
          return "Select ONE AND ONLY ONE NODE to rename";
        }
      }
      return "CANNOT RENAME THIS ITEM";
    }
  });
  mgr.add(new SimpleCommand{
    "View/OpenCommandPalette",
    "Open Palette",
    [](GraphView* view, StringView args) { view->editor()->commandManager().openPalette(); },
    Shortcut{'P', ModKey::CTRL},
    "*",
    "",
    nullptr,
    true}).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "File/Quit",
    "Quit",
    [](GraphView* view, StringView args) { /*TODO: post quit message*/ },
    Shortcut{'Q', ModKey::CTRL},
  }).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "File/New",
    "New Document ...",
    [](GraphView* view, StringView args) { view->editor()->createNewDocAndDefaultViews(); },
    Shortcut{'N', ModKey::CTRL},
  }).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "View/Close",
    "Close Current View",
    [](GraphView* view, StringView args) { view->editor()->closeView(view->shared_from_this()); },
    Shortcut{'W', ModKey::CTRL},
  }).setMayModifyGraph(false);

  NetworkView::addCommands(&mgr);

  // clang-format off
  mgr.add(new SimpleCommand{
    "View/Network",
    "Open Network View",
    [](GraphView* view, StringView args) {
      if (view->doc() && view->graph()) {
        if (auto netview = view->editor()->addView(view->doc(), "network")) {
          netview->reset(view->graph());
        }
      } else {
        msghub::error(
          "no nodegraph to add network view to, please open or create a nodegraph first");
      }
    },
    Shortcut{'W', utils::eor(ModKey::SHIFT, ModKey::ALT)}
  }).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "View/Inspector",
    "Open Inspector View",
    [](GraphView* view, StringView args) {
      if (auto inspector = view->editor()->addView(view->doc(), "inspector")) {
        static_cast<InspectorView*>(inspector.get())->linkToView(view);
      }
    }
  }).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "View/Messages",
    "Open Messages View",
    [](GraphView* view, StringView args) {
      if (auto msgview = view->editor()->addView(view->doc(), "message")) {
        msghub::info("message view opened");
      }
    }
  }).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "Message/ClearOutput",
    "Clear Output",
    [](GraphView* view, StringView args) {
      msghub::instance().clear(msghub::Category::Output);
    },
    Shortcut{'L', ModKey::CTRL},
    "message"
  }).setMayModifyGraph(false);

  // -------------- debug commands ----------------
#ifdef DEBUG
  mgr.add(new SimpleCommand{
    "Debug/TravelUp",
    "[Debug] Travel From Here",
    [](GraphView* view, StringView args) {
      bool  topdown = args.length() > 0 && args[0] == 'd';
      auto* nv      = static_cast<NetworkView*>(view);
      if (nv->selectedItems().size() > 0) {
        GraphTraverseResult bfs;
        Vector<ItemID>      starts{nv->selectedItems().begin(), nv->selectedItems().end()};
        bool                succeed = topdown ? nv->graph()->travelTopDown(bfs, starts)
                                              : nv->graph()->travelBottomUp(bfs, starts);
        if (succeed) {
          for (auto acc : bfs) {
            msghub::outputf("node: {}", acc->name());
            if (acc.inputCount() > 0) {
              msghub::outputf("  inputs: ({})", acc.inputCount());
              for (int i = 0; i < acc.inputCount(); ++i) {
                msghub::outputf("    {}", acc.input(i) ? acc.input(i)->name() : "<nil>");
              }
            }
          }
        }
      }
    },
    Shortcut{},
    "network",
    "Direction (up/down):"}).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "Debug/CheckHistroySize",
    "[Debug] Check History Memory Usage",
    [](GraphView* view, StringView args) {
      if (auto graph = view->graph()) {
        auto bytes = graph->docRoot()->history().memoryBytesUsed();
        if (bytes >= 1024*1024)
          msghub::outputf("{:.2f}MB", bytes/float(1024*1024));
        else
          msghub::outputf("{:.2f}KB", bytes/float(1024));
      }
    },
    Shortcut{},
    "network"}).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "Debug/ToggleDocReadonly",
    "[Debug] Toggle Document Readonly",
    [](GraphView* view, StringView args) {
      view->graph()->docRoot()->setReadonly(!view->graph()->docRoot()->readonly());
    },
    Shortcut{},
    "network"}).setMayModifyGraph(false);
  mgr.add(new SimpleCommand{
    "Debug/ToggleGraphReadonly",
    "[Debug] Toggle Graph Readonly",
    [](GraphView* view, StringView args) {
      view->graph()->setSelfReadonly(!view->graph()->selfReadonly());
    },
    Shortcut{},
    "network"}).setMayModifyGraph(false);
#endif
  // clang-format on
}
// }}} Builtin Commands

NodeGraphEditor::ViewPtr NodeGraphEditor::addView(NodeGraphEditor::DocPtr doc, String const& kind)
{
  ViewPtr view = viewFactory_->createView(kind, this, doc);
  if (!view)
    return nullptr;
  pendingAddViews_.insert(view);
  return view;
}

NodeGraphEditor::DocPtr NodeGraphEditor::createNewDocAndDefaultViews()
{
  auto doc = docFactory_(nodeFactory_, itemFactory_.get());
  doc->history().reset(true);
  doc->history().markSaved();
  doc->setModifiedNotifier([this](Graph* g) { notifyGraphModified(g); });
  addView(doc, "network");
  return doc;
}

// TODO: warn if doc is already open

bool NodeGraphEditor::loadDocInto(StringView path, NodeGraphDocPtr dest)
{
  if (!dest) {
    msghub::error("trying to load doc into null");
    return false;
  }
  String filepath = String(path);
  if (filepath.empty()) {
    char* cpath  = nullptr;
    auto  result = NFD_OpenDialog(fileExt_.c_str(), nullptr, &cpath);
    if (result == NFD_OKAY && cpath) {
      if (*cpath)
        filepath = cpath;
      free(cpath);
    } else if (result == NFD_CANCEL) {
      return false;
    }
  }
  if (!filepath.empty() && dest->open(filepath)) {
    for (auto&& view : views_) {
      if (view->doc() == dest) {
        view->reset(dest->root());
        view->setTitle(String(dest->title()));
      }
    }
    return true;
  } else {
    MessageHub::noticef("cannot open document \"{}\"", filepath);
  }
  return false;
}

NodeGraphEditor::DocPtr NodeGraphEditor::openDoc(StringView path)
{
  auto doc = docFactory_(nodeFactory_, itemFactory_.get());
  doc->setModifiedNotifier([this](Graph* g) { notifyGraphModified(g); });
  if (!loadDocInto(path, doc)) {
    return nullptr;
  } else {
    if (auto newview = addView(doc, "network"))
      newview->setTitle(String(doc->title()));
    doc->history().reset(true);
    return doc;
  }
}

bool NodeGraphEditor::saveDoc(DocPtr doc)
{
  bool succeed = false;
  if (doc->savePath().empty()) {
    char* path   = nullptr;
    auto  result = NFD_SaveDialog(fileExt_.c_str(), nullptr, &path);
    if (result == NFD_OKAY && path) {
      succeed = doc->saveAs(path);
      if (succeed) {
        for (auto view : views_) {
          if (view->doc() == doc) {
            view->setTitle(String(doc->title()));
          }
        }
      }
      free(path);
    } else {
      succeed = false;
    }
  } else {
    succeed = doc->save();
  }
  if (succeed)
    doc->history().markSaved();
  return succeed;
}

bool NodeGraphEditor::saveDocAs(DocPtr doc, StringView inputpath)
{
  bool succeed = false;
  if (inputpath.empty()) {
    char* path   = nullptr;
    auto  result = NFD_SaveDialog(fileExt_.c_str(), nullptr, &path);
    if (result == NFD_OKAY && path) {
      succeed = doc->saveAs(path);
      free(path);
    } else {
      succeed = false;
    }
  } else {
    succeed = doc->saveAs(String(inputpath));
  }
  if (succeed) {
    for (auto view : views_) {
      if (view->doc() == doc) {
        view->setTitle(String(doc->title()));
      }
    }
  }
  return succeed;
}

void NodeGraphEditor::update(float dt)
{
  for (auto& view : pendingAddViews_)
    views_.insert(std::move(view));
  pendingAddViews_.clear();
  for (auto view : pendingRemoveViews_) {
    auto doc = view->doc();
    if (doc && doc.use_count() <= dyingRefCount_+2) // <- one held by the `doc` var here, one held by `view`
      beforeDocRemoved(doc);
    views_.erase(view);
    if (responser_)
      responser_->afterViewRemoved(view.get());
  }
  pendingRemoveViews_.clear();

  for (auto const& view : views_) {
    if (responser_)
      responser_->beforeViewUpdate(view.get());

    view->update(dt);

    if (responser_)
      responser_->afterViewUpdate(view.get());
  }
}

void NodeGraphEditor::notifyGraphModified(Graph* graph)
{
  for (auto&& v : views_) {
    if (v->doc().get() == graph->docRoot())
      v->onDocModified();
    if (v->graph().get() == graph)
      v->onGraphModified();
  }
}

void NodeGraphEditor::boardcastViewEvent(GraphView* view, StringView eventType)
{
  for (auto&& v : views_) {
    if (v.get() != view)
      v->onViewEvent(view, eventType);
  }
}

bool NodeGraphEditor::closeView(ViewPtr view, bool needConfirm)
{
   int ref = 0;
  if (view->doc()) {
    for (auto&& v : views_) {
      if (v->doc() == view->doc())
        ++ref;
    }
  }
  bool confirmed = !needConfirm || ref > 1 || !view->doc();
  if (ref == 1) { // only one view is referencing this doc
    auto doc     = view->doc();
    auto message = fmt::format("\"{}\" has unsaved edit, are you sure to close?", doc->title());
    if (
      !doc->dirty() ||
      needConfirm && boxer::show(message.c_str(), "Close View", boxer::Buttons::YesNo) ==
                       boxer::Selection::Yes) {
      confirmed = true;
    }
  }
  if (confirmed)
    removeView(view);
  return confirmed;
}

void NodeGraphEditor::removeView(ViewPtr view)
{
  if (responser_ && !responser_->beforeViewRemoved(view.get()))
    return;
  pendingRemoveViews_.insert(view);
  if (views_.size() == pendingRemoveViews_.size())
    this->createNewDocAndDefaultViews(); // keep at least one view
}

bool NodeGraphEditor::agreeToQuit() const
{
  HashSet<DocPtr> docs;
  for (auto const& view : views_)
    if (view->doc() && view->doc()->dirty())
      docs.insert(view->doc());

  if (!docs.empty()) {
    Vector<String> titles;
    for (auto doc : docs) {
      titles.push_back(fmt::format("\"{}\"", doc->title()));
    }
    auto message =
      fmt::format("{} has unsaved edit, are you sure to close?", fmt::join(titles, ", "));
    if (boxer::show(message.c_str(), "Close View", boxer::Buttons::YesNo) != boxer::Selection::Yes)
      return false;
  }
  return true;
}

void NodeGraphEditor::switchMessageTab(StringView tab)
{
  for (auto v: views()) {
    if (v->kind() == "message")
      v->please(fmt::format("open {} tab", tab));
  }
}

NodePtr NodeGraphEditor::createNode(Graph* graph, StringView type)
{
  auto nodeptr = NodePtr(graph->nodeFactory()->createNode(graph, type));
  GraphItem* replacement = nullptr;
  if (responser_ && !responser_->beforeItemAdded(graph, nodeptr.get(), &replacement))
    return nullptr;
  if (replacement) {
    if (auto* nodeptr = replacement->asNode())
      return std::static_pointer_cast<Node>(nodeptr->shared_from_this());
    else
      return nullptr;
  }
  graph->add(nodeptr);
  graph->docRoot()->history().commitIfAppropriate("add node");
  if (responser_)
    responser_->afterItemAdded(graph, nodeptr.get());
  return nodeptr;
}

ItemID NodeGraphEditor::addItem(Graph* graph, GraphItemPtr itemptr)
{
  GraphItem* replacement = nullptr;
  if (responser_ && !responser_->beforeItemAdded(graph, itemptr.get(), &replacement))
    return ID_None;
  if (replacement)
    return replacement->id();
  auto id = graph->add(itemptr);
  graph->docRoot()->history().commitIfAppropriate("add item");
  if (responser_)
    responser_->afterItemAdded(graph, itemptr.get());
  return id;
}

void NodeGraphEditor::confirmItemPlacements(Graph* graph, HashSet<ItemID> const& items)
{
  if (responser_) {
    for (auto id: items) {
      auto itemptr = graph->get(id);
      responser_->onItemMoved(itemptr.get());
    }
  }
  for (auto id: graph->items()) {
    auto itemptr = graph->get(id);
    if (auto* group = itemptr->asGroupBox()) {
      for (auto movedid: items) {
        if (id == movedid)
          continue;
        auto moveditem = graph->get(movedid);
        if (group->aabb().contains(moveditem->aabb()))
          group->insertItem(movedid);
        else
          group->eraseItem(movedid);
      }
    }
  }
}

bool NodeGraphEditor::moveItems(Graph* graph, HashSet<ItemID> const& items, Vec2 delta)
{
  graph->move(items, delta);
  confirmItemPlacements(graph, items);
  return true;
}

void NodeGraphEditor::removeItems(Graph* graph, HashSet<ItemID> const& items, HashSet<ItemID>* remaining)
{
  HashSet<ItemID> itemsToRemove;
  HashSet<Link*>  linksToRemove;
  HashMap<OutputConnection, InputConnection> linksToRestore;
  for (auto id: items) {
    auto itemptr = graph->get(id);
    if (responser_ && !responser_->beforeItemRemoved(graph, itemptr.get())) {
      if (remaining) remaining->insert(id);
    } else {
      if (auto* linkptr = itemptr->asLink())
        linksToRemove.insert(linkptr);
      else
        itemsToRemove.insert(id);
    }
  }
  if (items.size() == linksToRemove.size()) { // delete links only if there are nothing else selected
    if (responser_) {
      for (auto* linkptr : linksToRemove)
        responser_->onLinkRemoved(linkptr);
    }
    for (auto* linkptr : linksToRemove)
      itemsToRemove.insert(linkptr->id());
  } else { // otherwise, try to restore links
    auto const& links = graph->allLinks();
    for (auto&& pair: links) {
      if (itemsToRemove.find(pair.first.destItem) != itemsToRemove.end())
        continue;
      auto inconn = pair.second;
      bool foundRestorePath = false;
      for (auto itr = itemsToRemove.find(inconn.sourceItem); itr != itemsToRemove.end();) {
        auto linkupitr = links.find({inconn.sourceItem, 0});
        if (linkupitr != links.end()) {
          inconn = linkupitr->second;
          itr = itemsToRemove.find(inconn.sourceItem);
          foundRestorePath = true;
        } else {
          inconn.sourceItem = ID_None;
          foundRestorePath = false;
          break;
        }
      }
      if (foundRestorePath && inconn.sourceItem != ID_None)
        linksToRestore[pair.first] = inconn;
    }
  }
  if (itemsToRemove.empty())
    return;
  auto edgroup = graph->docRoot()->editGroup("remove items");
  for (auto&& pair: linksToRestore) {
    setLink(graph, nullptr, pair.second.sourceItem, pair.second.sourcePort, pair.first.destItem, pair.first.destPort);
  }
  graph->remove(itemsToRemove);
}

bool NodeGraphEditor::setLink(Graph* graph, NetworkView* fromView, ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort)
{
  if (NodePin errPin;
      fromView &&
      sourceItem != ID_None &&
      destItem != ID_None &&
      !fromView->graph()->checkLinkIsAllowed(
        sourceItem, sourcePort, destItem, destPort, &errPin)) {
    auto errPos = graph->pinPos(errPin);
    fromView->addFadingText("link here with current input is not allowed", errPos);
    return false;
  }
  if (responser_ &&
      !responser_->beforeLinkSet(
        graph, InputConnection{sourceItem, sourcePort}, OutputConnection{destItem, destPort})) {
    if (fromView) {
      auto errPos = graph->pinPos(NodePin{destItem, destPort, NodePin::Type::In});
      fromView->addFadingText("link here with current input is not allowed", errPos);
    }
    return false;
  }
  auto existing = graph->getLink(destItem, destPort);
  bool anythingDone = false;
  if (existing && responser_) {
    responser_->onLinkRemoved(existing.get());
    anythingDone = true;
  }
  if (auto linkptr = graph->setLink(sourceItem, sourcePort, destItem, destPort)) {
    if (responser_)
      responser_->onLinkSet(linkptr.get());
    anythingDone = true;
  }
  if (anythingDone)
    graph->docRoot()->history().commitIfAppropriate("set link");
  return true;
}

void NodeGraphEditor::swapInput(
  Graph* graph,
  ItemID oldSourceItem,
  sint oldSourcePort,
  ItemID newSourceItem,
  sint newSourcePort,
  ItemID destItem,
  sint destPort)
{
  auto editscope = graph->docRoot()->editGroup("swap input");
  if (Vector<OutputConnection> ocs; graph->getLinkDestiny(newSourceItem, newSourcePort, ocs)) {
    for (auto&& oc: ocs) 
      setLink(graph, nullptr, oldSourceItem, oldSourcePort, oc.destItem, oc.destPort);
  }
  setLink(graph, nullptr, newSourceItem, newSourcePort, destItem, destPort);
}

void NodeGraphEditor::swapOutput(
  Graph* graph,
  ItemID sourceItem,
  sint sourcePort,
  ItemID oldDestItem,
  sint oldDestPort,
  ItemID newDestItem,
  sint newDestPort)
{
  auto editscope = graph->docRoot()->editGroup("swap output");
  if (InputConnection ic; graph->getLinkSource(newDestItem, newDestPort, ic)) {
    setLink(graph, nullptr, ic.sourceItem, ic.sourcePort, oldDestItem, oldDestPort);
  }
  setLink(graph, nullptr, sourceItem, sourcePort, newDestItem, newDestPort);
}

void NodeGraphEditor::removeLink(Graph* graph, ItemID destItem, sint destPort)
{
  auto existing = graph->getLink(destItem, destPort);
  if (!existing)
    return;
  if (responser_)
    responser_->onLinkRemoved(existing.get());
  graph->removeLink(destItem, destPort);
  graph->docRoot()->history().commitIfAppropriate("remove link");
}
// }}} NodeGraphEditor

} // namespace nged
