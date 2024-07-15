#pragma once

#include "ngdoc.h"
namespace nged {

// View {{{
class GraphView : public std::enable_shared_from_this<GraphView>
{
protected:
  NodeGraphDocPtr  doc_ = nullptr;
  WeakGraphPtr     graph_;
  String           kind_      = "unknown"; // can be "network", "inspector" ...
  String           title_     = "untitled";
  size_t           id_        = 0;
  bool             open_      = true;
  bool             isFocused_ = false;
  bool             isHovered_ = false;
  NodeGraphEditor* editor_    = nullptr;

  friend class ViewFactory;

public:
  GraphView(NodeGraphEditor* editor, NodeGraphDocPtr doc);
  virtual void postInit() {}

  NodeGraphDocPtr  doc() const { return doc_; }
  GraphPtr         graph() const { return graph_.lock(); }
  NodeGraphEditor* editor() const { return editor_; }
  StringView       kind() const { return kind_; }
  String const&    title() const { return title_; }
  void             setTitle(String title) { title_ = std::move(title); }
  size_t           id() const { return id_; }
  bool             isOpen() const { return open_; }
  void             setOpen(bool open) { open_ = open; }
  void             setFocused(bool focus) { isFocused_ = focus; }
  bool             isFocused() const { return isFocused_; }
  void             setHovered(bool hovered) { isHovered_ = hovered; }
  bool             isHovered() const { return isHovered_; }
  bool             readonly() const;

  virtual ~GraphView() = default;
  virtual float dpiScale() const { return 1.0f; }
  virtual Vec2  defaultSize() const { return {800, 600}; }
  virtual void  reset(NodeGraphDocPtr doc);
  virtual void  reset(WeakGraphPtr graph);
  virtual void  update(float dt);
  virtual void  onDocModified()   = 0;
  virtual void  onGraphModified() = 0;
  virtual void  draw()            = 0;

  // events are customable, may be "focus", "select", "delete", ...
  virtual void onViewEvent(GraphView* view, StringView eventType) {}
  // try to execute certain command, but anything can happen in response,
  // so make sure the caller and callee are friends and they know each other well.
  virtual void please(StringView request) {}

  virtual bool hasMenu() const { return false; }
  virtual void updateMenu() {}
};
using GraphViewPtr = std::shared_ptr<GraphView>;

class ViewFactory
{
protected:
  static void finalize(GraphView* viewptr, String kind, NodeGraphEditor* editor)
  {
    viewptr->kind_   = std::move(kind);
    viewptr->editor_ = editor;
    viewptr->postInit();
  }

public:
  virtual ~ViewFactory() = default;
  virtual GraphViewPtr createView(String const& kind, NodeGraphEditor* editor, NodeGraphDocPtr doc) const = 0;
};
using ViewFactoryPtr = std::shared_ptr<ViewFactory>;

ViewFactoryPtr defaultViewFactory();
// }}} View

// NetworkView & Interaction {{{
class NetworkView : public GraphView
{
public:
  class InteractionState
  {
    bool active_ = false;
    friend class NetworkView;

  public:
    virtual ~InteractionState() = default;

    bool active() const { return active_; }

    virtual StringView name() const { return "unknown"; }
    virtual int        priority() const
    {
      return 50;
    } // states with smaller priority value will be updated first

    virtual bool shouldEnter(NetworkView const* view) const { return false; }
    virtual void onEnter(NetworkView* view) {}
    virtual bool shouldExit(NetworkView const* view) const { return true; }
    virtual void onExit(NetworkView* view) {}

    virtual void tick(NetworkView* view, float dt) {} // called every frame if active
    virtual bool update(NetworkView* view)
    {
      return false;
    }                                       // will block following states::update() if return true
    virtual void draw(NetworkView* view) {} // called every frame if active

    virtual void onGraphModified(NetworkView* view) {}
  };
  template<class Derived>
  class NamedInteractionState : public InteractionState
  {
  public:
    StringView name() const override final { return Derived::className; }
  };
  using InteractionStatePtr = std::shared_ptr<InteractionState>;

  class Effect
  {
  public:
    virtual ~Effect() {}
    virtual void updateAndDraw(Canvas* canvas, float dt) = 0;
    virtual bool alive() const { return false; } // will be removed once dead
  };
  class FadingText : public Effect
  {
    String text_;
    Vec2   pos_;
    Color  color_    = {255,255,255,255};
    float  duration_ = 0.5f;
    float  age_      = 0.0f;

  public:
    FadingText(String text, Vec2 pos, Color color, float duration):
      text_(std::move(text)),
      pos_(pos),
      color_(color),
      duration_(duration),
      age_(0.f)
    { }
    void updateAndDraw(Canvas* canvas, float dt) override
    {
      age_ += dt;
      float t = gmath::clamp(age_/duration_, 0.f, 1.f);
      color_.a = static_cast<uint8_t>((1-utils::ease::inQuad(t)) * 255);
      auto style = Canvas::TextStyle {
        Canvas::TextAlign::Center,
        Canvas::TextVerticalAlign::Center,
        Canvas::FontFamily::SansSerif,
        Canvas::FontStyle::Regular,
        Canvas::FontSize::Large,
        toUint32RGBA(color_)
      };
      auto bgstyle = Canvas::ShapeStyle {
        true, toUint32RGBA(Color{0,0,0,color_.a}),
        2.f,  toUint32RGBA(color_)
      };
      auto halfSize = canvas->measureTextSize(text_, style)*0.5f+Vec2{16,8};
      canvas->drawRect(pos_ - halfSize, pos_ + halfSize, 4.f, bgstyle);
      canvas->drawText(pos_, text_, style);
    }
    bool alive() const override
    {
      return age_ <= duration_;
    }
  };

  template <class Effect, class ...Args>
  void addEffect(Args&& ...args)
  {
    effects_.emplace_back(new Effect(std::forward<Args>(args)...));
  }
  void addFadingText(String text, Vec2 pos, Color color={255,0,0,255}, float duration=1.0f)
  {
    addEffect<FadingText>(text, pos, color, duration);
  }

  enum class NavDirection
  {
    Up, Down, Left, Right
  };

protected:
  std::unique_ptr<Canvas> canvas_ = {nullptr};
  Vector<std::unique_ptr<Effect>> effects_;

  bool            canvasIsFocused_ = false;
  HashSet<ItemID> selectedItems_   = {};
  HashSet<ItemID> hiddenItems_     = {};
  HashSet<ItemID> hiddenOnceItems_ = {}; // hidden for one frame
  ItemID          hoveringItem_    = ID_None;
  NodePin         hoveringPin_     = PIN_None;

  size_t                      highZ_ = 0;
  HashMap<ItemID, size_t>     zOrder_;

  struct InteractionStateFactory
  {
    InteractionState* (*creator)(void*);
    void               *arg;
  };
  static Vector<InteractionStateFactory> stateFactories_;
  Vector<InteractionStatePtr>            states_ = {};

  // std::unordered map does not support Heterogeneous lookup for C++17
  // so we use phmap::flat_hash_map for now.
  HashMap<String, InteractionStatePtr> stateTypeMap_ = {};

  void updateAndDrawEffects(float dt);

public:
  NetworkView(NodeGraphEditor* editor, NodeGraphDocPtr doc, std::unique_ptr<Canvas> canvas);
  virtual ~NetworkView() {}
  virtual void initInteractionStates();
  virtual void postInit() override { initInteractionStates(); }

  Canvas*     canvas() const { return canvas_.get(); }
  bool        canvasIsFocused() const { return canvasIsFocused_; }
  void        setCanvasIsFocused(bool f) { canvasIsFocused_ = f; }
  auto const& selectedItems() const { return selectedItems_; }
  void        setSelectedItems(HashSet<ItemID> items);
  ItemID      hoveringItem() const { return hoveringItem_; }
  void        setHoveringItem(ItemID item) { hoveringItem_ = item; }
  NodePin     hoveringPin() const { return hoveringPin_; }
  void        setHoveringPin(NodePin pin) { hoveringPin_ = pin; }
  auto const& hiddenItems() const { return hiddenItems_; }
  void        hideItem(ItemID id) { hiddenItems_.insert(id); }
  void        hideItemOnce(ItemID id) { hiddenOnceItems_.insert(id); }
  void        unhideItem(ItemID id) { hiddenItems_.erase(id); }
  void        unhideAll() { hiddenItems_.clear(); }
  bool        toggleNodeFlagOfSelection(uint64_t flag); // set flag if none of the selected node has the flag, otherwise unset the flag, returns the previous flag
  bool        copyTo(Json&);
  bool        pasteFrom(Json const&);
  Node*       solySelectedNode() const;

  // zoom to selected or all if nothing was selected
  // easingOrder == 1 -> linear
  // easingOrder == 2 -> quad
  // easingOrder == 3 -> cubic
  // easingOrder == 4 -> expo
  void zoomToSelected(float time, bool doScale = true, int easingOrder = 3, Vec2 offset = {0, 0});
  // compare z-order, return -1 if lhs<rhs, 0 if lhs==rhs, 1 if lhs>rhs.
  int zCompare(GraphItem* lhs, GraphItem* rhs) const;

  void addState(InteractionStatePtr state)
  {
    auto key = String(state->name());
    assert(stateTypeMap_.find(key) == stateTypeMap_.end());
    stateTypeMap_[key] = state;
    states_.push_back(state);
    std::stable_sort(states_.begin(), states_.end(), [](auto a, auto b) {
      return a->priority() < b->priority();
    });
  }

  bool isActive(StringView const& name) const
  {
    if (auto itr = stateTypeMap_.find(name); itr != stateTypeMap_.end())
      return itr->second->active();
    return false;
  }

  InteractionStatePtr getState(StringView const& name) const
  {
    if (auto itr = stateTypeMap_.find(name); itr != stateTypeMap_.end())
      return itr->second;
    return nullptr;
  }

  template<class T>
  static
  std::enable_if_t<std::is_base_of_v<InteractionState, T>, void>
  registerInteraction()
  {
    stateFactories_.push_back({
      [](void* arg)->InteractionState*{return new T();},
      nullptr
    });
  }

  template<class T>
  static
  std::enable_if_t<std::is_base_of_v<InteractionState, T>, void>
  registerInteraction(void* initArg)
  {
    stateFactories_.push_back({
      [](void* arg)->InteractionState*{return new T(arg);},
      initArg
    });
  }

  template<class T>
  typename std::enable_if<std::is_base_of<NamedInteractionState<T>, T>::value, bool>::type
  isActive() const
  {
    return isActive(T::className);
  }

  template<class T>
  typename std::
    enable_if<std::is_base_of<NamedInteractionState<T>, T>::value, std::shared_ptr<T>>::type
    getState() const
  {
    if (auto ptr = getState(T::className)) {
      assert(ptr->name() == T::className);
      return std::static_pointer_cast<T>(ptr);
    }
    return nullptr;
  }

  void update(float dt) override;
  void draw() override;
  void onDocModified() override;
  void onGraphModified() override;
  void reset(WeakGraphPtr graph) override;

  void navigate(NavDirection direction);

  static void addCommands(class CommandManager*);
};

// }}} NetworkView & Interaction

// Inspector {{{
class InspectorView : public GraphView
{
protected:
  using WeakGraphItem = std::weak_ptr<GraphItem>;

  std::weak_ptr<GraphView> linkedView_;
  HashSet<ItemID>          inspectingItems_;
  bool                     lockOnItem_ = false;
  bool                     lockOnView_ = false;

  void removeExpiredItems();

public:
  InspectorView(NodeGraphEditor* editor);
  virtual ~InspectorView() = default;
  auto linkedView() const { return linkedView_.lock(); }
  bool lockOnItem() const { return lockOnItem_; }
  bool lockOnView() const { return lockOnView_; }
  auto const& inspectingItems() const { return inspectingItems_; }

  void onDocModified() override;
  void onGraphModified() override;
  Vec2 defaultSize() const override { return {400, 600}; }

  void setInspectingItems(HashSet<ItemID> const& ids);
  void linkToView(GraphView* view)
  {
    if (view)
      linkedView_ = view->weak_from_this();
    else
      linkedView_.reset();
  }
  void onViewEvent(GraphView* view, StringView eventType) override;
};
// }}} Inspector

// Responser {{{
// NodeGraphEditResponser acts as a hook on certain edit events
// the reason NOT to put `bool accept(GraphItem* item) const`
// or things like that in Graph class is,
// this hook can potentially do more than that -
// e.g. make a graph / view read only, giving visual feedbacks
class NodeGraphEditResponser
{
public:
  virtual ~NodeGraphEditResponser() {}

  // return false prevents item from being added
  virtual bool beforeItemAdded(Graph* graph, GraphItem* item, GraphItem** replacement) { return true; }
  virtual void afterItemAdded(Graph* graph, GraphItem* item) {}

  // return false prevents item from being removed
  virtual bool beforeItemRemoved(Graph* graph, GraphItem* item) { return true; }
  //virtual void afterItemRemoved(Graph* graph, GraphItem* item) {}

  // return false prevents node from being renamed
  virtual bool beforeNodeRenamed(Graph* graph, Node* node) { return true; }
  virtual void afterNodeRenamed(Graph* graph, Node* node) {}

  // return false prevents view from being removed
  virtual bool beforeViewRemoved(GraphView* view) { return true; }
  virtual void afterViewRemoved(GraphView* view) {}

  virtual void beforeViewUpdate(GraphView* view) {}
  virtual void afterViewUpdate(GraphView* view) {}
  virtual void beforeViewDraw(GraphView* view) {}
  virtual void afterViewDraw(GraphView* view) {}

  virtual void onItemAdded(GraphItem* item) {}
  virtual void onItemMoved(GraphItem* item) {}
  virtual void onItemModified(GraphItem* item) {}
  virtual void onItemRemoved(GraphItem* item) {}

  virtual void onInspect(InspectorView* view, GraphItem** items, size_t count) {}
  virtual void afterPaste(Graph* graph, GraphItem** new_items, size_t count) {}

  // button: 0:left, 1:right, 2:middle
  virtual void onItemClicked(NetworkView* view, GraphItem* item, int button) {}
  virtual void onItemDoubleClicked(NetworkView* view, GraphItem* item, int button) {}
  virtual void onItemHovered(NetworkView* view, GraphItem* item) {}
  virtual void onItemSelected(NetworkView* view, GraphItem* item) {}
  virtual void onItemDeselected(NetworkView* view, GraphItem* item) {}
  virtual void onSelectionChanged(NetworkView* view) {}

  // return false prevents the link from being set
  virtual bool beforeLinkSet(Graph* graph, InputConnection src, OutputConnection dst) { return true; }
  virtual void onLinkSet(Link* link) {}
  virtual void onLinkRemoved(Link* link) {}
};
using NodeGraphEditResponserPtr = std::shared_ptr<NodeGraphEditResponser>;
// }}} Responser

// Command & Command Manager {{{
enum class ModKey
{
  NONE  = 0,
  CTRL  = 1,
  SHIFT = 2,
  ALT   = 4,
  SUPER = 8,
};
struct Shortcut
{
  uint8_t key = 0;
  ModKey  mod = ModKey::NONE;

  static Shortcut parse(String shortcutStr);
  static String   describ(Shortcut shortcut);
  static bool     check(Shortcut const& shortcut);
};

class CommandManager
{
private:
  CommandManager(CommandManager const&) = delete;
  CommandManager(CommandManager&&)      = delete;

public:
  CommandManager();
  ~CommandManager() = default;

  class Command
  {
  private:
    String name_;
    String description_;
    String
      view_; // active only in matching view, can be regex; "*" or "" means every view (global)
    Shortcut shortcut_;
    bool     hiddenInMenu_  = false; // for shortcut-only commands - e.g. open palette itself
    bool     mayModifyGraph_ = true; // if the command may modify graph,
                                     // if false, the cammand will be available in read-only views
    Command(Command const&) = delete;

  public:
    Command(
      String   name,
      String   description,
      String   viewKind,
      Shortcut shortcut,
      bool     hiddenInMenu = false)
        : name_(std::move(name))
        , description_(std::move(description))
        , view_(std::move(viewKind))
        , shortcut_(shortcut)
        , hiddenInMenu_(hiddenInMenu)
    {
    }
    virtual ~Command() {}

    auto const& name() const { return name_; }
    auto const& description() const { return description_; }
    auto const& view() const { return view_; }
    auto const& shortcut() const { return shortcut_; }
    bool        hiddenInMenu() const { return hiddenInMenu_; }
    bool        mayModifyGraph() const { return mayModifyGraph_; }
    void        setMayModifyGraph(bool m) { mayModifyGraph_ = m; }

    virtual void onConfirm(GraphView* view) = 0;
    virtual bool hasPrompt() const { return false; }
    virtual void onOpenPrompt(GraphView* view) {}
    virtual bool onUpdatePrompt(GraphView* view)
    {
      return true;
    } // return true: still prompting, else: done.
    virtual void draw(NetworkView* view) {} // only network view will call this
  };
  using CommandPtr = std::shared_ptr<Command>;
  class SimpleCommand : public Command
  {
    String promptInput_ = "";
    String argPrompt_   = ""; // if not empty, a prompt should be opened
    void (*onConfirmCallback_)(GraphView* view, StringView args) = nullptr;
    String (*promptDefault_)(GraphView* view) = nullptr; // default value for prompt
  public:
    SimpleCommand(
      String name,
      String description,
      void (*onConfirmCallback)(GraphView* view, StringView arg),
      Shortcut shortcut                        = {},
      String   viewKind                        = "*",
      String   argumentPrompt                  = "",
      String (*promptDefault)(GraphView* view) = nullptr,
      bool hiddenInMenu                        = false)
        : Command(name, description, viewKind, shortcut, hiddenInMenu)
        , argPrompt_(std::move(argumentPrompt))
        , onConfirmCallback_(onConfirmCallback)
        , promptDefault_(promptDefault)
    {
    }

    void onConfirm(GraphView* view) override
    {
      if (onConfirmCallback_)
        onConfirmCallback_(view, promptInput_);
    }
    bool hasPrompt() const override { return !argPrompt_.empty(); }
    void onOpenPrompt(GraphView* view) override;   // implement in nged_imgui.cpp
    bool onUpdatePrompt(GraphView* view) override; // implement in nged_imgui.cpp
  };
  void checkShortcut(GraphView* view);
  void prompt(CommandPtr cmd, GraphView* view);
  void resetPrompt();
  void openPalette();
  void update(GraphView* view);
  void draw(NetworkView* view);

public:
  auto const& commands() const { return commands_; }
  Command&    add(Command*&& cmd) { commands_.emplace_back(cmd); return *commands_.back(); }
  Command&    add(CommandPtr cmd) { commands_.push_back(std::move(cmd)); return *commands_.back(); }
  bool        remove(String const& name)
  {
    return commands_.end() ==
           std::remove_if(commands_.begin(), commands_.end(), [&name](CommandPtr const& cmd) {
             return cmd->name() == name;
           });
  }

protected:
  Vector<CommandPtr> commands_;

  CommandPtr               prompting_;
  std::weak_ptr<GraphView> promptingView_;

  String paletteInput_;
};
// }}} Command & Command Manager

// Editor {{{
class NodeGraphEditor
{
public:
  using DocPtr  = std::shared_ptr<NodeGraphDoc>;
  using ViewPtr = std::shared_ptr<GraphView>;
  virtual ~NodeGraphEditor() { }

  struct ContextMenuEntry
  {
    std::function<bool(GraphView const*)> condition;
    std::function<void(GraphView*)> reaction;
    String text;
  };
  using ContextMenuEntries = Vector<ContextMenuEntry>;
  using ContextMenuEntriesPtr = std::shared_ptr<ContextMenuEntries>;

protected:
  HashSet<ViewPtr> views_;              // docs are held by views
  HashSet<ViewPtr> pendingAddViews_;    // newly added views, moved into `views_` on each update()
  HashSet<ViewPtr> pendingRemoveViews_; // views to be removed, also will be done at next update()
  ContextMenuEntriesPtr contextMenuEntries_;

  int                 dyingRefCount_ = 0; // reference count of a dying doc ptr
  String              fileExt_ = "ng";
  NodeFactoryPtr      nodeFactory_;
  GraphItemFactoryPtr itemFactory_;
  ViewFactoryPtr      viewFactory_;
  CommandManager      commandManager_;

  std::function<NodeGraphDocPtr(NodeFactoryPtr, GraphItemFactory const*)> docFactory_
    = [](NodeFactoryPtr nodeFactory, GraphItemFactory const* itemFactory) {
        return std::make_shared<NodeGraphDoc>(nodeFactory, itemFactory);
    };

  NodeGraphEditResponserPtr responser_;

  void removeView(ViewPtr view);

public:
  auto const& views() const { return views_; }

  void setFileExt(String ext) { fileExt_ = ext; }
  void setItemFactory(GraphItemFactoryPtr factory) { itemFactory_ = std::move(factory); }
  void setViewFactory(ViewFactoryPtr factory) { viewFactory_ = std::move(factory); }
  void setNodeFactory(NodeFactoryPtr factory) { nodeFactory_ = factory; }
  void setResponser(NodeGraphEditResponserPtr responser) { responser_ = responser; }
  template <class T>
  std::enable_if_t<std::is_base_of_v<NodeGraphDoc, T>, void>
  setDocType()
  {
    docFactory_ = [](NodeFactoryPtr nodeFactory, GraphItemFactory const* itemFactory)->NodeGraphDocPtr {
      return std::make_shared<T>(nodeFactory, itemFactory);
    };
  }
  void setDocFactory(std::function<NodeGraphDocPtr(NodeFactoryPtr, GraphItemFactory const*)> factory)
  {
    docFactory_ = std::move(factory);
  }
  void setContextMenus(ContextMenuEntriesPtr menus) { contextMenuEntries_ = menus; }

  auto  contextMenus() const { return contextMenuEntries_; }
  auto  itemFactory()       { return itemFactory_.get(); }
  auto  itemFactory() const { return itemFactory_.get(); }
  auto  viewFactory()       { return viewFactory_; }
  auto  viewFactory() const { return viewFactory_; }
  auto  nodeFactory()       { return nodeFactory_; }
  auto  nodeFactory() const { return nodeFactory_; }
  auto* responser() const { return responser_.get(); }
  auto& commandManager() { return commandManager_; }

  void notifyGraphModified(Graph* graph);
  void boardcastViewEvent(GraphView* view, StringView eventType);

  bool closeView(ViewPtr view, bool confirmIfNotSaved = true);
  bool agreeToQuit() const; // if no doc is dirty
  void switchMessageTab(StringView tab); // simple helper function to switch message view tab

  virtual void   initCommands();
  virtual DocPtr createNewDocAndDefaultViews(); // create a new doc, and assign default views to it
  virtual DocPtr openDoc(StringView path);      // open a doc, and assign default views to it
  virtual bool   loadDocInto(StringView path, DocPtr dest); // load into dest
  virtual bool   saveDoc(DocPtr doc); // save in place, ask for file path if missing
  virtual bool   saveDocAs(DocPtr     doc,
                           StringView path); // save to path, ask for path if it's empty
  virtual void    beforeDocRemoved(DocPtr doc) {} // called before doc was removed
  virtual ViewPtr addView(DocPtr doc, String const& kind); // add a view to existing doc
  virtual void    update(float dt);
  virtual void    draw()                                  = 0; // for each view : view->draw()
  virtual void    setClipboardText(StringView text) const = 0;
  virtual String  getClipboardText() const                = 0;

  // graph manipulation respecting responser {{{
  NodePtr createNode(Graph* graph, StringView type);
  ItemID  addItem(Graph* graph, GraphItemPtr item);
  void    confirmItemPlacements(Graph* graph, HashSet<ItemID> const& items);
  bool    moveItems(Graph* graph, HashSet<ItemID> const& items, Vec2 delta);
  void    removeItems(Graph* graph, HashSet<ItemID> const& items, HashSet<ItemID>* remainingItems=nullptr);
  bool    setLink(Graph* graph, NetworkView* fromView, ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort);
  void    removeLink(Graph* graph, ItemID destItem, sint destPort);
  void    swapInput(Graph* graph, ItemID oldSourceItem, sint oldSourcePort, ItemID newSourceItem, sint newSourcePort, ItemID destItem, sint destPort);
  void    swapOutput(Graph* graph, ItemID sourceItem, sint sourcePort, ItemID oldDestItem, sint oldDestPort, ItemID newDestItem, sint newDestPort);
  // }}} graph manipulation respecting responser
};

using EditorPtr = std::shared_ptr<NodeGraphEditor>;
// }}} Editor

// Common Interaction States {{{
class AnimationState : public NetworkView::NamedInteractionState<AnimationState>
{
  Vec2  viewPosStart_, viewPosDest_;
  float viewScaleStart_, viewScaleDest_;
  float duration_;
  float t_;
  int   easingOrder_ = 3;

public:
  static constexpr StringView className = "animation";
  int                         priority() const override { return 10; }

  bool shouldEnter(NetworkView const* view) const override { return true; }
  bool shouldExit(NetworkView const* view) const override { return false; }

  void setViewPos(Canvas* canvas, Vec2 pos);
  void setViewScale(Canvas* canvas, float scale);
  void animateToPos(Canvas* canvas, Vec2 pos, float duration = 0.2f, int order = 3);
  void animateToScale(Canvas* canvas, float scale, float duration = 0.2f, int order = 3);
  void animateTo(Canvas* canvas, Vec2 pos, float scale, float duration = 0.2f, int order = 3);

  void tick(NetworkView* view, float dt) override;
};
// }}} Common Interaction States

} // namespace nged
