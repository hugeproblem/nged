#pragma once
#include "nged.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace nged {

std::shared_ptr<NodeGraphEditor> newImGuiNodeGraphEditor();
GraphItemFactoryPtr              addImGuiItems(GraphItemFactoryPtr factory);
void                             addImGuiInteractions();

// convert proceduals {{{

namespace detail {

static inline ImVec2&       imvec(Vec2& v) { return *reinterpret_cast<ImVec2*>(&v); }
static inline ImVec2 const& imvec(Vec2 const& v) { return *reinterpret_cast<ImVec2 const*>(&v); }
static inline ImVec2 const* imvec(Vec2 const* v) { return reinterpret_cast<ImVec2 const*>(v); }
static inline Vec2&         vec(ImVec2& v) { return *reinterpret_cast<Vec2*>(&v); }
static inline Vec2 const&   vec(ImVec2 const& v) { return *reinterpret_cast<Vec2 const*>(&v); }
static inline Vec2 const*   vec(ImVec2 const* v) { return reinterpret_cast<Vec2 const*>(v); }

} // namespace detail }}}

// Shared Resource {{{
class ImGuiResource
{
  ImGuiResource()                     = default;
  ImGuiResource(ImGuiResource const&) = delete;
  static ImGuiResource instance_;

public:
  ImFont* sansSerifFont      = nullptr;
  ImFont* monoFont           = nullptr;
  ImFont* iconFont           = nullptr;
  ImFont* largeSansSerifFont = nullptr;
  ImFont* largeIconFont      = nullptr;

  static ImGuiResource const& instance() { return instance_; }
  static void                 reloadFonts();

  ImFont* getBestMatchingFont(Canvas::TextStyle const& style, float scale) const;
};
// }}} Shared Resource

// Comment Box Impl {{{
class ImGuiCommentBox : public CommentBox
{
public:
  ImGuiCommentBox(Graph* parent) : CommentBox(parent) {}

  void onInspect(GraphView* inspector);
};
// }}} Comment Box Impl

// ImGui GraphView {{{
class ImGuiNamedWindow
{
public:
  virtual ~ImGuiNamedWindow() {}
  virtual String titleWithId() const = 0;
};

template<class This, class Base = GraphView>
class ImGuiGraphView
    : public Base
    , public ImGuiNamedWindow
{
protected:
  float dt_       = 0.f;
  float dpiScale_ = 1.f;
  ImGuiWindowFlags windowFlags_ = 0;

public:
  template<class... T>
  ImGuiGraphView(T&&... arg) : Base(std::forward<T>(arg)...)
  {}

  virtual String titleWithId() const override
  {
    return Base::doc() ? fmt::format(
                           "{}: {}{}###{}[{}]",
                           Base::kind(),
                           Base::title(),
                           (Base::doc()->dirty() ? " *" : ""),
                           Base::kind(),
                           Base::id())
                       : fmt::format("{}##{}[{}]", Base::title(), Base::kind(), Base::id());
  }

  float dpiScale() const override { return dpiScale_; }

  void update(float dt) override { dt_ = dt; }
  void draw() override
  {
    if (bool open = Base::isOpen()) {
      auto title = titleWithId();
      if (ImGui::FindWindowByName(title.c_str()) == nullptr) {
        ImGui::SetNextWindowSize(detail::imvec(this->defaultSize()));
      }
      bool             hasMenu  = this->hasMenu();
      ImGuiWindowFlags winFlags = windowFlags_;
      if (hasMenu)
        winFlags |= ImGuiWindowFlags_MenuBar;
      bool const windowIsOpen = ImGui::Begin(title.c_str(), &open, winFlags);
      dpiScale_               = ImGui::GetWindowDpiScale();
      if (windowIsOpen) {
        if (hasMenu) {
          this->updateMenu();
          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
          ImGui::BeginChild("ContentArea");
        }
        bool const wasFocused = Base::isFocused();
        bool const isFocused  = ImGui::IsWindowFocused(
          ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);
        if constexpr (std::is_base_of_v<NetworkView, This>) {
          this->setCanvasIsFocused(ImGui::IsWindowFocused(
            ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_NoPopupHierarchy));
        }
        if (isFocused && !wasFocused) {
          Base::editor()->boardcastViewEvent(this, "focus");
        } else if (!isFocused && wasFocused) {
          Base::editor()->boardcastViewEvent(this, "lost-focus");
        }
        Base::setFocused(isFocused);
        Base::setHovered(ImGui::IsWindowHovered(
          ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy));
      } else {
        Base::setFocused(false);
        Base::setHovered(false);
      }
      if (windowIsOpen) {
        Base::update(dt_);
        static_cast<This*>(this)->drawContent();
        if (hasMenu) {
          ImGui::PopStyleVar();
          ImGui::EndChild();
        }
      }
      ImGui::End();
      Base::setOpen(open);
    } else {
      if (!Base::editor()->closeView(this->shared_from_this()))
        Base::setOpen(true);
    }
  }
};
// }}}

// Responser {{{
class DefaultImGuiResponser : public NodeGraphEditResponser
{
public:
  void onInspect(InspectorView* view, GraphItem** items, size_t count) override;
};
// }}} Responser

// ImGuiNodeGraphEditor {{{
class ImGuiNodeGraphEditor : public NodeGraphEditor
{
  std::vector<std::function<void()>> runOnceBeforeDraw_;

  String  defaultLayoutDesc_=R"(vsplit:
  hsplit:7
    network:5
    inspector:3:hide_tab_bar
  message:3:hide_tab_bar)";


  ImGuiID mainDockID_=0;

public:
  void   initCommands() override;
  void   draw() override;
  void   setClipboardText(StringView text) const override;
  String getClipboardText() const override;

/* 
 * Layout description example:
 *
hsplit:
  vsplit:7
    dataview:4:hide_tab_bar
    network:5
    inspector:3:hide_tab_bar
  message:3:hide_tab_bar

*/
  void   setDefaultLayoutDesc(String desc) { defaultLayoutDesc_ = std::move(desc); }
  auto const& defaultLayoutDesc() const { return defaultLayoutDesc_; }
  DocPtr createNewDocAndDefaultViews() override;
};
// }}}

// Detail {{{
namespace detail {

template<class T>
class TickJob : public NetworkView::NamedInteractionState<T>
{
public:
  bool shouldEnter(NetworkView const* view) const override { return true; }

  bool shouldExit(NetworkView const* view) const override { return false; }
};

class LinkState : public NetworkView::NamedInteractionState<LinkState>
{
  NodePin      srcPin_;
  NodePin      dstPin_;
  Vec2         pos_;
  ItemID       hiddenLink_;
  Vector<Vec2> outPath_;
  Vector<Vec2> inPath_;
  bool mutable pendingEnter_;
  ItemID mutable pendingLinkID_;
  bool manualActivated_;

public:
  static constexpr StringView className = "link";

  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override;

  bool shouldEnter(NetworkView const* view) const override
  {
    if (!view->isFocused())
      return false;
    if (view->readonly())
      return false;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      if (view->hoveringPin() != PIN_None)
        return true;
      if (view->hoveringItem() != ID_None && view->graph()->get(view->hoveringItem())->asLink()) {
        pendingEnter_  = true;
        pendingLinkID_ = view->hoveringItem();
      } else {
        pendingEnter_  = false;
        pendingLinkID_ = ID_None;
      }
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      pendingEnter_  = false;
      pendingLinkID_ = ID_None;
    }
    if (pendingEnter_)
      return ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    return false;
  }
  void onEnter(NetworkView* view) override;
  bool shouldExit(NetworkView const* view) const override
  {
    if (view->readonly()) return true;
    return ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  }
  void onExit(NetworkView* view) override;
  auto srcPin() const { return srcPin_; }
  auto dstPin() const { return dstPin_; }
  void clear()
  {
    srcPin_ = PIN_None;
    dstPin_ = PIN_None;
  }
  bool activate(NodePin source = PIN_None, NodePin dest = PIN_None);
};

class CutLinkState : public NetworkView::NamedInteractionState<CutLinkState>
{
  Vector<Vec2> stroke_;
  bool         done_ = false;

public:
  static constexpr StringView className = "cut-link";

  bool shouldEnter(NetworkView const* view) const override
  {
    if (view->readonly())
      return false;
    return view->isFocused() && ImGui::IsKeyDown(ImGuiKey_Y) && ImGui::GetIO().KeyMods == 0 &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  }
  bool shouldExit(NetworkView const* view) const override { return done_ || view->readonly(); }

  void onEnter(NetworkView* view) override;
  void onExit(NetworkView* view) override;

  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override;
};

class ResizeBoxState : public NetworkView::NamedInteractionState<ResizeBoxState>
{
  ItemID resizingItem_;
  AABB   resizingBox_;
  bool   resized_ = false;
  enum ResizingLocation
  {
    Nowhere,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    TopLeft
  } resizingWhere_;

public:
  static constexpr StringView className = "resize-box";

  bool shouldEnter(NetworkView const* view) const override;
  bool shouldExit(NetworkView const* view) const override;
  void onEnter(NetworkView* view) override;
  void onExit(NetworkView* view) override;

  void updateCursor(ResizingLocation location) const;
  void activate(GraphItemPtr item, ResizingLocation where);

  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override;
};

class CreateNodeState : public NetworkView::NamedInteractionState<CreateNodeState>
{
  struct MatchItem
  {
    enum
    {
      ITEM,
      NODE
    } kind;
    String type;
    String name;
  };

  String                           input_ = "";
  std::multimap<int, MatchItem,
                std::greater<int>> orderedMatches_; // edit distance -> string

  GraphItemPtr pendingItemToPlace_;
  ItemID       hiddenLink_;

  String confirmedNodeType_ = "";
  String confirmedItemType_ = "";
  bool   isConfirmed_       = false;
  bool   isPlaced_          = false;

  mutable InputConnection  pendingInputLink_;
  mutable OutputConnection pendingOutputLink_;
  mutable bool             manualActivated_ = false;

  // for sorting
  int tempCounter_ = 0;

public:
  static constexpr StringView className = "create-node";

  bool shouldEnter(NetworkView const* view) const override;
  bool shouldExit(NetworkView const* view) const override;
  void onEnter(NetworkView* view) override;
  void onExit(NetworkView* view) override;
  bool update(NetworkView* view) override;
  // void tick(NetworkView*, float dt) override;
  void draw(NetworkView*) override;

  auto const& input() const { return input_; }

  bool activate(ItemID src = ID_None)
  {
    if (active())
      return false;
    manualActivated_ = true;
    if (src != ID_None)
      pendingInputLink_ = {src, 0};
    else
      pendingInputLink_ = {ID_None, -1};
    return true;
  }
  bool activate(InputConnection in, OutputConnection out)
  {
    if (active())
      return false;
    manualActivated_   = true;
    pendingInputLink_  = in;
    pendingOutputLink_ = out;
    return true;
  }
};

class MoveState : public NetworkView::NamedInteractionState<MoveState>
{
  bool            done_            = false;
  bool            moved_           = false;
  bool            movedSinceEnter_ = false;
  Vec2            anchor_          = {0, 0};
  HashSet<ItemID> itemsToMove_     = {};

public:
  static constexpr StringView className = "move";
  int                         priority() const override { return 10; }

  bool shouldEnter(NetworkView const* view) const override;
  bool shouldExit(NetworkView const* view) const override { return done_; }

  void onEnter(NetworkView* view) override;
  void onExit(NetworkView* view) override;
  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override {}
};

class SelectionState : public NetworkView::NamedInteractionState<SelectionState>
{
  bool            mouseDown_              = false;
  bool            shiftDown_              = false;
  bool            ctrlDown_               = false;
  bool            isBoxSelecting_         = false;
  bool            isBoxDeselecting_       = false;
  Vec2            boxSelectionAnchor_     = {0, 0};
  Vec2            mousePos_               = {};
  HashSet<ItemID> confirmedItemSelection_ = {};
  HashSet<ItemID> selectedThisFrame_ =
    {}; // unconfirmed selection of items, will be confirmed at mouse releasing
  HashSet<ItemID> deselectedThisFrame_ =
    {}; // unconfirmed de-selection of items, will be confirmed at mouse releasing
  bool isUpdating_ = false;
  Vec2 viewSize_;

public:
  static constexpr StringView className = "selection";

  bool shouldEnter(NetworkView const* view) const override;
  bool shouldExit(NetworkView const* view) const override;

  void onEnter(NetworkView* view) override;
  void onExit(NetworkView* view) override;

  void tick(NetworkView* view, float dt) override { isUpdating_ = false; }
  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override;

  void onGraphModified(NetworkView* view) override;
};

class EditArrow : public NetworkView::NamedInteractionState<EditArrow>
{
  std::weak_ptr<Arrow> editingArrow_;
  enum class EditHandle : uint8_t
  {
    START_POINT,
    END_POINT,
    SEGMENT,
    NONE
  } editHandle_;
  Vec2 mousePos_;

public:
  static constexpr StringView className = "edit-arrow";

  int  priority() const override { return 2; }
  bool shouldEnter(NetworkView const* view) const override;
  bool shouldExit(NetworkView const* view) const override;
  void onEnter(NetworkView* view) override;
  void onExit(NetworkView* view) override;
  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override;
  void onGraphModified(NetworkView* view) override;
};

class HandleView : public TickJob<HandleView>
{
  bool panButtonDown_ = false;
  bool canPan_        = false;
  Vec2 mouseAnchor_   = {-1, -1};
  Vec2 viewAnchor_    = {-1, -1};

public:
  static constexpr StringView className = "view";
  int                         priority() const override { return 0; }

  bool update(NetworkView* view) override;
  void draw(NetworkView* view) override;
};

class HandleShortcut : public TickJob<HandleShortcut>
{
public:
  static constexpr StringView className = "shortcut";
  int                         priority() const override { return 100; }

  bool update(NetworkView* view) override;
};

} // namespace detail
// }}} Detail

} // namespace nged
