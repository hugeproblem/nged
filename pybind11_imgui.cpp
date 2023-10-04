
#include "pybind11_imgui.h"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace py = pybind11;

void bind_imgui_to_py(py::module& m)
{

  py::class_<ImVec2>(m, "ImVec2")
    .def(py::init<>())
    .def(py::init<float, float>())
    .def_readwrite("x", &ImVec2::x)
    .def_readwrite("y", &ImVec2::y);

  py::class_<ImVec4>(m, "ImVec4")
    .def(py::init<>())
    .def(py::init<float, float, float, float>())
    .def_readwrite("x", &ImVec4::x)
    .def_readwrite("y", &ImVec4::y)
    .def_readwrite("z", &ImVec4::z)
    .def_readwrite("w", &ImVec4::w);

  py::class_<ImGuiListClipper>(m, "ListClipper")
    .def(py::init<>())
    .def_readonly("DisplayStart", &ImGuiListClipper::DisplayStart, "First item to display, updated by each call to Step()")
    .def_readonly("DisplayEnd", &ImGuiListClipper::DisplayEnd, "End of items to display (exclusive)")
    .def("Begin", &ImGuiListClipper::Begin, py::arg("items_count"), py::arg("items_height") = -1.0f, "items_count: Use INT_MAX if you don't know how many items you have (in which case the cursor won't be advanced in the final step)\\nitems_height: Use -1.0f to be calculated automatically on first step. Otherwise pass in the distance between your items, typically GetTextLineHeightWithSpacing() or GetFrameHeightWithSpacing().")
    .def("End", &ImGuiListClipper::End, "Automatically called on the last call of Step() that returns false.")
    .def("Step", &ImGuiListClipper::Step, "Call until it returns false. The DisplayStart/DisplayEnd fields will be set and you can process/draw those items.");

  py::class_<ImGuiViewport>(m, "Viewport")
    .def_readonly("Flags", &ImGuiViewport::Flags)
    .def_readonly("Pos", &ImGuiViewport::Pos)
    .def_readonly("Size", &ImGuiViewport::Size)
    .def_readonly("WorkPos", &ImGuiViewport::WorkPos)
    .def_readonly("WorkSize", &ImGuiViewport::WorkSize);

  py::enum_<ImGuiWindowFlags_>(m, "WindowFlags", py::arithmetic())
    .value("NONE", ImGuiWindowFlags_None)
    .value("NoTitleBar", ImGuiWindowFlags_NoTitleBar, "Disable title-bar")
    .value("NoResize", ImGuiWindowFlags_NoResize, "Disable user resizing with the lower-right grip")
    .value("NoMove", ImGuiWindowFlags_NoMove, "Disable user moving the window")
    .value("NoScrollbar", ImGuiWindowFlags_NoScrollbar, "Disable scrollbars (window can still scroll with mouse or programmatically)")
    .value("NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse, "Disable user vertically scrolling with mouse wheel. On child window, mouse wheel will be forwarded to the parent unless NoScrollbar is also set.")
    .value("NoCollapse", ImGuiWindowFlags_NoCollapse, "Disable user collapsing window by double-clicking on it. Also referred to as Window Menu Button (e.g. within a docking node).")
    .value("AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize, "Resize every window to its content every frame")
    .value("NoBackground", ImGuiWindowFlags_NoBackground, "Disable drawing background color (WindowBg, etc.) and outside border. Similar as using SetNextWindowBgAlpha(0.0f).")
    .value("NoSavedSettings", ImGuiWindowFlags_NoSavedSettings, "Never load/save settings in .ini file")
    .value("NoMouseInputs", ImGuiWindowFlags_NoMouseInputs, "Disable catching mouse, hovering test with pass through.")
    .value("MenuBar", ImGuiWindowFlags_MenuBar, "Has a menu-bar")
    .value("HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar, "Allow horizontal scrollbar to appear (off by default). You may use SetNextWindowContentSize(ImVec2(width,0.0f)); prior to calling Begin() to specify width. Read code in imgui_demo in the \"Horizontal Scrolling\" section.")
    .value("NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing, "Disable taking focus when transitioning from hidden to visible state")
    .value("NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus, "Disable bringing window to front when taking focus (e.g. clicking on it or programmatically giving it focus)")
    .value("AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar, "Always show vertical scrollbar (even if ContentSize.y < Size.y)")
    .value("AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar, "Always show horizontal scrollbar (even if ContentSize.x < Size.x)")
    .value("AlwaysUseWindowPadding", ImGuiWindowFlags_AlwaysUseWindowPadding, "Ensure child windows without border uses style.WindowPadding (ignored by default for non-bordered child windows, because more convenient)")
    .value("NoNavInputs", ImGuiWindowFlags_NoNavInputs, "No gamepad/keyboard navigation within the window")
    .value("NoNavFocus", ImGuiWindowFlags_NoNavFocus, "No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)")
    .value("UnsavedDocument", ImGuiWindowFlags_UnsavedDocument, "Display a dot next to the title. When used in a tab/docking context, tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.")
    .value("NoDocking", ImGuiWindowFlags_NoDocking, "Disable docking of this window")
    .value("NoNav", ImGuiWindowFlags_NoNav)
    .value("NoDecoration", ImGuiWindowFlags_NoDecoration)
    .value("NoInputs", ImGuiWindowFlags_NoInputs)
    .value("NavFlattened", ImGuiWindowFlags_NavFlattened, "[BETA] On child window: allow gamepad/keyboard navigation to cross over parent border to this child or between sibling child windows.")
    .value("ChildWindow", ImGuiWindowFlags_ChildWindow, "Don't use! For internal use by BeginChild()")
    .value("Tooltip", ImGuiWindowFlags_Tooltip, "Don't use! For internal use by BeginTooltip()")
    .value("Popup", ImGuiWindowFlags_Popup, "Don't use! For internal use by BeginPopup()")
    .value("Modal", ImGuiWindowFlags_Modal, "Don't use! For internal use by BeginPopupModal()")
    .value("ChildMenu", ImGuiWindowFlags_ChildMenu, "Don't use! For internal use by BeginMenu()")
    .value("DockNodeHost", ImGuiWindowFlags_DockNodeHost, "Don't use! For internal use by Begin()/NewFrame()")
  ;

  py::enum_<ImGuiInputTextFlags_>(m, "InputTextFlags", py::arithmetic())
    .value("NONE", ImGuiInputTextFlags_None)
    .value("CharsDecimal", ImGuiInputTextFlags_CharsDecimal, "Allow 0123456789.+-*/")
    .value("CharsHexadecimal", ImGuiInputTextFlags_CharsHexadecimal, "Allow 0123456789ABCDEFabcdef")
    .value("CharsUppercase", ImGuiInputTextFlags_CharsUppercase, "Turn a..z into A..Z")
    .value("CharsNoBlank", ImGuiInputTextFlags_CharsNoBlank, "Filter out spaces, tabs")
    .value("AutoSelectAll", ImGuiInputTextFlags_AutoSelectAll, "Select entire text when first taking mouse focus")
    .value("EnterReturnsTrue", ImGuiInputTextFlags_EnterReturnsTrue, "Return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider looking at the IsItemDeactivatedAfterEdit() function.")
    .value("CallbackCompletion", ImGuiInputTextFlags_CallbackCompletion, "Callback on pressing TAB (for completion handling)")
    .value("CallbackHistory", ImGuiInputTextFlags_CallbackHistory, "Callback on pressing Up/Down arrows (for history handling)")
    .value("CallbackAlways", ImGuiInputTextFlags_CallbackAlways, "Callback on each iteration. User code may query cursor position, modify text buffer.")
    .value("CallbackCharFilter", ImGuiInputTextFlags_CallbackCharFilter, "Callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.")
    .value("AllowTabInput", ImGuiInputTextFlags_AllowTabInput, "Pressing TAB input a '\t' character into the text field")
    .value("CtrlEnterForNewLine", ImGuiInputTextFlags_CtrlEnterForNewLine, "In multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).")
    .value("NoHorizontalScroll", ImGuiInputTextFlags_NoHorizontalScroll, "Disable following the cursor horizontally")
    .value("AlwaysOverwrite", ImGuiInputTextFlags_AlwaysOverwrite, "Overwrite mode")
    .value("ReadOnly", ImGuiInputTextFlags_ReadOnly, "Read-only mode")
    .value("Password", ImGuiInputTextFlags_Password, "Password mode, display all characters as '*'")
    .value("NoUndoRedo", ImGuiInputTextFlags_NoUndoRedo, "Disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().")
    .value("CharsScientific", ImGuiInputTextFlags_CharsScientific, "Allow 0123456789.+-*/eE (Scientific notation input)")
    .value("CallbackResize", ImGuiInputTextFlags_CallbackResize, "Callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)")
    .value("CallbackEdit", ImGuiInputTextFlags_CallbackEdit, "Callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)")
    .value("EscapeClearsAll", ImGuiInputTextFlags_EscapeClearsAll, "Escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)")
  ;

  py::enum_<ImGuiTreeNodeFlags_>(m, "TreeNodeFlags", py::arithmetic())
    .value("NONE", ImGuiTreeNodeFlags_None)
    .value("Selected", ImGuiTreeNodeFlags_Selected, "Draw as selected")
    .value("Framed", ImGuiTreeNodeFlags_Framed, "Draw frame with background (e.g. for CollapsingHeader)")
    .value("AllowOverlap", ImGuiTreeNodeFlags_AllowOverlap, "Hit testing to allow subsequent widgets to overlap this one")
    .value("NoTreePushOnOpen", ImGuiTreeNodeFlags_NoTreePushOnOpen, "Don't do a TreePush() when open (e.g. for CollapsingHeader) = no extra indent nor pushing on ID stack")
    .value("NoAutoOpenOnLog", ImGuiTreeNodeFlags_NoAutoOpenOnLog, "Don't automatically and temporarily open node when Logging is active (by default logging will automatically open tree nodes)")
    .value("DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen, "Default node to be open")
    .value("OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick, "Need double-click to open node")
    .value("OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow, "Only open when clicking on the arrow part. If ImGuiTreeNodeFlags_OpenOnDoubleClick is also set, single-click arrow or double-click all box to open.")
    .value("Leaf", ImGuiTreeNodeFlags_Leaf, "No collapsing, no arrow (use as a convenience for leaf nodes).")
    .value("Bullet", ImGuiTreeNodeFlags_Bullet, "Display a bullet instead of arrow")
    .value("FramePadding", ImGuiTreeNodeFlags_FramePadding, "Use FramePadding (even for an unframed text node) to vertically align text baseline to regular widget height. Equivalent to calling AlignTextToFramePadding().")
    .value("SpanAvailWidth", ImGuiTreeNodeFlags_SpanAvailWidth, "Extend hit box to the right-most edge, even if not framed. This is not the default in order to allow adding other items on the same line. In the future we may refactor the hit system to be front-to-back, allowing natural overlaps and then this can become the default.")
    .value("SpanFullWidth", ImGuiTreeNodeFlags_SpanFullWidth, "Extend hit box to the left-most and right-most edges (bypass the indented area).")
    .value("NavLeftJumpsBackHere", ImGuiTreeNodeFlags_NavLeftJumpsBackHere, "(WIP) Nav: left direction may move to this TreeNode() from any of its child (items submitted between TreeNode and TreePop)")
    .value("CollapsingHeader", ImGuiTreeNodeFlags_CollapsingHeader)
    .value("AllowItemOverlap", ImGuiTreeNodeFlags_AllowItemOverlap, "Renamed in 1.89.7")
  ;

  py::enum_<ImGuiPopupFlags_>(m, "PopupFlags", py::arithmetic())
    .value("NONE", ImGuiPopupFlags_None)
    .value("MouseButtonLeft", ImGuiPopupFlags_MouseButtonLeft, "For BeginPopupContext*(): open on Left Mouse release. Guaranteed to always be == 0 (same as ImGuiMouseButton_Left)")
    .value("MouseButtonRight", ImGuiPopupFlags_MouseButtonRight, "For BeginPopupContext*(): open on Right Mouse release. Guaranteed to always be == 1 (same as ImGuiMouseButton_Right)")
    .value("MouseButtonMiddle", ImGuiPopupFlags_MouseButtonMiddle, "For BeginPopupContext*(): open on Middle Mouse release. Guaranteed to always be == 2 (same as ImGuiMouseButton_Middle)")
    .value("MouseButtonMask_", ImGuiPopupFlags_MouseButtonMask_)
    .value("MouseButtonDefault_", ImGuiPopupFlags_MouseButtonDefault_)
    .value("NoOpenOverExistingPopup", ImGuiPopupFlags_NoOpenOverExistingPopup, "For OpenPopup*(), BeginPopupContext*(): don't open if there's already a popup at the same level of the popup stack")
    .value("NoOpenOverItems", ImGuiPopupFlags_NoOpenOverItems, "For BeginPopupContextWindow(): don't return true when hovering items, only when hovering empty space")
    .value("AnyPopupId", ImGuiPopupFlags_AnyPopupId, "For IsPopupOpen(): ignore the ImGuiID parameter and test for any popup.")
    .value("AnyPopupLevel", ImGuiPopupFlags_AnyPopupLevel, "For IsPopupOpen(): search/test at any level of the popup stack (default test in the current level)")
    .value("AnyPopup", ImGuiPopupFlags_AnyPopup)
  ;

  py::enum_<ImGuiSelectableFlags_>(m, "SelectableFlags", py::arithmetic())
    .value("NONE", ImGuiSelectableFlags_None)
    .value("DontClosePopups", ImGuiSelectableFlags_DontClosePopups, "Clicking this doesn't close parent popup window")
    .value("SpanAllColumns", ImGuiSelectableFlags_SpanAllColumns, "Selectable frame can span all columns (text will still fit in current column)")
    .value("AllowDoubleClick", ImGuiSelectableFlags_AllowDoubleClick, "Generate press events on double clicks too")
    .value("Disabled", ImGuiSelectableFlags_Disabled, "Cannot be selected, display grayed out text")
    .value("AllowOverlap", ImGuiSelectableFlags_AllowOverlap, "(WIP) Hit testing to allow subsequent widgets to overlap this one")
    .value("AllowItemOverlap", ImGuiSelectableFlags_AllowItemOverlap, "Renamed in 1.89.7")
  ;

  py::enum_<ImGuiComboFlags_>(m, "ComboFlags", py::arithmetic())
    .value("NONE", ImGuiComboFlags_None)
    .value("PopupAlignLeft", ImGuiComboFlags_PopupAlignLeft, "Align the popup toward the left by default")
    .value("HeightSmall", ImGuiComboFlags_HeightSmall, "Max ~4 items visible. Tip: If you want your combo popup to be a specific size you can use SetNextWindowSizeConstraints() prior to calling BeginCombo()")
    .value("HeightRegular", ImGuiComboFlags_HeightRegular, "Max ~8 items visible (default)")
    .value("HeightLarge", ImGuiComboFlags_HeightLarge, "Max ~20 items visible")
    .value("HeightLargest", ImGuiComboFlags_HeightLargest, "As many fitting items as possible")
    .value("NoArrowButton", ImGuiComboFlags_NoArrowButton, "Display on the preview box without the square arrow button")
    .value("NoPreview", ImGuiComboFlags_NoPreview, "Display only a square arrow button")
    .value("HeightMask_", ImGuiComboFlags_HeightMask_)
  ;

  py::enum_<ImGuiTabBarFlags_>(m, "TabBarFlags", py::arithmetic())
    .value("NONE", ImGuiTabBarFlags_None)
    .value("Reorderable", ImGuiTabBarFlags_Reorderable, "Allow manually dragging tabs to re-order them + New tabs are appended at the end of list")
    .value("AutoSelectNewTabs", ImGuiTabBarFlags_AutoSelectNewTabs, "Automatically select new tabs when they appear")
    .value("TabListPopupButton", ImGuiTabBarFlags_TabListPopupButton, "Disable buttons to open the tab list popup")
    .value("NoCloseWithMiddleMouseButton", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton, "Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You can still repro this behavior on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.")
    .value("NoTabListScrollingButtons", ImGuiTabBarFlags_NoTabListScrollingButtons, "Disable scrolling buttons (apply when fitting policy is ImGuiTabBarFlags_FittingPolicyScroll)")
    .value("NoTooltip", ImGuiTabBarFlags_NoTooltip, "Disable tooltips when hovering a tab")
    .value("FittingPolicyResizeDown", ImGuiTabBarFlags_FittingPolicyResizeDown, "Resize tabs when they don't fit")
    .value("FittingPolicyScroll", ImGuiTabBarFlags_FittingPolicyScroll, "Add scroll buttons when tabs don't fit")
    .value("FittingPolicyMask_", ImGuiTabBarFlags_FittingPolicyMask_)
    .value("FittingPolicyDefault_", ImGuiTabBarFlags_FittingPolicyDefault_)
  ;

  py::enum_<ImGuiTabItemFlags_>(m, "TabItemFlags", py::arithmetic())
    .value("NONE", ImGuiTabItemFlags_None)
    .value("UnsavedDocument", ImGuiTabItemFlags_UnsavedDocument, "Display a dot next to the title + tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.")
    .value("SetSelected", ImGuiTabItemFlags_SetSelected, "Trigger flag to programmatically make the tab selected when calling BeginTabItem()")
    .value("NoCloseWithMiddleMouseButton", ImGuiTabItemFlags_NoCloseWithMiddleMouseButton, "Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You can still repro this behavior on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.")
    .value("NoPushId", ImGuiTabItemFlags_NoPushId, "Don't call PushID(tab->ID)/PopID() on BeginTabItem()/EndTabItem()")
    .value("NoTooltip", ImGuiTabItemFlags_NoTooltip, "Disable tooltip for the given tab")
    .value("NoReorder", ImGuiTabItemFlags_NoReorder, "Disable reordering this tab or having another tab cross over this tab")
    .value("Leading", ImGuiTabItemFlags_Leading, "Enforce the tab position to the left of the tab bar (after the tab list popup button)")
    .value("Trailing", ImGuiTabItemFlags_Trailing, "Enforce the tab position to the right of the tab bar (before the scrolling buttons)")
  ;

  py::enum_<ImGuiTableFlags_>(m, "TableFlags", py::arithmetic())
    .value("NONE", ImGuiTableFlags_None)
    .value("Resizable", ImGuiTableFlags_Resizable, "Enable resizing columns.")
    .value("Reorderable", ImGuiTableFlags_Reorderable, "Enable reordering columns in header row (need calling TableSetupColumn() + TableHeadersRow() to display headers)")
    .value("Hideable", ImGuiTableFlags_Hideable, "Enable hiding/disabling columns in context menu.")
    .value("Sortable", ImGuiTableFlags_Sortable, "Enable sorting. Call TableGetSortSpecs() to obtain sort specs. Also see ImGuiTableFlags_SortMulti and ImGuiTableFlags_SortTristate.")
    .value("NoSavedSettings", ImGuiTableFlags_NoSavedSettings, "Disable persisting columns order, width and sort settings in the .ini file.")
    .value("ContextMenuInBody", ImGuiTableFlags_ContextMenuInBody, "Right-click on columns body/contents will display table context menu. By default it is available in TableHeadersRow().")
    .value("RowBg", ImGuiTableFlags_RowBg, "Set each RowBg color with ImGuiCol_TableRowBg or ImGuiCol_TableRowBgAlt (equivalent of calling TableSetBgColor with ImGuiTableBgFlags_RowBg0 on each row manually)")
    .value("BordersInnerH", ImGuiTableFlags_BordersInnerH, "Draw horizontal borders between rows.")
    .value("BordersOuterH", ImGuiTableFlags_BordersOuterH, "Draw horizontal borders at the top and bottom.")
    .value("BordersInnerV", ImGuiTableFlags_BordersInnerV, "Draw vertical borders between columns.")
    .value("BordersOuterV", ImGuiTableFlags_BordersOuterV, "Draw vertical borders on the left and right sides.")
    .value("BordersH", ImGuiTableFlags_BordersH, "Draw horizontal borders.")
    .value("BordersV", ImGuiTableFlags_BordersV, "Draw vertical borders.")
    .value("BordersInner", ImGuiTableFlags_BordersInner, "Draw inner borders.")
    .value("BordersOuter", ImGuiTableFlags_BordersOuter, "Draw outer borders.")
    .value("Borders", ImGuiTableFlags_Borders, "Draw all borders.")
    .value("NoBordersInBody", ImGuiTableFlags_NoBordersInBody, "[ALPHA] Disable vertical borders in columns Body (borders will always appear in Headers). -> May move to style")
    .value("NoBordersInBodyUntilResize", ImGuiTableFlags_NoBordersInBodyUntilResize, "[ALPHA] Disable vertical borders in columns Body until hovered for resize (borders will always appear in Headers). -> May move to style")
    .value("SizingFixedFit", ImGuiTableFlags_SizingFixedFit, "Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching contents width.")
    .value("SizingFixedSame", ImGuiTableFlags_SizingFixedSame, "Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching the maximum contents width of all columns. Implicitly enable ImGuiTableFlags_NoKeepColumnsVisible.")
    .value("SizingStretchProp", ImGuiTableFlags_SizingStretchProp, "Columns default to _WidthStretch with default weights proportional to each columns contents widths.")
    .value("SizingStretchSame", ImGuiTableFlags_SizingStretchSame, "Columns default to _WidthStretch with default weights all equal, unless overridden by TableSetupColumn().")
    .value("NoHostExtendX", ImGuiTableFlags_NoHostExtendX, "Make outer width auto-fit to columns, overriding outer_size.x value. Only available when ScrollX/ScrollY are disabled and Stretch columns are not used.")
    .value("NoHostExtendY", ImGuiTableFlags_NoHostExtendY, "Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit). Only available when ScrollX/ScrollY are disabled. Data below the limit will be clipped and not visible.")
    .value("NoKeepColumnsVisible", ImGuiTableFlags_NoKeepColumnsVisible, "Disable keeping column always minimally visible when ScrollX is off and table gets too small. Not recommended if columns are resizable.")
    .value("PreciseWidths", ImGuiTableFlags_PreciseWidths, "Disable distributing remainder width to stretched columns (width allocation on a 100-wide table with 3 columns: Without this flag: 33,33,34. With this flag: 33,33,33). With larger number of columns, resizing will appear to be less smooth.")
    .value("NoClip", ImGuiTableFlags_NoClip, "Disable clipping rectangle for every individual columns (reduce draw command count, items will be able to overflow into other columns). Generally incompatible with TableSetupScrollFreeze().")
    .value("PadOuterX", ImGuiTableFlags_PadOuterX, "Default if BordersOuterV is on. Enable outermost padding. Generally desirable if you have headers.")
    .value("NoPadOuterX", ImGuiTableFlags_NoPadOuterX, "Default if BordersOuterV is off. Disable outermost padding.")
    .value("NoPadInnerX", ImGuiTableFlags_NoPadInnerX, "Disable inner padding between columns (double inner padding if BordersOuterV is on, single inner padding if BordersOuterV is off).")
    .value("ScrollX", ImGuiTableFlags_ScrollX, "Enable horizontal scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size. Changes default sizing policy. Because this creates a child window, ScrollY is currently generally recommended when using ScrollX.")
    .value("ScrollY", ImGuiTableFlags_ScrollY, "Enable vertical scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size.")
    .value("SortMulti", ImGuiTableFlags_SortMulti, "Hold shift when clicking headers to sort on multiple column. TableGetSortSpecs() may return specs where (SpecsCount > 1).")
    .value("SortTristate", ImGuiTableFlags_SortTristate, "Allow no sorting, disable default sorting. TableGetSortSpecs() may return specs where (SpecsCount == 0).")
    .value("SizingMask_", ImGuiTableFlags_SizingMask_)
  ;

  py::enum_<ImGuiTableColumnFlags_>(m, "TableColumnFlags", py::arithmetic())
    .value("NONE", ImGuiTableColumnFlags_None)
    .value("Disabled", ImGuiTableColumnFlags_Disabled, "Overriding/master disable flag: hide column, won't show in context menu (unlike calling TableSetColumnEnabled() which manipulates the user accessible state)")
    .value("DefaultHide", ImGuiTableColumnFlags_DefaultHide, "Default as a hidden/disabled column.")
    .value("DefaultSort", ImGuiTableColumnFlags_DefaultSort, "Default as a sorting column.")
    .value("WidthStretch", ImGuiTableColumnFlags_WidthStretch, "Column will stretch. Preferable with horizontal scrolling disabled (default if table sizing policy is _SizingStretchSame or _SizingStretchProp).")
    .value("WidthFixed", ImGuiTableColumnFlags_WidthFixed, "Column will not stretch. Preferable with horizontal scrolling enabled (default if table sizing policy is _SizingFixedFit and table is resizable).")
    .value("NoResize", ImGuiTableColumnFlags_NoResize, "Disable manual resizing.")
    .value("NoReorder", ImGuiTableColumnFlags_NoReorder, "Disable manual reordering this column, this will also prevent other columns from crossing over this column.")
    .value("NoHide", ImGuiTableColumnFlags_NoHide, "Disable ability to hide/disable this column.")
    .value("NoClip", ImGuiTableColumnFlags_NoClip, "Disable clipping for this column (all NoClip columns will render in a same draw command).")
    .value("NoSort", ImGuiTableColumnFlags_NoSort, "Disable ability to sort on this field (even if ImGuiTableFlags_Sortable is set on the table).")
    .value("NoSortAscending", ImGuiTableColumnFlags_NoSortAscending, "Disable ability to sort in the ascending direction.")
    .value("NoSortDescending", ImGuiTableColumnFlags_NoSortDescending, "Disable ability to sort in the descending direction.")
    .value("NoHeaderLabel", ImGuiTableColumnFlags_NoHeaderLabel, "TableHeadersRow() will not submit label for this column. Convenient for some small columns. Name will still appear in context menu.")
    .value("NoHeaderWidth", ImGuiTableColumnFlags_NoHeaderWidth, "Disable header text width contribution to automatic column width.")
    .value("PreferSortAscending", ImGuiTableColumnFlags_PreferSortAscending, "Make the initial sort direction Ascending when first sorting on this column (default).")
    .value("PreferSortDescending", ImGuiTableColumnFlags_PreferSortDescending, "Make the initial sort direction Descending when first sorting on this column.")
    .value("IndentEnable", ImGuiTableColumnFlags_IndentEnable, "Use current Indent value when entering cell (default for column 0).")
    .value("IndentDisable", ImGuiTableColumnFlags_IndentDisable, "Ignore current Indent value when entering cell (default for columns > 0). Indentation changes _within_ the cell will still be honored.")
    .value("IsEnabled", ImGuiTableColumnFlags_IsEnabled, "Status: is enabled == not hidden by user/api (referred to as \"Hide\" in _DefaultHide and _NoHide) flags.")
    .value("IsVisible", ImGuiTableColumnFlags_IsVisible, "Status: is visible == is enabled AND not clipped by scrolling.")
    .value("IsSorted", ImGuiTableColumnFlags_IsSorted, "Status: is currently part of the sort specs")
    .value("IsHovered", ImGuiTableColumnFlags_IsHovered, "Status: is hovered by mouse")
    .value("WidthMask_", ImGuiTableColumnFlags_WidthMask_)
    .value("IndentMask_", ImGuiTableColumnFlags_IndentMask_)
    .value("StatusMask_", ImGuiTableColumnFlags_StatusMask_)
    .value("NoDirectResize_", ImGuiTableColumnFlags_NoDirectResize_, "[Internal] Disable user resizing this column directly (it may however we resized indirectly from its left edge)")
  ;

  py::enum_<ImGuiTableRowFlags_>(m, "TableRowFlags", py::arithmetic())
    .value("NONE", ImGuiTableRowFlags_None)
    .value("Headers", ImGuiTableRowFlags_Headers, "Identify header row (set default background color + width of its contents accounted differently for auto column width)")
  ;

  py::enum_<ImGuiTableBgTarget_>(m, "TableBgTarget", py::arithmetic())
    .value("NONE", ImGuiTableBgTarget_None)
    .value("RowBg0", ImGuiTableBgTarget_RowBg0, "Set row background color 0 (generally used for background, automatically set when ImGuiTableFlags_RowBg is used)")
    .value("RowBg1", ImGuiTableBgTarget_RowBg1, "Set row background color 1 (generally used for selection marking)")
    .value("CellBg", ImGuiTableBgTarget_CellBg, "Set cell background color (top-most color)")
  ;

  py::enum_<ImGuiFocusedFlags_>(m, "FocusedFlags", py::arithmetic())
    .value("NONE", ImGuiFocusedFlags_None)
    .value("ChildWindows", ImGuiFocusedFlags_ChildWindows, "Return true if any children of the window is focused")
    .value("RootWindow", ImGuiFocusedFlags_RootWindow, "Test from root window (top most parent of the current hierarchy)")
    .value("AnyWindow", ImGuiFocusedFlags_AnyWindow, "Return true if any window is focused. Important: If you are trying to tell how to dispatch your low-level inputs, do NOT use this. Use 'io.WantCaptureMouse' instead! Please read the FAQ!")
    .value("NoPopupHierarchy", ImGuiFocusedFlags_NoPopupHierarchy, "Do not consider popup hierarchy (do not treat popup emitter as parent of popup) (when used with _ChildWindows or _RootWindow)")
    .value("DockHierarchy", ImGuiFocusedFlags_DockHierarchy, "Consider docking hierarchy (treat dockspace host as parent of docked window) (when used with _ChildWindows or _RootWindow)")
    .value("RootAndChildWindows", ImGuiFocusedFlags_RootAndChildWindows)
  ;

  py::enum_<ImGuiHoveredFlags_>(m, "HoveredFlags", py::arithmetic())
    .value("NONE", ImGuiHoveredFlags_None, "Return true if directly over the item/window, not obstructed by another window, not obstructed by an active popup or modal blocking inputs under them.")
    .value("ChildWindows", ImGuiHoveredFlags_ChildWindows, "IsWindowHovered() only: Return true if any children of the window is hovered")
    .value("RootWindow", ImGuiHoveredFlags_RootWindow, "IsWindowHovered() only: Test from root window (top most parent of the current hierarchy)")
    .value("AnyWindow", ImGuiHoveredFlags_AnyWindow, "IsWindowHovered() only: Return true if any window is hovered")
    .value("NoPopupHierarchy", ImGuiHoveredFlags_NoPopupHierarchy, "IsWindowHovered() only: Do not consider popup hierarchy (do not treat popup emitter as parent of popup) (when used with _ChildWindows or _RootWindow)")
    .value("DockHierarchy", ImGuiHoveredFlags_DockHierarchy, "IsWindowHovered() only: Consider docking hierarchy (treat dockspace host as parent of docked window) (when used with _ChildWindows or _RootWindow)")
    .value("AllowWhenBlockedByPopup", ImGuiHoveredFlags_AllowWhenBlockedByPopup, "Return true even if a popup window is normally blocking access to this item/window")
    .value("AllowWhenBlockedByActiveItem", ImGuiHoveredFlags_AllowWhenBlockedByActiveItem, "Return true even if an active item is blocking access to this item/window. Useful for Drag and Drop patterns.")
    .value("AllowWhenOverlappedByItem", ImGuiHoveredFlags_AllowWhenOverlappedByItem, "IsItemHovered() only: Return true even if the item uses AllowOverlap mode and is overlapped by another hoverable item.")
    .value("AllowWhenOverlappedByWindow", ImGuiHoveredFlags_AllowWhenOverlappedByWindow, "IsItemHovered() only: Return true even if the position is obstructed or overlapped by another window.")
    .value("AllowWhenDisabled", ImGuiHoveredFlags_AllowWhenDisabled, "IsItemHovered() only: Return true even if the item is disabled")
    .value("NoNavOverride", ImGuiHoveredFlags_NoNavOverride, "IsItemHovered() only: Disable using gamepad/keyboard navigation state when active, always query mouse")
    .value("AllowWhenOverlapped", ImGuiHoveredFlags_AllowWhenOverlapped)
    .value("RectOnly", ImGuiHoveredFlags_RectOnly)
    .value("RootAndChildWindows", ImGuiHoveredFlags_RootAndChildWindows)
    .value("ForTooltip", ImGuiHoveredFlags_ForTooltip, "Shortcut for standard flags when using IsItemHovered() + SetTooltip() sequence.")
    .value("Stationary", ImGuiHoveredFlags_Stationary, "Require mouse to be stationary for style.HoverStationaryDelay (~0.15 sec) _at least one time_. After this, can move on same item/window. Using the stationary test tends to reduces the need for a long delay.")
    .value("DelayNone", ImGuiHoveredFlags_DelayNone, "IsItemHovered() only: Return true immediately (default). As this is the default you generally ignore this.")
    .value("DelayShort", ImGuiHoveredFlags_DelayShort, "IsItemHovered() only: Return true after style.HoverDelayShort elapsed (~0.15 sec) (shared between items) + requires mouse to be stationary for style.HoverStationaryDelay (once per item).")
    .value("DelayNormal", ImGuiHoveredFlags_DelayNormal, "IsItemHovered() only: Return true after style.HoverDelayNormal elapsed (~0.40 sec) (shared between items) + requires mouse to be stationary for style.HoverStationaryDelay (once per item).")
    .value("NoSharedDelay", ImGuiHoveredFlags_NoSharedDelay, "IsItemHovered() only: Disable shared delay system where moving from one item to the next keeps the previous timer for a short time (standard for tooltips with long delays)")
  ;

  py::enum_<ImGuiDockNodeFlags_>(m, "DockNodeFlags", py::arithmetic())
    .value("NONE", ImGuiDockNodeFlags_None)
    .value("KeepAliveOnly", ImGuiDockNodeFlags_KeepAliveOnly, "Shared       // Don't display the dockspace node but keep it alive. Windows docked into this dockspace node won't be undocked.")
    .value("NoDockingInCentralNode", ImGuiDockNodeFlags_NoDockingInCentralNode, "Shared       // Disable docking inside the Central Node, which will be always kept empty.")
    .value("PassthruCentralNode", ImGuiDockNodeFlags_PassthruCentralNode, "Shared       // Enable passthru dockspace: 1) DockSpace() will render a ImGuiCol_WindowBg background covering everything excepted the Central Node when empty. Meaning the host window should probably use SetNextWindowBgAlpha(0.0f) prior to Begin() when using this. 2) When Central Node is empty: let inputs pass-through + won't display a DockingEmptyBg background. See demo for details.")
    .value("NoSplit", ImGuiDockNodeFlags_NoSplit, "Shared/Local // Disable splitting the node into smaller nodes. Useful e.g. when embedding dockspaces into a main root one (the root one may have splitting disabled to reduce confusion). Note: when turned off, existing splits will be preserved.")
    .value("NoResize", ImGuiDockNodeFlags_NoResize, "Shared/Local // Disable resizing node using the splitter/separators. Useful with programmatically setup dockspaces.")
    .value("AutoHideTabBar", ImGuiDockNodeFlags_AutoHideTabBar, "Shared/Local // Tab bar will automatically hide when there is a single window in the dock node.")
  ;

  py::enum_<ImGuiDragDropFlags_>(m, "DragDropFlags", py::arithmetic())
    .value("NONE", ImGuiDragDropFlags_None)
    .value("SourceNoPreviewTooltip", ImGuiDragDropFlags_SourceNoPreviewTooltip, "Disable preview tooltip. By default, a successful call to BeginDragDropSource opens a tooltip so you can display a preview or description of the source contents. This flag disables this behavior.")
    .value("SourceNoDisableHover", ImGuiDragDropFlags_SourceNoDisableHover, "By default, when dragging we clear data so that IsItemHovered() will return false, to avoid subsequent user code submitting tooltips. This flag disables this behavior so you can still call IsItemHovered() on the source item.")
    .value("SourceNoHoldToOpenOthers", ImGuiDragDropFlags_SourceNoHoldToOpenOthers, "Disable the behavior that allows to open tree nodes and collapsing header by holding over them while dragging a source item.")
    .value("SourceAllowNullID", ImGuiDragDropFlags_SourceAllowNullID, "Allow items such as Text(), Image() that have no unique identifier to be used as drag source, by manufacturing a temporary identifier based on their window-relative position. This is extremely unusual within the dear imgui ecosystem and so we made it explicit.")
    .value("SourceExtern", ImGuiDragDropFlags_SourceExtern, "External source (from outside of dear imgui), won't attempt to read current item/window info. Will always return true. Only one Extern source can be active simultaneously.")
    .value("SourceAutoExpirePayload", ImGuiDragDropFlags_SourceAutoExpirePayload, "Automatically expire the payload if the source cease to be submitted (otherwise payloads are persisting while being dragged)")
    .value("AcceptBeforeDelivery", ImGuiDragDropFlags_AcceptBeforeDelivery, "AcceptDragDropPayload() will returns true even before the mouse button is released. You can then call IsDelivery() to test if the payload needs to be delivered.")
    .value("AcceptNoDrawDefaultRect", ImGuiDragDropFlags_AcceptNoDrawDefaultRect, "Do not draw the default highlight rectangle when hovering over target.")
    .value("AcceptNoPreviewTooltip", ImGuiDragDropFlags_AcceptNoPreviewTooltip, "Request hiding the BeginDragDropSource tooltip from the BeginDragDropTarget site.")
    .value("AcceptPeekOnly", ImGuiDragDropFlags_AcceptPeekOnly, "For peeking ahead and inspecting the payload before delivery.")
  ;

  py::enum_<ImGuiDataType_>(m, "DataType", py::arithmetic())
    .value("S8", ImGuiDataType_S8, "signed char / char (with sensible compilers)")
    .value("U8", ImGuiDataType_U8, "unsigned char")
    .value("S16", ImGuiDataType_S16, "short")
    .value("U16", ImGuiDataType_U16, "unsigned short")
    .value("S32", ImGuiDataType_S32, "int")
    .value("U32", ImGuiDataType_U32, "unsigned int")
    .value("S64", ImGuiDataType_S64, "long long / __int64")
    .value("U64", ImGuiDataType_U64, "unsigned long long / unsigned __int64")
    .value("Float", ImGuiDataType_Float, "float")
    .value("Double", ImGuiDataType_Double, "double")
  ;

  py::enum_<ImGuiDir_>(m, "Dir", py::arithmetic())
    .value("NONE", ImGuiDir_None)
    .value("Left", ImGuiDir_Left)
    .value("Right", ImGuiDir_Right)
    .value("Up", ImGuiDir_Up)
    .value("Down", ImGuiDir_Down)
  ;

  py::enum_<ImGuiSortDirection_>(m, "SortDirection", py::arithmetic())
    .value("NONE", ImGuiSortDirection_None)
    .value("Ascending", ImGuiSortDirection_Ascending, "Ascending = 0->9, A->Z etc.")
    .value("Descending", ImGuiSortDirection_Descending, "Descending = 9->0, Z->A etc.")
  ;

  py::enum_<ImGuiConfigFlags_>(m, "ConfigFlags", py::arithmetic())
    .value("NONE", ImGuiConfigFlags_None)
    .value("NavEnableKeyboard", ImGuiConfigFlags_NavEnableKeyboard, "Master keyboard navigation enable flag. Enable full Tabbing + directional arrows + space/enter to activate.")
    .value("NavEnableGamepad", ImGuiConfigFlags_NavEnableGamepad, "Master gamepad navigation enable flag. Backend also needs to set ImGuiBackendFlags_HasGamepad.")
    .value("NavEnableSetMousePos", ImGuiConfigFlags_NavEnableSetMousePos, "Instruct navigation to move the mouse cursor. May be useful on TV/console systems where moving a virtual mouse is awkward. Will update io.MousePos and set io.WantSetMousePos=true. If enabled you MUST honor io.WantSetMousePos requests in your backend, otherwise ImGui will react as if the mouse is jumping around back and forth.")
    .value("NavNoCaptureKeyboard", ImGuiConfigFlags_NavNoCaptureKeyboard, "Instruct navigation to not set the io.WantCaptureKeyboard flag when io.NavActive is set.")
    .value("NoMouse", ImGuiConfigFlags_NoMouse, "Instruct imgui to clear mouse position/buttons in NewFrame(). This allows ignoring the mouse information set by the backend.")
    .value("NoMouseCursorChange", ImGuiConfigFlags_NoMouseCursorChange, "Instruct backend to not alter mouse cursor shape and visibility. Use if the backend cursor changes are interfering with yours and you don't want to use SetMouseCursor() to change mouse cursor. You may want to honor requests from imgui by reading GetMouseCursor() yourself instead.")
    .value("DockingEnable", ImGuiConfigFlags_DockingEnable, "Docking enable flags.")
    .value("ViewportsEnable", ImGuiConfigFlags_ViewportsEnable, "Viewport enable flags (require both ImGuiBackendFlags_PlatformHasViewports + ImGuiBackendFlags_RendererHasViewports set by the respective backends)")
    .value("DpiEnableScaleViewports", ImGuiConfigFlags_DpiEnableScaleViewports, "[BETA: Don't use] FIXME-DPI: Reposition and resize imgui windows when the DpiScale of a viewport changed (mostly useful for the main viewport hosting other window). Note that resizing the main window itself is up to your application.")
    .value("DpiEnableScaleFonts", ImGuiConfigFlags_DpiEnableScaleFonts, "[BETA: Don't use] FIXME-DPI: Request bitmap-scaled fonts to match DpiScale. This is a very low-quality workaround. The correct way to handle DPI is _currently_ to replace the atlas and/or fonts in the Platform_OnChangedViewport callback, but this is all early work in progress.")
    .value("IsSRGB", ImGuiConfigFlags_IsSRGB, "Application is SRGB-aware.")
    .value("IsTouchScreen", ImGuiConfigFlags_IsTouchScreen, "Application is using a touch screen instead of a mouse.")
  ;

  py::enum_<ImGuiBackendFlags_>(m, "BackendFlags", py::arithmetic())
    .value("NONE", ImGuiBackendFlags_None)
    .value("HasGamepad", ImGuiBackendFlags_HasGamepad, "Backend Platform supports gamepad and currently has one connected.")
    .value("HasMouseCursors", ImGuiBackendFlags_HasMouseCursors, "Backend Platform supports honoring GetMouseCursor() value to change the OS cursor shape.")
    .value("HasSetMousePos", ImGuiBackendFlags_HasSetMousePos, "Backend Platform supports io.WantSetMousePos requests to reposition the OS mouse position (only used if ImGuiConfigFlags_NavEnableSetMousePos is set).")
    .value("RendererHasVtxOffset", ImGuiBackendFlags_RendererHasVtxOffset, "Backend Renderer supports ImDrawCmd::VtxOffset. This enables output of large meshes (64K+ vertices) while still using 16-bit indices.")
    .value("PlatformHasViewports", ImGuiBackendFlags_PlatformHasViewports, "Backend Platform supports multiple viewports.")
    .value("HasMouseHoveredViewport", ImGuiBackendFlags_HasMouseHoveredViewport, "Backend Platform supports calling io.AddMouseViewportEvent() with the viewport under the mouse. IF POSSIBLE, ignore viewports with the ImGuiViewportFlags_NoInputs flag (Win32 backend, GLFW 3.30+ backend can do this, SDL backend cannot). If this cannot be done, Dear ImGui needs to use a flawed heuristic to find the viewport under.")
    .value("RendererHasViewports", ImGuiBackendFlags_RendererHasViewports, "Backend Renderer supports multiple viewports.")
  ;

  py::enum_<ImGuiCol_>(m, "Col", py::arithmetic())
    .value("Text", ImGuiCol_Text)
    .value("TextDisabled", ImGuiCol_TextDisabled)
    .value("WindowBg", ImGuiCol_WindowBg, "Background of normal windows")
    .value("ChildBg", ImGuiCol_ChildBg, "Background of child windows")
    .value("PopupBg", ImGuiCol_PopupBg, "Background of popups, menus, tooltips windows")
    .value("Border", ImGuiCol_Border)
    .value("BorderShadow", ImGuiCol_BorderShadow)
    .value("FrameBg", ImGuiCol_FrameBg, "Background of checkbox, radio button, plot, slider, text input")
    .value("FrameBgHovered", ImGuiCol_FrameBgHovered)
    .value("FrameBgActive", ImGuiCol_FrameBgActive)
    .value("TitleBg", ImGuiCol_TitleBg)
    .value("TitleBgActive", ImGuiCol_TitleBgActive)
    .value("TitleBgCollapsed", ImGuiCol_TitleBgCollapsed)
    .value("MenuBarBg", ImGuiCol_MenuBarBg)
    .value("ScrollbarBg", ImGuiCol_ScrollbarBg)
    .value("ScrollbarGrab", ImGuiCol_ScrollbarGrab)
    .value("ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered)
    .value("ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive)
    .value("CheckMark", ImGuiCol_CheckMark)
    .value("SliderGrab", ImGuiCol_SliderGrab)
    .value("SliderGrabActive", ImGuiCol_SliderGrabActive)
    .value("Button", ImGuiCol_Button)
    .value("ButtonHovered", ImGuiCol_ButtonHovered)
    .value("ButtonActive", ImGuiCol_ButtonActive)
    .value("Header", ImGuiCol_Header, "Header* colors are used for CollapsingHeader, TreeNode, Selectable, MenuItem")
    .value("HeaderHovered", ImGuiCol_HeaderHovered)
    .value("HeaderActive", ImGuiCol_HeaderActive)
    .value("Separator", ImGuiCol_Separator)
    .value("SeparatorHovered", ImGuiCol_SeparatorHovered)
    .value("SeparatorActive", ImGuiCol_SeparatorActive)
    .value("ResizeGrip", ImGuiCol_ResizeGrip, "Resize grip in lower-right and lower-left corners of windows.")
    .value("ResizeGripHovered", ImGuiCol_ResizeGripHovered)
    .value("ResizeGripActive", ImGuiCol_ResizeGripActive)
    .value("Tab", ImGuiCol_Tab, "TabItem in a TabBar")
    .value("TabHovered", ImGuiCol_TabHovered)
    .value("TabActive", ImGuiCol_TabActive)
    .value("TabUnfocused", ImGuiCol_TabUnfocused)
    .value("TabUnfocusedActive", ImGuiCol_TabUnfocusedActive)
    .value("DockingPreview", ImGuiCol_DockingPreview, "Preview overlay color when about to docking something")
    .value("DockingEmptyBg", ImGuiCol_DockingEmptyBg, "Background color for empty node (e.g. CentralNode with no window docked into it)")
    .value("PlotLines", ImGuiCol_PlotLines)
    .value("PlotLinesHovered", ImGuiCol_PlotLinesHovered)
    .value("PlotHistogram", ImGuiCol_PlotHistogram)
    .value("PlotHistogramHovered", ImGuiCol_PlotHistogramHovered)
    .value("TableHeaderBg", ImGuiCol_TableHeaderBg, "Table header background")
    .value("TableBorderStrong", ImGuiCol_TableBorderStrong, "Table outer and header borders (prefer using Alpha=1.0 here)")
    .value("TableBorderLight", ImGuiCol_TableBorderLight, "Table inner borders (prefer using Alpha=1.0 here)")
    .value("TableRowBg", ImGuiCol_TableRowBg, "Table row background (even rows)")
    .value("TableRowBgAlt", ImGuiCol_TableRowBgAlt, "Table row background (odd rows)")
    .value("TextSelectedBg", ImGuiCol_TextSelectedBg)
    .value("DragDropTarget", ImGuiCol_DragDropTarget, "Rectangle highlighting a drop target")
    .value("NavHighlight", ImGuiCol_NavHighlight, "Gamepad/keyboard: current highlighted item")
    .value("NavWindowingHighlight", ImGuiCol_NavWindowingHighlight, "Highlight window when using CTRL+TAB")
    .value("NavWindowingDimBg", ImGuiCol_NavWindowingDimBg, "Darken/colorize entire screen behind the CTRL+TAB window list, when active")
    .value("ModalWindowDimBg", ImGuiCol_ModalWindowDimBg, "Darken/colorize entire screen behind a modal window, when one is active")
  ;

  py::enum_<ImGuiStyleVar_>(m, "StyleVar", py::arithmetic())
    .value("Alpha", ImGuiStyleVar_Alpha, "float     Alpha")
    .value("DisabledAlpha", ImGuiStyleVar_DisabledAlpha, "float     DisabledAlpha")
    .value("WindowPadding", ImGuiStyleVar_WindowPadding, "ImVec2    WindowPadding")
    .value("WindowRounding", ImGuiStyleVar_WindowRounding, "float     WindowRounding")
    .value("WindowBorderSize", ImGuiStyleVar_WindowBorderSize, "float     WindowBorderSize")
    .value("WindowMinSize", ImGuiStyleVar_WindowMinSize, "ImVec2    WindowMinSize")
    .value("WindowTitleAlign", ImGuiStyleVar_WindowTitleAlign, "ImVec2    WindowTitleAlign")
    .value("ChildRounding", ImGuiStyleVar_ChildRounding, "float     ChildRounding")
    .value("ChildBorderSize", ImGuiStyleVar_ChildBorderSize, "float     ChildBorderSize")
    .value("PopupRounding", ImGuiStyleVar_PopupRounding, "float     PopupRounding")
    .value("PopupBorderSize", ImGuiStyleVar_PopupBorderSize, "float     PopupBorderSize")
    .value("FramePadding", ImGuiStyleVar_FramePadding, "ImVec2    FramePadding")
    .value("FrameRounding", ImGuiStyleVar_FrameRounding, "float     FrameRounding")
    .value("FrameBorderSize", ImGuiStyleVar_FrameBorderSize, "float     FrameBorderSize")
    .value("ItemSpacing", ImGuiStyleVar_ItemSpacing, "ImVec2    ItemSpacing")
    .value("ItemInnerSpacing", ImGuiStyleVar_ItemInnerSpacing, "ImVec2    ItemInnerSpacing")
    .value("IndentSpacing", ImGuiStyleVar_IndentSpacing, "float     IndentSpacing")
    .value("CellPadding", ImGuiStyleVar_CellPadding, "ImVec2    CellPadding")
    .value("ScrollbarSize", ImGuiStyleVar_ScrollbarSize, "float     ScrollbarSize")
    .value("ScrollbarRounding", ImGuiStyleVar_ScrollbarRounding, "float     ScrollbarRounding")
    .value("GrabMinSize", ImGuiStyleVar_GrabMinSize, "float     GrabMinSize")
    .value("GrabRounding", ImGuiStyleVar_GrabRounding, "float     GrabRounding")
    .value("TabRounding", ImGuiStyleVar_TabRounding, "float     TabRounding")
    .value("ButtonTextAlign", ImGuiStyleVar_ButtonTextAlign, "ImVec2    ButtonTextAlign")
    .value("SelectableTextAlign", ImGuiStyleVar_SelectableTextAlign, "ImVec2    SelectableTextAlign")
    .value("SeparatorTextBorderSize", ImGuiStyleVar_SeparatorTextBorderSize, "float  SeparatorTextBorderSize")
    .value("SeparatorTextAlign", ImGuiStyleVar_SeparatorTextAlign, "ImVec2    SeparatorTextAlign")
    .value("SeparatorTextPadding", ImGuiStyleVar_SeparatorTextPadding, "ImVec2    SeparatorTextPadding")
    .value("DockingSeparatorSize", ImGuiStyleVar_DockingSeparatorSize, "float     DockingSeparatorSize")
  ;

  py::enum_<ImGuiButtonFlags_>(m, "ButtonFlags", py::arithmetic())
    .value("NONE", ImGuiButtonFlags_None)
    .value("MouseButtonLeft", ImGuiButtonFlags_MouseButtonLeft, "React on left mouse button (default)")
    .value("MouseButtonRight", ImGuiButtonFlags_MouseButtonRight, "React on right mouse button")
    .value("MouseButtonMiddle", ImGuiButtonFlags_MouseButtonMiddle, "React on center mouse button")
    .value("MouseButtonMask_", ImGuiButtonFlags_MouseButtonMask_)
    .value("MouseButtonDefault_", ImGuiButtonFlags_MouseButtonDefault_)
  ;

  py::enum_<ImGuiColorEditFlags_>(m, "ColorEditFlags", py::arithmetic())
    .value("NONE", ImGuiColorEditFlags_None)
    .value("NoAlpha", ImGuiColorEditFlags_NoAlpha, "// ColorEdit, ColorPicker, ColorButton: ignore Alpha component (will only read 3 components from the input pointer).")
    .value("NoPicker", ImGuiColorEditFlags_NoPicker, "// ColorEdit: disable picker when clicking on color square.")
    .value("NoOptions", ImGuiColorEditFlags_NoOptions, "// ColorEdit: disable toggling options menu when right-clicking on inputs/small preview.")
    .value("NoSmallPreview", ImGuiColorEditFlags_NoSmallPreview, "// ColorEdit, ColorPicker: disable color square preview next to the inputs. (e.g. to show only the inputs)")
    .value("NoInputs", ImGuiColorEditFlags_NoInputs, "// ColorEdit, ColorPicker: disable inputs sliders/text widgets (e.g. to show only the small preview color square).")
    .value("NoTooltip", ImGuiColorEditFlags_NoTooltip, "// ColorEdit, ColorPicker, ColorButton: disable tooltip when hovering the preview.")
    .value("NoLabel", ImGuiColorEditFlags_NoLabel, "// ColorEdit, ColorPicker: disable display of inline text label (the label is still forwarded to the tooltip and picker).")
    .value("NoSidePreview", ImGuiColorEditFlags_NoSidePreview, "// ColorPicker: disable bigger color preview on right side of the picker, use small color square preview instead.")
    .value("NoDragDrop", ImGuiColorEditFlags_NoDragDrop, "// ColorEdit: disable drag and drop target. ColorButton: disable drag and drop source.")
    .value("NoBorder", ImGuiColorEditFlags_NoBorder, "// ColorButton: disable border (which is enforced by default)")
    .value("AlphaBar", ImGuiColorEditFlags_AlphaBar, "// ColorEdit, ColorPicker: show vertical alpha bar/gradient in picker.")
    .value("AlphaPreview", ImGuiColorEditFlags_AlphaPreview, "// ColorEdit, ColorPicker, ColorButton: display preview as a transparent color over a checkerboard, instead of opaque.")
    .value("AlphaPreviewHalf", ImGuiColorEditFlags_AlphaPreviewHalf, "// ColorEdit, ColorPicker, ColorButton: display half opaque / half checkerboard, instead of opaque.")
    .value("HDR", ImGuiColorEditFlags_HDR, "// (WIP) ColorEdit: Currently only disable 0.0f..1.0f limits in RGBA edition (note: you probably want to use ImGuiColorEditFlags_Float flag as well).")
    .value("DisplayRGB", ImGuiColorEditFlags_DisplayRGB, "[Display]    // ColorEdit: override _display_ type among RGB/HSV/Hex. ColorPicker: select any combination using one or more of RGB/HSV/Hex.")
    .value("DisplayHSV", ImGuiColorEditFlags_DisplayHSV, "[Display]    // \"")
    .value("DisplayHex", ImGuiColorEditFlags_DisplayHex, "[Display]    // \"")
    .value("Uint8", ImGuiColorEditFlags_Uint8, "[DataType]   // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0..255.")
    .value("Float", ImGuiColorEditFlags_Float, "[DataType]   // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0.0f..1.0f floats instead of 0..255 integers. No round-trip of value via integers.")
    .value("PickerHueBar", ImGuiColorEditFlags_PickerHueBar, "[Picker]     // ColorPicker: bar for Hue, rectangle for Sat/Value.")
    .value("PickerHueWheel", ImGuiColorEditFlags_PickerHueWheel, "[Picker]     // ColorPicker: wheel for Hue, triangle for Sat/Value.")
    .value("InputRGB", ImGuiColorEditFlags_InputRGB, "[Input]      // ColorEdit, ColorPicker: input and output data in RGB format.")
    .value("InputHSV", ImGuiColorEditFlags_InputHSV, "[Input]      // ColorEdit, ColorPicker: input and output data in HSV format.")
    .value("DefaultOptions_", ImGuiColorEditFlags_DefaultOptions_)
    .value("DisplayMask_", ImGuiColorEditFlags_DisplayMask_)
    .value("DataTypeMask_", ImGuiColorEditFlags_DataTypeMask_)
    .value("PickerMask_", ImGuiColorEditFlags_PickerMask_)
    .value("InputMask_", ImGuiColorEditFlags_InputMask_)
  ;

  py::enum_<ImGuiSliderFlags_>(m, "SliderFlags", py::arithmetic())
    .value("NONE", ImGuiSliderFlags_None)
    .value("AlwaysClamp", ImGuiSliderFlags_AlwaysClamp, "Clamp value to min/max bounds when input manually with CTRL+Click. By default CTRL+Click allows going out of bounds.")
    .value("Logarithmic", ImGuiSliderFlags_Logarithmic, "Make the widget logarithmic (linear otherwise). Consider using ImGuiSliderFlags_NoRoundToFormat with this if using a format-string with small amount of digits.")
    .value("NoRoundToFormat", ImGuiSliderFlags_NoRoundToFormat, "Disable rounding underlying value to match precision of the display format string (e.g. %.3f values are rounded to those 3 digits)")
    .value("NoInput", ImGuiSliderFlags_NoInput, "Disable CTRL+Click or Enter key allowing to input text directly into the widget")
    .value("InvalidMask_", ImGuiSliderFlags_InvalidMask_, "[Internal] We treat using those bits as being potentially a 'float power' argument from the previous API that has got miscast to this enum, and will trigger an assert if needed.")
  ;

  py::enum_<ImGuiMouseButton_>(m, "MouseButton", py::arithmetic())
    .value("Left", ImGuiMouseButton_Left)
    .value("Right", ImGuiMouseButton_Right)
    .value("Middle", ImGuiMouseButton_Middle)
  ;

  py::enum_<ImGuiMouseCursor_>(m, "MouseCursor", py::arithmetic())
    .value("NONE", ImGuiMouseCursor_None)
    .value("Arrow", ImGuiMouseCursor_Arrow)
    .value("TextInput", ImGuiMouseCursor_TextInput, "When hovering over InputText, etc.")
    .value("ResizeAll", ImGuiMouseCursor_ResizeAll, "(Unused by Dear ImGui functions)")
    .value("ResizeNS", ImGuiMouseCursor_ResizeNS, "When hovering over a horizontal border")
    .value("ResizeEW", ImGuiMouseCursor_ResizeEW, "When hovering over a vertical border or a column")
    .value("ResizeNESW", ImGuiMouseCursor_ResizeNESW, "When hovering over the bottom-left corner of a window")
    .value("ResizeNWSE", ImGuiMouseCursor_ResizeNWSE, "When hovering over the bottom-right corner of a window")
    .value("Hand", ImGuiMouseCursor_Hand, "(Unused by Dear ImGui functions. Use for e.g. hyperlinks)")
    .value("NotAllowed", ImGuiMouseCursor_NotAllowed, "When hovering something with disallowed interaction. Usually a crossed circle.")
  ;

  py::enum_<ImGuiCond_>(m, "Cond", py::arithmetic())
    .value("NONE", ImGuiCond_None, "No condition (always set the variable), same as _Always")
    .value("Always", ImGuiCond_Always, "No condition (always set the variable), same as _None")
    .value("Once", ImGuiCond_Once, "Set the variable once per runtime session (only the first call will succeed)")
    .value("FirstUseEver", ImGuiCond_FirstUseEver, "Set the variable if the object/window has no persistently saved data (no entry in .ini file)")
    .value("Appearing", ImGuiCond_Appearing, "Set the variable if the object/window is appearing after being hidden/inactive (or the first time)")
  ;

  py::enum_<ImGuiViewportFlags_>(m, "ViewportFlags", py::arithmetic())
    .value("NONE", ImGuiViewportFlags_None)
    .value("IsPlatformWindow", ImGuiViewportFlags_IsPlatformWindow, "Represent a Platform Window")
    .value("IsPlatformMonitor", ImGuiViewportFlags_IsPlatformMonitor, "Represent a Platform Monitor (unused yet)")
    .value("OwnedByApp", ImGuiViewportFlags_OwnedByApp, "Platform Window: Was created/managed by the user application? (rather than our backend)")
    .value("NoDecoration", ImGuiViewportFlags_NoDecoration, "Platform Window: Disable platform decorations: title bar, borders, etc. (generally set all windows, but if ImGuiConfigFlags_ViewportsDecoration is set we only set this on popups/tooltips)")
    .value("NoTaskBarIcon", ImGuiViewportFlags_NoTaskBarIcon, "Platform Window: Disable platform task bar icon (generally set on popups/tooltips, or all windows if ImGuiConfigFlags_ViewportsNoTaskBarIcon is set)")
    .value("NoFocusOnAppearing", ImGuiViewportFlags_NoFocusOnAppearing, "Platform Window: Don't take focus when created.")
    .value("NoFocusOnClick", ImGuiViewportFlags_NoFocusOnClick, "Platform Window: Don't take focus when clicked on.")
    .value("NoInputs", ImGuiViewportFlags_NoInputs, "Platform Window: Make mouse pass through so we can drag this window while peaking behind it.")
    .value("NoRendererClear", ImGuiViewportFlags_NoRendererClear, "Platform Window: Renderer doesn't need to clear the framebuffer ahead (because we will fill it entirely).")
    .value("NoAutoMerge", ImGuiViewportFlags_NoAutoMerge, "Platform Window: Avoid merging this window into another host window. This can only be set via ImGuiWindowClass viewport flags override (because we need to now ahead if we are going to create a viewport in the first place!).")
    .value("TopMost", ImGuiViewportFlags_TopMost, "Platform Window: Display on top (for tooltips only).")
    .value("CanHostOtherWindows", ImGuiViewportFlags_CanHostOtherWindows, "Viewport can host multiple imgui windows (secondary viewports are associated to a single window). // FIXME: In practice there's still probably code making the assumption that this is always and only on the MainViewport. Will fix once we add support for \"no main viewport\".")
    .value("IsMinimized", ImGuiViewportFlags_IsMinimized, "Platform Window: Window is minimized, can skip render. When minimized we tend to avoid using the viewport pos/size for clipping window or testing if they are contained in the viewport.")
    .value("IsFocused", ImGuiViewportFlags_IsFocused, "Platform Window: Window is focused (last call to Platform_GetWindowFocus() returned true)")
  ;

  m.def("End", &ImGui::End);
  m.def("BeginChild", py::overload_cast<ImGuiID, const ImVec2&, bool, ImGuiWindowFlags>(&ImGui::BeginChild), py::arg("id"), py::arg("size") = ImVec2(0, 0), py::arg("border") = false, py::arg("flags") = 0);
  m.def("BeginChild", py::overload_cast<const char*, const ImVec2&, bool, ImGuiWindowFlags>(&ImGui::BeginChild), py::arg("str_id"), py::arg("size") = ImVec2(0, 0), py::arg("border") = false, py::arg("flags") = 0);
  m.def("EndChild", &ImGui::EndChild);
  m.def("IsWindowAppearing", &ImGui::IsWindowAppearing);
  m.def("IsWindowCollapsed", &ImGui::IsWindowCollapsed);
  m.def("IsWindowFocused", &ImGui::IsWindowFocused, py::arg("flags") = 0, "is current window focused? or its root/child, depending on flags. see flags for options.");
  m.def("IsWindowHovered", &ImGui::IsWindowHovered, py::arg("flags") = 0, "is current window hovered (and typically: not blocked by a popup/modal)? see flags for options. NB: If you are trying to check whether your mouse should be dispatched to imgui or to your app, you should use the 'io.WantCaptureMouse' boolean for that! Please read the FAQ!");
  m.def("GetWindowPos", &ImGui::GetWindowPos, "get current window position in screen space (useful if you want to do your own drawing via the DrawList API)");
  m.def("GetWindowSize", &ImGui::GetWindowSize, "get current window size");
  m.def("GetWindowWidth", &ImGui::GetWindowWidth, "get current window width (shortcut for GetWindowSize().x)");
  m.def("GetWindowHeight", &ImGui::GetWindowHeight, "get current window height (shortcut for GetWindowSize().y)");
  m.def("SetNextWindowPos", &ImGui::SetNextWindowPos, py::arg("pos"), py::arg("cond") = 0, py::arg("pivot") = ImVec2(0, 0), "set next window position. call before Begin(). use pivot=(0.5f,0.5f) to center on given point, etc.");
  m.def("SetNextWindowSize", &ImGui::SetNextWindowSize, py::arg("size"), py::arg("cond") = 0, "set next window size. set axis to 0.0f to force an auto-fit on this axis. call before Begin()");
  m.def("SetNextWindowContentSize", &ImGui::SetNextWindowContentSize, py::arg("size"), "set next window content size (~ scrollable client area, which enforce the range of scrollbars). Not including window decorations (title bar, menu bar, etc.) nor WindowPadding. set an axis to 0.0f to leave it automatic. call before Begin()");
  m.def("SetNextWindowCollapsed", &ImGui::SetNextWindowCollapsed, py::arg("collapsed"), py::arg("cond") = 0, "set next window collapsed state. call before Begin()");
  m.def("SetNextWindowFocus", &ImGui::SetNextWindowFocus, "set next window to be focused / top-most. call before Begin()");
  m.def("SetNextWindowBgAlpha", &ImGui::SetNextWindowBgAlpha, py::arg("alpha"), "set next window background color alpha. helper to easily override the Alpha component of ImGuiCol_WindowBg/ChildBg/PopupBg. you may also use ImGuiWindowFlags_NoBackground.");
  m.def("SetWindowPos", py::overload_cast<const ImVec2&, ImGuiCond>(&ImGui::SetWindowPos), py::arg("pos"), py::arg("cond") = 0, "(not recommended) set current window position - call within Begin()/End(). prefer using SetNextWindowPos(), as this may incur tearing and side-effects.");
  m.def("SetWindowPos", py::overload_cast<const char*, const ImVec2&, ImGuiCond>(&ImGui::SetWindowPos), py::arg("name"), py::arg("pos"), py::arg("cond") = 0, "set named window position.");
  m.def("SetWindowSize", py::overload_cast<const char*, const ImVec2&, ImGuiCond>(&ImGui::SetWindowSize), py::arg("name"), py::arg("size"), py::arg("cond") = 0, "set named window size. set axis to 0.0f to force an auto-fit on this axis.");
  m.def("SetWindowSize", py::overload_cast<const ImVec2&, ImGuiCond>(&ImGui::SetWindowSize), py::arg("size"), py::arg("cond") = 0, "(not recommended) set current window size - call within Begin()/End(). set to ImVec2(0, 0) to force an auto-fit. prefer using SetNextWindowSize(), as this may incur tearing and minor side-effects.");
  m.def("SetWindowCollapsed", py::overload_cast<const char*, bool, ImGuiCond>(&ImGui::SetWindowCollapsed), py::arg("name"), py::arg("collapsed"), py::arg("cond") = 0, "set named window collapsed state");
  m.def("SetWindowCollapsed", py::overload_cast<bool, ImGuiCond>(&ImGui::SetWindowCollapsed), py::arg("collapsed"), py::arg("cond") = 0, "(not recommended) set current window collapsed state. prefer using SetNextWindowCollapsed().");
  m.def("SetWindowFocus", py::overload_cast<const char*>(&ImGui::SetWindowFocus), py::arg("name"), "set named window to be focused / top-most. use NULL to remove focus.");
  m.def("SetWindowFocus", py::overload_cast<>(&ImGui::SetWindowFocus), "(not recommended) set current window to be focused / top-most. prefer using SetNextWindowFocus().");
  m.def("SetWindowFontScale", &ImGui::SetWindowFontScale, py::arg("scale"), "[OBSOLETE] set font scale. Adjust IO.FontGlobalScale if you want to scale all windows. This is an old API! For correct scaling, prefer to reload font + rebuild ImFontAtlas + call style.ScaleAllSizes().");
  m.def("GetContentRegionAvail", &ImGui::GetContentRegionAvail, "== GetContentRegionMax() - GetCursorPos()");
  m.def("GetContentRegionMax", &ImGui::GetContentRegionMax, "current content boundaries (typically window boundaries including scrolling, or current column boundaries), in windows coordinates");
  m.def("GetWindowContentRegionMin", &ImGui::GetWindowContentRegionMin, "content boundaries min for the full window (roughly (0,0)-Scroll), in window coordinates");
  m.def("GetWindowContentRegionMax", &ImGui::GetWindowContentRegionMax, "content boundaries max for the full window (roughly (0,0)+Size-Scroll) where Size can be overridden with SetNextWindowContentSize(), in window coordinates");
  m.def("GetScrollX", &ImGui::GetScrollX, "get scrolling amount [0 .. GetScrollMaxX()]");
  m.def("GetScrollY", &ImGui::GetScrollY, "get scrolling amount [0 .. GetScrollMaxY()]");
  m.def("SetScrollX", &ImGui::SetScrollX, py::arg("scroll_x"), "set scrolling amount [0 .. GetScrollMaxX()]");
  m.def("SetScrollY", &ImGui::SetScrollY, py::arg("scroll_y"), "set scrolling amount [0 .. GetScrollMaxY()]");
  m.def("GetScrollMaxX", &ImGui::GetScrollMaxX, "get maximum scrolling amount ~~ ContentSize.x - WindowSize.x - DecorationsSize.x");
  m.def("GetScrollMaxY", &ImGui::GetScrollMaxY, "get maximum scrolling amount ~~ ContentSize.y - WindowSize.y - DecorationsSize.y");
  m.def("SetScrollHereX", &ImGui::SetScrollHereX, py::arg("center_x_ratio") = 0.5f, "adjust scrolling amount to make current cursor position visible. center_x_ratio=0.0: left, 0.5: center, 1.0: right. When using to make a \"default/current item\" visible, consider using SetItemDefaultFocus() instead.");
  m.def("SetScrollHereY", &ImGui::SetScrollHereY, py::arg("center_y_ratio") = 0.5f, "adjust scrolling amount to make current cursor position visible. center_y_ratio=0.0: top, 0.5: center, 1.0: bottom. When using to make a \"default/current item\" visible, consider using SetItemDefaultFocus() instead.");
  m.def("SetScrollFromPosX", &ImGui::SetScrollFromPosX, py::arg("local_x"), py::arg("center_x_ratio") = 0.5f, "adjust scrolling amount to make given position visible. Generally GetCursorStartPos() + offset to compute a valid position.");
  m.def("SetScrollFromPosY", &ImGui::SetScrollFromPosY, py::arg("local_y"), py::arg("center_y_ratio") = 0.5f, "adjust scrolling amount to make given position visible. Generally GetCursorStartPos() + offset to compute a valid position.");
  m.def("PushStyleColor", py::overload_cast<ImGuiCol, const ImVec4&>(&ImGui::PushStyleColor), py::arg("idx"), py::arg("col"));
  m.def("PushStyleColor", py::overload_cast<ImGuiCol, ImU32>(&ImGui::PushStyleColor), py::arg("idx"), py::arg("col"), "modify a style color. always use this if you modify the style after NewFrame().");
  m.def("PopStyleColor", &ImGui::PopStyleColor, py::arg("count") = 1);
  m.def("PushStyleVar", py::overload_cast<ImGuiStyleVar, const ImVec2&>(&ImGui::PushStyleVar), py::arg("idx"), py::arg("val"), "modify a style ImVec2 variable. always use this if you modify the style after NewFrame().");
  m.def("PushStyleVar", py::overload_cast<ImGuiStyleVar, float>(&ImGui::PushStyleVar), py::arg("idx"), py::arg("val"), "modify a style float variable. always use this if you modify the style after NewFrame().");
  m.def("PopStyleVar", &ImGui::PopStyleVar, py::arg("count") = 1);
  m.def("PushTabStop", &ImGui::PushTabStop, py::arg("tab_stop"), "== tab stop enable. Allow focusing using TAB/Shift-TAB, enabled by default but you can disable it for certain widgets");
  m.def("PopTabStop", &ImGui::PopTabStop);
  m.def("PushButtonRepeat", &ImGui::PushButtonRepeat, py::arg("repeat"), "in 'repeat' mode, Button*() functions return repeated true in a typematic manner (using io.KeyRepeatDelay/io.KeyRepeatRate setting). Note that you can call IsItemActive() after any Button() to tell if the button is held in the current frame.");
  m.def("PopButtonRepeat", &ImGui::PopButtonRepeat);
  m.def("PushItemWidth", &ImGui::PushItemWidth, py::arg("item_width"), "push width of items for common large \"item+label\" widgets. >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -FLT_MIN always align width to the right side).");
  m.def("PopItemWidth", &ImGui::PopItemWidth);
  m.def("SetNextItemWidth", &ImGui::SetNextItemWidth, py::arg("item_width"), "set width of the _next_ common large \"item+label\" widget. >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -FLT_MIN always align width to the right side)");
  m.def("CalcItemWidth", &ImGui::CalcItemWidth, "width of item given pushed settings and current cursor position. NOT necessarily the width of last item unlike most 'Item' functions.");
  m.def("PushTextWrapPos", &ImGui::PushTextWrapPos, py::arg("wrap_local_pos_x") = 0.0f, "push word-wrapping position for Text*() commands. < 0.0f: no wrapping; 0.0f: wrap to end of window (or column); > 0.0f: wrap at 'wrap_pos_x' position in window local space");
  m.def("PopTextWrapPos", &ImGui::PopTextWrapPos);
  m.def("Separator", &ImGui::Separator, "separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.");
  m.def("SameLine", &ImGui::SameLine, py::arg("offset_from_start_x") = 0.0f, py::arg("spacing") = -1.0f, "call between widgets or groups to layout them horizontally. X position given in window coordinates.");
  m.def("NewLine", &ImGui::NewLine, "undo a SameLine() or force a new line when in a horizontal-layout context.");
  m.def("Spacing", &ImGui::Spacing, "add vertical spacing.");
  m.def("Dummy", &ImGui::Dummy, py::arg("size"), "add a dummy item of given size. unlike InvisibleButton(), Dummy() won't take the mouse click or be navigable into.");
  m.def("Indent", &ImGui::Indent, py::arg("indent_w") = 0.0f, "move content position toward the right, by indent_w, or style.IndentSpacing if indent_w <= 0");
  m.def("Unindent", &ImGui::Unindent, py::arg("indent_w") = 0.0f, "move content position back to the left, by indent_w, or style.IndentSpacing if indent_w <= 0");
  m.def("BeginGroup", &ImGui::BeginGroup, "lock horizontal starting position");
  m.def("EndGroup", &ImGui::EndGroup, "unlock horizontal starting position + capture the whole group bounding box into one \"item\" (so you can use IsItemHovered() or layout primitives such as SameLine() on whole group, etc.)");
  m.def("GetCursorPos", &ImGui::GetCursorPos, "cursor position in window coordinates (relative to window position)");
  m.def("GetCursorPosX", &ImGui::GetCursorPosX, "(some functions are using window-relative coordinates, such as: GetCursorPos, GetCursorStartPos, GetContentRegionMax, GetWindowContentRegion* etc.");
  m.def("GetCursorPosY", &ImGui::GetCursorPosY, "other functions such as GetCursorScreenPos or everything in ImDrawList::");
  m.def("SetCursorPos", &ImGui::SetCursorPos, py::arg("local_pos"), "are using the main, absolute coordinate system.");
  m.def("SetCursorPosX", &ImGui::SetCursorPosX, py::arg("local_x"), "GetWindowPos() + GetCursorPos() == GetCursorScreenPos() etc.)");
  m.def("SetCursorPosY", &ImGui::SetCursorPosY, py::arg("local_y"));
  m.def("GetCursorStartPos", &ImGui::GetCursorStartPos, "initial cursor position in window coordinates");
  m.def("GetCursorScreenPos", &ImGui::GetCursorScreenPos, "cursor position in absolute coordinates (useful to work with ImDrawList API). generally top-left == GetMainViewport()->Pos == (0,0) in single viewport mode, and bottom-right == GetMainViewport()->Pos+Size == io.DisplaySize in single-viewport mode.");
  m.def("SetCursorScreenPos", &ImGui::SetCursorScreenPos, py::arg("pos"), "cursor position in absolute coordinates");
  m.def("AlignTextToFramePadding", &ImGui::AlignTextToFramePadding, "vertically align upcoming text baseline to FramePadding.y so that it will align properly to regularly framed items (call if you have text on a line before a framed item)");
  m.def("GetTextLineHeight", &ImGui::GetTextLineHeight, "~ FontSize");
  m.def("GetTextLineHeightWithSpacing", &ImGui::GetTextLineHeightWithSpacing, "~ FontSize + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of text)");
  m.def("GetFrameHeight", &ImGui::GetFrameHeight, "~ FontSize + style.FramePadding.y * 2");
  m.def("GetFrameHeightWithSpacing", &ImGui::GetFrameHeightWithSpacing, "~ FontSize + style.FramePadding.y * 2 + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of framed widgets)");
  m.def("PopID", &ImGui::PopID, "pop from the ID stack.");
  m.def("Button", &ImGui::Button, py::arg("label"), py::arg("size") = ImVec2(0, 0), "button");
  m.def("SmallButton", &ImGui::SmallButton, py::arg("label"), "button with FramePadding=(0,0) to easily embed within text");
  m.def("InvisibleButton", &ImGui::InvisibleButton, py::arg("str_id"), py::arg("size"), py::arg("flags") = 0, "flexible button behavior without the visuals, frequently useful to build custom behaviors using the public api (along with IsItemActive, IsItemHovered, etc.)");
  m.def("ArrowButton", &ImGui::ArrowButton, py::arg("str_id"), py::arg("dir"), "square button with an arrow shape");
  m.def("RadioButton", py::overload_cast<const char*, bool>(&ImGui::RadioButton), py::arg("label"), py::arg("active"), "use with e.g. if (RadioButton(\"one\", my_value==1)) { my_value = 1; }");
  m.def("ProgressBar", &ImGui::ProgressBar, py::arg("fraction"), py::arg("size_arg") = ImVec2(-FLT_MIN, 0), py::arg("overlay") = NULL);
  m.def("Bullet", &ImGui::Bullet, "draw a small circle + keep the cursor on the same line. advance cursor x position by GetTreeNodeToLabelSpacing(), same distance that TreeNode() uses");
  m.def("BeginCombo", &ImGui::BeginCombo, py::arg("label"), py::arg("preview_value"), py::arg("flags") = 0);
  m.def("EndCombo", &ImGui::EndCombo, "only call EndCombo() if BeginCombo() returns true!");
  m.def("BeginListBox", &ImGui::BeginListBox, py::arg("label"), py::arg("size") = ImVec2(0, 0), "open a framed scrolling region");
  m.def("EndListBox", &ImGui::EndListBox, "only call EndListBox() if BeginListBox() returned true!");
  m.def("TreeNode", py::overload_cast<const char*>(&ImGui::TreeNode), py::arg("label"));
  m.def("TreePush", py::overload_cast<const char*>(&ImGui::TreePush), py::arg("str_id"), "~ Indent()+PushId(). Already called by TreeNode() when returning true, but you can call TreePush/TreePop yourself if desired.");
  m.def("TreePop", &ImGui::TreePop, "~ Unindent()+PopId()");
  m.def("GetTreeNodeToLabelSpacing", &ImGui::GetTreeNodeToLabelSpacing, "horizontal distance preceding label when using TreeNode*() or Bullet() == (g.FontSize + style.FramePadding.x*2) for a regular unframed TreeNode");
  m.def("CollapsingHeader", py::overload_cast<const char*, ImGuiTreeNodeFlags>(&ImGui::CollapsingHeader), py::arg("label"), py::arg("flags") = 0, "if returning 'true' the header is open. doesn't indent nor push on ID stack. user doesn't have to call TreePop().");
  m.def("SetNextItemOpen", &ImGui::SetNextItemOpen, py::arg("is_open"), py::arg("cond") = 0, "set next TreeNode/CollapsingHeader open state.");
  m.def("Selectable", py::overload_cast<const char*, bool, ImGuiSelectableFlags, const ImVec2&>(&ImGui::Selectable), py::arg("label"), py::arg("selected") = false, py::arg("flags") = 0, py::arg("size") = ImVec2(0, 0), "\"bool selected\" carry the selection state (read-only). Selectable() is clicked is returns true so you can modify your selection state. size.x==0.0: use remaining width, size.x>0.0: specify width. size.y==0.0: use label height, size.y>0.0: specify height");
  m.def("GetMainViewport", &ImGui::GetMainViewport, "return primary/default viewport. This can never be NULL.");
  m.def("BeginMenuBar", &ImGui::BeginMenuBar, "append to menu-bar of current window (requires ImGuiWindowFlags_MenuBar flag set on parent window).");
  m.def("EndMenuBar", &ImGui::EndMenuBar, "only call EndMenuBar() if BeginMenuBar() returns true!");
  m.def("BeginMainMenuBar", &ImGui::BeginMainMenuBar, "create and append to a full screen menu-bar.");
  m.def("EndMainMenuBar", &ImGui::EndMainMenuBar, "only call EndMainMenuBar() if BeginMainMenuBar() returns true!");
  m.def("BeginMenu", &ImGui::BeginMenu, py::arg("label"), py::arg("enabled") = true, "create a sub-menu entry. only call EndMenu() if this returns true!");
  m.def("EndMenu", &ImGui::EndMenu, "only call EndMenu() if BeginMenu() returns true!");
  m.def("MenuItem", py::overload_cast<const char*, const char*, bool, bool>(&ImGui::MenuItem), py::arg("label"), py::arg("shortcut") = NULL, py::arg("selected") = false, py::arg("enabled") = true, "return true when activated.");
  m.def("BeginTooltip", &ImGui::BeginTooltip, "begin/append a tooltip window.");
  m.def("EndTooltip", &ImGui::EndTooltip, "only call EndTooltip() if BeginTooltip()/BeginItemTooltip() returns true!");
  m.def("BeginItemTooltip", &ImGui::BeginItemTooltip, "begin/append a tooltip window if preceding item was hovered.");
  m.def("BeginPopup", &ImGui::BeginPopup, py::arg("str_id"), py::arg("flags") = 0, "return true if the popup is open, and you can start outputting to it.");
  m.def("EndPopup", &ImGui::EndPopup, "only call EndPopup() if BeginPopupXXX() returns true!");
  m.def("OpenPopup", py::overload_cast<ImGuiID, ImGuiPopupFlags>(&ImGui::OpenPopup), py::arg("id"), py::arg("popup_flags") = 0, "id overload to facilitate calling from nested stacks");
  m.def("OpenPopup", py::overload_cast<const char*, ImGuiPopupFlags>(&ImGui::OpenPopup), py::arg("str_id"), py::arg("popup_flags") = 0, "call to mark popup as open (don't call every frame!).");
  m.def("OpenPopupOnItemClick", &ImGui::OpenPopupOnItemClick, py::arg("str_id") = NULL, py::arg("popup_flags") = 1, "helper to open popup when clicked on last item. Default to ImGuiPopupFlags_MouseButtonRight == 1. (note: actually triggers on the mouse _released_ event to be consistent with popup behaviors)");
  m.def("CloseCurrentPopup", &ImGui::CloseCurrentPopup, "manually close the popup we have begin-ed into.");
  m.def("BeginPopupContextItem", &ImGui::BeginPopupContextItem, py::arg("str_id") = NULL, py::arg("popup_flags") = 1, "open+begin popup when clicked on last item. Use str_id==NULL to associate the popup to previous item. If you want to use that on a non-interactive item such as Text() you need to pass in an explicit ID here. read comments in .cpp!");
  m.def("BeginPopupContextWindow", &ImGui::BeginPopupContextWindow, py::arg("str_id") = NULL, py::arg("popup_flags") = 1, "open+begin popup when clicked on current window.");
  m.def("BeginPopupContextVoid", &ImGui::BeginPopupContextVoid, py::arg("str_id") = NULL, py::arg("popup_flags") = 1, "open+begin popup when clicked in void (where there are no windows).");
  m.def("IsPopupOpen", &ImGui::IsPopupOpen, py::arg("str_id"), py::arg("flags") = 0, "return true if the popup is open.");
  m.def("BeginTable", &ImGui::BeginTable, py::arg("str_id"), py::arg("column"), py::arg("flags") = 0, py::arg("outer_size") = ImVec2(0.0f, 0.0f), py::arg("inner_width") = 0.0f);
  m.def("EndTable", &ImGui::EndTable, "only call EndTable() if BeginTable() returns true!");
  m.def("TableNextRow", &ImGui::TableNextRow, py::arg("row_flags") = 0, py::arg("min_row_height") = 0.0f, "append into the first cell of a new row.");
  m.def("TableNextColumn", &ImGui::TableNextColumn, "append into the next column (or first column of next row if currently in last column). Return true when column is visible.");
  m.def("TableSetColumnIndex", &ImGui::TableSetColumnIndex, py::arg("column_n"), "append into the specified column. Return true when column is visible.");
  m.def("TableSetupColumn", &ImGui::TableSetupColumn, py::arg("label"), py::arg("flags") = 0, py::arg("init_width_or_weight") = 0.0f, py::arg("user_id") = 0);
  m.def("TableSetupScrollFreeze", &ImGui::TableSetupScrollFreeze, py::arg("cols"), py::arg("rows"), "lock columns/rows so they stay visible when scrolled.");
  m.def("TableHeadersRow", &ImGui::TableHeadersRow, "submit all headers cells based on data provided to TableSetupColumn() + submit context menu");
  m.def("TableHeader", &ImGui::TableHeader, py::arg("label"), "submit one header cell manually (rarely used)");
  m.def("TableGetColumnCount", &ImGui::TableGetColumnCount, "return number of columns (value passed to BeginTable)");
  m.def("TableGetColumnIndex", &ImGui::TableGetColumnIndex, "return current column index.");
  m.def("TableGetRowIndex", &ImGui::TableGetRowIndex, "return current row index.");
  m.def("TableGetColumnName", &ImGui::TableGetColumnName, py::arg("column_n") = -1, "return \"\" if column didn't have a name declared by TableSetupColumn(). Pass -1 to use current column.");
  m.def("TableGetColumnFlags", &ImGui::TableGetColumnFlags, py::arg("column_n") = -1, "return column flags so you can query their Enabled/Visible/Sorted/Hovered status flags. Pass -1 to use current column.");
  m.def("TableSetColumnEnabled", &ImGui::TableSetColumnEnabled, py::arg("column_n"), py::arg("v"), "change user accessible enabled/disabled state of a column. Set to false to hide the column. User can use the context menu to change this themselves (right-click in headers, or right-click in columns body with ImGuiTableFlags_ContextMenuInBody)");
  m.def("TableSetBgColor", &ImGui::TableSetBgColor, py::arg("target"), py::arg("color"), py::arg("column_n") = -1, "change the color of a cell, row, or column. See ImGuiTableBgTarget_ flags for details.");
  m.def("BeginTabBar", &ImGui::BeginTabBar, py::arg("str_id"), py::arg("flags") = 0, "create and append into a TabBar");
  m.def("EndTabBar", &ImGui::EndTabBar, "only call EndTabBar() if BeginTabBar() returns true!");
  m.def("EndTabItem", &ImGui::EndTabItem, "only call EndTabItem() if BeginTabItem() returns true!");
  m.def("TabItemButton", &ImGui::TabItemButton, py::arg("label"), py::arg("flags") = 0, "create a Tab behaving like a button. return true when clicked. cannot be selected in the tab bar.");
  m.def("SetTabItemClosed", &ImGui::SetTabItemClosed, py::arg("tab_or_docked_window_label"), "notify TabBar or Docking system of a closed tab/window ahead (useful to reduce visual flicker on reorderable tab bars). For tab-bar: call after BeginTabBar() and before Tab submissions. Otherwise call with a window name.");
  m.def("BeginDisabled", &ImGui::BeginDisabled, py::arg("disabled") = true);
  m.def("EndDisabled", &ImGui::EndDisabled);
  m.def("SetItemDefaultFocus", &ImGui::SetItemDefaultFocus, "make last item the default focused item of a window.");
  m.def("SetKeyboardFocusHere", &ImGui::SetKeyboardFocusHere, py::arg("offset") = 0, "focus keyboard on the next widget. Use positive 'offset' to access sub components of a multiple component widget. Use -1 to access previous widget.");
  m.def("SetNextItemAllowOverlap", &ImGui::SetNextItemAllowOverlap, "allow next item to be overlapped by a subsequent item. Useful with invisible buttons, selectable, treenode covering an area where subsequent items may need to be added. Note that both Selectable() and TreeNode() have dedicated flags doing this.");
  m.def("IsItemHovered", &ImGui::IsItemHovered, py::arg("flags") = 0, "is the last item hovered? (and usable, aka not blocked by a popup, etc.). See ImGuiHoveredFlags for more options.");
  m.def("IsItemActive", &ImGui::IsItemActive, "is the last item active? (e.g. button being held, text field being edited. This will continuously return true while holding mouse button on an item. Items that don't interact will always return false)");
  m.def("IsItemFocused", &ImGui::IsItemFocused, "is the last item focused for keyboard/gamepad navigation?");
  m.def("IsItemClicked", &ImGui::IsItemClicked, py::arg("mouse_button") = 0, "is the last item hovered and mouse clicked on? (**)  == IsMouseClicked(mouse_button) && IsItemHovered()Important. (**) this is NOT equivalent to the behavior of e.g. Button(). Read comments in function definition.");
  m.def("IsItemVisible", &ImGui::IsItemVisible, "is the last item visible? (items may be out of sight because of clipping/scrolling)");
  m.def("IsItemEdited", &ImGui::IsItemEdited, "did the last item modify its underlying value this frame? or was pressed? This is generally the same as the \"bool\" return value of many widgets.");
  m.def("IsItemActivated", &ImGui::IsItemActivated, "was the last item just made active (item was previously inactive).");
  m.def("IsItemDeactivated", &ImGui::IsItemDeactivated, "was the last item just made inactive (item was previously active). Useful for Undo/Redo patterns with widgets that require continuous editing.");
  m.def("IsItemDeactivatedAfterEdit", &ImGui::IsItemDeactivatedAfterEdit, "was the last item just made inactive and made a value change when it was active? (e.g. Slider/Drag moved). Useful for Undo/Redo patterns with widgets that require continuous editing. Note that you may get false positives (some widgets such as Combo()/ListBox()/Selectable() will return true even when clicking an already selected item).");
  m.def("IsItemToggledOpen", &ImGui::IsItemToggledOpen, "was the last item open state toggled? set by TreeNode().");
  m.def("IsAnyItemHovered", &ImGui::IsAnyItemHovered, "is any item hovered?");
  m.def("IsAnyItemActive", &ImGui::IsAnyItemActive, "is any item active?");
  m.def("IsAnyItemFocused", &ImGui::IsAnyItemFocused, "is any item focused?");
  m.def("GetItemID", &ImGui::GetItemID, "get ID of last item (~~ often same ImGui::GetID(label) beforehand)");
  m.def("GetItemRectMin", &ImGui::GetItemRectMin, "get upper-left bounding rectangle of the last item (screen space)");
  m.def("GetItemRectMax", &ImGui::GetItemRectMax, "get lower-right bounding rectangle of the last item (screen space)");
  m.def("GetItemRectSize", &ImGui::GetItemRectSize, "get size of last item");
  m.def("IsRectVisible", py::overload_cast<const ImVec2&, const ImVec2&>(&ImGui::IsRectVisible), py::arg("rect_min"), py::arg("rect_max"), "test if rectangle (in screen space) is visible / not clipped. to perform coarse clipping on user's side.");
  m.def("IsRectVisible", py::overload_cast<const ImVec2&>(&ImGui::IsRectVisible), py::arg("size"), "test if rectangle (of given size, starting from cursor position) is visible / not clipped.");
  m.def("BeginChildFrame", &ImGui::BeginChildFrame, py::arg("id"), py::arg("size"), py::arg("flags") = 0, "helper to create a child window / scrolling region that looks like a normal widget frame");
  m.def("EndChildFrame", &ImGui::EndChildFrame, "always call EndChildFrame() regardless of BeginChildFrame() return values (which indicates a collapsed/clipped window)");
  m.def("IsKeyDown", &ImGui::IsKeyDown, py::arg("key"), "is key being held.");
  m.def("IsKeyPressed", &ImGui::IsKeyPressed, py::arg("key"), py::arg("repeat") = true, "was key pressed (went from !Down to Down)? if repeat=true, uses io.KeyRepeatDelay / KeyRepeatRate");
  m.def("IsKeyReleased", &ImGui::IsKeyReleased, py::arg("key"), "was key released (went from Down to !Down)?");
  m.def("SetNextFrameWantCaptureKeyboard", &ImGui::SetNextFrameWantCaptureKeyboard, py::arg("want_capture_keyboard"), "Override io.WantCaptureKeyboard flag next frame (said flag is left for your application to handle, typically when true it instructs your app to ignore inputs). e.g. force capture keyboard when your widget is being hovered. This is equivalent to setting \"io.WantCaptureKeyboard = want_capture_keyboard\"; after the next NewFrame() call.");
  m.def("IsMouseDown", &ImGui::IsMouseDown, py::arg("button"), "is mouse button held?");
  m.def("IsMouseClicked", &ImGui::IsMouseClicked, py::arg("button"), py::arg("repeat") = false, "did mouse button clicked? (went from !Down to Down). Same as GetMouseClickedCount() == 1.");
  m.def("IsMouseReleased", &ImGui::IsMouseReleased, py::arg("button"), "did mouse button released? (went from Down to !Down)");
  m.def("IsMouseDoubleClicked", &ImGui::IsMouseDoubleClicked, py::arg("button"), "did mouse button double-clicked? Same as GetMouseClickedCount() == 2. (note that a double-click will also report IsMouseClicked() == true)");
  m.def("IsMouseHoveringRect", &ImGui::IsMouseHoveringRect, py::arg("r_min"), py::arg("r_max"), py::arg("clip") = true, "is mouse hovering given bounding rect (in screen space). clipped by current clipping settings, but disregarding of other consideration of focus/window ordering/popup-block.");
  m.def("IsAnyMouseDown", &ImGui::IsAnyMouseDown, "[WILL OBSOLETE] is any mouse button held? This was designed for backends, but prefer having backend maintain a mask of held mouse buttons, because upcoming input queue system will make this invalid.");
  m.def("GetMousePos", &ImGui::GetMousePos, "shortcut to ImGui::GetIO().MousePos provided by user, to be consistent with other calls");
  m.def("GetMousePosOnOpeningCurrentPopup", &ImGui::GetMousePosOnOpeningCurrentPopup, "retrieve mouse position at the time of opening popup we have BeginPopup() into (helper to avoid user backing that value themselves)");
  m.def("IsMouseDragging", &ImGui::IsMouseDragging, py::arg("button"), py::arg("lock_threshold") = -1.0f, "is mouse dragging? (if lock_threshold < -1.0f, uses io.MouseDraggingThreshold)");
  m.def("GetMouseDragDelta", &ImGui::GetMouseDragDelta, py::arg("button") = 0, py::arg("lock_threshold") = -1.0f, "return the delta from the initial clicking position while the mouse button is pressed or was just released. This is locked and return 0.0f until the mouse moves past a distance threshold at least once (if lock_threshold < -1.0f, uses io.MouseDraggingThreshold)");
  m.def("ResetMouseDragDelta", &ImGui::ResetMouseDragDelta, py::arg("button") = 0);
  m.def("GetMouseCursor", &ImGui::GetMouseCursor, "get desired mouse cursor shape. Important: reset in ImGui::NewFrame(), this is updated during the frame. valid before Render(). If you use software rendering by setting io.MouseDrawCursor ImGui will render those for you");
  m.def("SetMouseCursor", &ImGui::SetMouseCursor, py::arg("cursor_type"), "set desired mouse cursor shape");
  m.def("SetNextFrameWantCaptureMouse", &ImGui::SetNextFrameWantCaptureMouse, py::arg("want_capture_mouse"), "Override io.WantCaptureMouse flag next frame (said flag is left for your application to handle, typical when true it instucts your app to ignore inputs). This is equivalent to setting \"io.WantCaptureMouse = want_capture_mouse;\" after the next NewFrame() call.");
  m.def("GetClipboardText", &ImGui::GetClipboardText);
  m.def("SetClipboardText", &ImGui::SetClipboardText, py::arg("text"));


  m.def("Begin", [](char const* name, bool open, ImGuiWindowFlags flags)->py::tuple {
    bool shown = ImGui::Begin(name, &open, flags);
    return py::make_tuple(shown, open);
  }, py::arg("name"), py::arg("open") = true, py::arg("flags") = 0);
  m.def("BeginTable", [](const char* str_id, int column, ImGuiTableFlags flags, ImVec2 outer_size, float inner_width){
    bool ret = ImGui::BeginTable(str_id, column, flags, outer_size, inner_width);
    return py::make_tuple(ret, outer_size);
  }, py::arg("str_id"), py::arg("column"), py::arg("flags")=0, py::arg("outer_size")=ImVec2(0,0), py::arg("inner_width")=0.f);
  m.def("PushID", py::overload_cast<char const*>(&ImGui::PushID), py::arg("str_id"));
  m.def("PushID", py::overload_cast<int>(&ImGui::PushID), py::arg("int_id"));
  m.def("GetID",  py::overload_cast<char const*>(&ImGui::GetID), py::arg("str_id"));
  m.def("BeginPopupModal", [](char const* name, bool open, ImGuiWindowFlags flags)->py::tuple {
    bool shown = ImGui::BeginPopupModal(name, &open, flags);
    return py::make_tuple(shown, open);
  }, py::arg("name"), py::arg("open") = true, py::arg("flags") = 0);
  m.def("Text", [](std::string_view str){
    ImGui::TextUnformatted(&*str.begin(), &*str.end());
  }, py::arg("text"));
  m.def("InputText", [](char const* label, std::string str, ImGuiInputTextFlags flags) {
    bool mod = ImGui::InputText(label, &str, flags);
    return py::make_tuple(mod, str);
  }, py::arg("label"), py::arg("text"), py::arg("flags") = 0);
  m.def("InputTextMultiline", [](char const* label, std::string str, ImVec2 const& size, ImGuiInputTextFlags flags) {
    bool mod = ImGui::InputTextMultiline(label, &str, size, flags);
    return py::make_tuple(mod, str);
  }, py::arg("label"), py::arg("text"), py::arg("size") = ImVec2(0,0), py::arg("flags") = 0);
  m.def("Checkbox", [](char const* label, bool checked) {
    bool mod = ImGui::Checkbox(label, &checked);
    return py::make_tuple(mod, checked);
  }, py::arg("label"), py::arg("checked"));
  m.def("BeginTabItem", [](char const* label, bool open, ImGuiTabItemFlags flags){
    bool shown = ImGui::BeginTabItem(label, &open, flags);
    return py::make_tuple(shown, open);
  }, py::arg("name"), py::arg("open") = true, py::arg("flags") = 0);
  m.def("SetTooltip", [](char const* tip){
    ImGui::SetTooltip("%s", tip);
  }, py::arg("tooltip"));
  m.def("SetItemTooltip", [](char const* tip){
    ImGui::SetItemTooltip("%s", tip);
  }, py::arg("tooltip"));
  
  m.def("DragScalar", [](char const* label, ImGuiDataType type, py::object value, float speed, py::object vmin, py::object vmax, char const* format, ImGuiSliderFlags flags){
    int8_t   i8[4]  = {0}, i8minmax[2];
    uint8_t  u8[4]  = {0}, u8minmax[2];
    int16_t  i16[4] = {0}, i16minmax[2];
    uint16_t u16[4] = {0}, u16minmax[2];
    int32_t  i32[4] = {0}, i32minmax[2];
    uint32_t u32[4] = {0}, u32minmax[2];
    int64_t  i64[4] = {0}, i64minmax[2];
    uint64_t u64[4] = {0}, u64minmax[2];
    float    f32[4] = {0}, f32minmax[2];
    double   f64[4] = {0}, f64minmax[2];
    void     *pdata = nullptr;
    void      *pmin = nullptr, *pmax = nullptr;
    int     numcomp = 1;

#define TYPE_CASE(X, Y, T) \
        case ImGuiDataType_##X:\
          pdata = Y; Y[comp] = py::cast<T>(val);\
          if (!pmin && vmin) { Y##minmax[0] = py::cast<T>(vmin); pmin = Y##minmax; }\
          if (!pmax && vmax) { Y##minmax[1] = py::cast<T>(vmax); pmax = Y##minmax+1; }\
          break
    auto assign = [&](int comp, py::handle val) {
      switch(type) {
        TYPE_CASE(S8, i8, int8_t);
        TYPE_CASE(U8, u8, uint8_t);
        TYPE_CASE(S16, i16, int16_t);
        TYPE_CASE(U16, u16, uint16_t);
        TYPE_CASE(S32, i32, int32_t);
        TYPE_CASE(U32, u32, uint32_t);
        TYPE_CASE(S64, i64, int64_t);
        TYPE_CASE(U64, u64, uint64_t);
        TYPE_CASE(Float, f32, float);
        TYPE_CASE(Double, f64, double);
        default:
          throw std::runtime_error("unsupported type for DragScalar");
      }
    };
#undef TYPE_CASE
    if (py::isinstance<py::tuple>(value)) {
      py::tuple tp = value;
      numcomp = tp.size();
      if (numcomp < 1 || numcomp > 4)
        throw std::range_error("number of component not in range [1,4]");
      for (int i=0; i<numcomp; ++i)
        assign(i, tp[i]);
    } else {
      assign(0, value);
    }

    if (format && format[0]==0) format = nullptr;
    bool mod = ImGui::DragScalarN(label, type, pdata, numcomp, speed, pmin, pmax, format, flags);

    py::tuple retval(numcomp);
    for (int i=0; i<numcomp; ++i) {
      switch(type) {
        case ImGuiDataType_S8:
          retval[i] = i8[i]; break;
        case ImGuiDataType_U8:
          retval[i] = u8[i]; break;
        case ImGuiDataType_S16:
          retval[i] = i16[i]; break;
        case ImGuiDataType_U16:
          retval[i] = u16[i]; break;
        case ImGuiDataType_S32:
          retval[i] = i32[i]; break;
        case ImGuiDataType_U32:
          retval[i] = u32[i]; break;
        case ImGuiDataType_S64:
          retval[i] = i64[i]; break;
        case ImGuiDataType_U64:
          retval[i] = u64[i]; break;
        case ImGuiDataType_Float:
          retval[i] = f32[i]; break;
        case ImGuiDataType_Double:
          retval[i] = f64[i]; break;
        default:
          throw std::runtime_error("unsupported type for DragScalar");
      }
    }
    if (py::isinstance<py::tuple>(value))
      return py::make_tuple(mod, retval);
    else
      return py::make_tuple(mod, retval[0]);
  }, py::arg("label"), py::arg("type"), py::arg("value"), py::arg("speed")=1.f, py::arg("min")=py::none(), py::arg("max")=py::none(), py::arg("format")="", py::arg("flags")=0);

  m.def("SliderScalar", [](char const* label, ImGuiDataType type, py::object value, py::object vmin, py::object vmax, char const* format, ImGuiSliderFlags flags){
    int8_t   i8[4]  = {0}, i8minmax[2];
    uint8_t  u8[4]  = {0}, u8minmax[2];
    int16_t  i16[4] = {0}, i16minmax[2];
    uint16_t u16[4] = {0}, u16minmax[2];
    int32_t  i32[4] = {0}, i32minmax[2];
    uint32_t u32[4] = {0}, u32minmax[2];
    int64_t  i64[4] = {0}, i64minmax[2];
    uint64_t u64[4] = {0}, u64minmax[2];
    float    f32[4] = {0}, f32minmax[2];
    double   f64[4] = {0}, f64minmax[2];
    void     *pdata = nullptr;
    void      *pmin = nullptr, *pmax = nullptr;
    int     numcomp = 1;

#define TYPE_CASE(X, Y, T) \
        case ImGuiDataType_##X:\
          pdata = Y; Y[comp] = py::cast<T>(val);\
          if (!pmin && vmin) { Y##minmax[0] = py::cast<T>(vmin); pmin = Y##minmax; }\
          if (!pmax && vmax) { Y##minmax[1] = py::cast<T>(vmax); pmax = Y##minmax+1; }\
          break
    auto assign = [&](int comp, py::handle val) {
      switch(type) {
        TYPE_CASE(S8, i8, int8_t);
        TYPE_CASE(U8, u8, uint8_t);
        TYPE_CASE(S16, i16, int16_t);
        TYPE_CASE(U16, u16, uint16_t);
        TYPE_CASE(S32, i32, int32_t);
        TYPE_CASE(U32, u32, uint32_t);
        TYPE_CASE(S64, i64, int64_t);
        TYPE_CASE(U64, u64, uint64_t);
        TYPE_CASE(Float, f32, float);
        TYPE_CASE(Double, f64, double);
        default:
          throw std::runtime_error("unsupported type for DragScalar");
      }
    };
#undef TYPE_CASE
    if (py::isinstance<py::tuple>(value)) {
      py::tuple tp = value;
      numcomp = tp.size();
      if (numcomp < 1 || numcomp > 4)
        throw std::range_error("number of component not in range [1,4]");
      for (int i=0; i<numcomp; ++i)
        assign(i, tp[i]);
    } else {
      assign(0, value);
    }

    bool mod = ImGui::SliderScalarN(label, type, pdata, numcomp, pmin, pmax, format, flags);

    py::tuple retval(numcomp);
    for (int i=0; i<numcomp; ++i) {
      switch(type) {
        case ImGuiDataType_S8:
          retval[i] = i8[i]; break;
        case ImGuiDataType_U8:
          retval[i] = u8[i]; break;
        case ImGuiDataType_S16:
          retval[i] = i16[i]; break;
        case ImGuiDataType_U16:
          retval[i] = u16[i]; break;
        case ImGuiDataType_S32:
          retval[i] = i32[i]; break;
        case ImGuiDataType_U32:
          retval[i] = u32[i]; break;
        case ImGuiDataType_S64:
          retval[i] = i64[i]; break;
        case ImGuiDataType_U64:
          retval[i] = u64[i]; break;
        case ImGuiDataType_Float:
          retval[i] = f32[i]; break;
        case ImGuiDataType_Double:
          retval[i] = f64[i]; break;
        default:
          throw std::runtime_error("unsupported type for SliderScalar");
      }
    }
    if (py::isinstance<py::tuple>(value))
      return py::make_tuple(mod, retval);
    else
      return py::make_tuple(mod, retval[0]);
  }, py::arg("label"), py::arg("type"), py::arg("value"), py::arg("min")=0, py::arg("max")=10, py::arg("format")="%.3f", py::arg("flags")=0);

  // TODO: VSliderScalar, InputScalarN, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker3, ColorButton

}
