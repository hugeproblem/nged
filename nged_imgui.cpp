#define IMGUI_DEFINE_MATH_OPERATORS
#include "nged.h"
#include "nged_imgui.h"
#include "style.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include "res/fa_icondef.h"

#include <nlohmann/json.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <charconv>
#include <limits>
#include <memory>

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

using namespace detail;
// helpers {{{
namespace helper {

static ptrdiff_t longestCommonSequenceLength(String const& a, String const& b)
{
  String const&     from = a.length() > b.length() ? b : a;
  String const&     to   = a.length() > b.length() ? a : b;
  Vector<ptrdiff_t> buf1(from.length() + 1, 0);
  Vector<ptrdiff_t> buf2(from.length() + 1, 0);
  for (size_t i = 1; i <= to.length(); ++i) {
    for (size_t j = 1, n = from.length(); j <= n; ++j) {
      buf2[j] = (from[j - 1] == to[i - 1] ? buf1[j - 1] + 1 : std::max(buf1[j], buf2[j - 1]));
    }
    buf1.swap(buf2);
  }
  return buf1.back();
}

// public domain fuzzy string match from
// https://github.com/forrestthewoods/lib_fts/blob/master/code/fts_fuzzy_match.h {{{ thanks Forrest
// Smith ref: https://www.forrestthewoods.com/blog/reverse_engineering_sublime_texts_fuzzy_match/
static bool fuzzy_match_recursive(
  const char*    pattern,
  const char*    patternend,
  const char*    str,
  const char*    strend,
  int&           outScore,
  const char*    strBegin,
  uint8_t const* srcMatches,
  uint8_t*       matches,
  int            maxMatches,
  int            nextMatch,
  int&           recursionCount,
  int            recursionLimit)
{
  // Count recursions
  ++recursionCount;
  if (recursionCount >= recursionLimit)
    return false;

  // Detect end of strings
  if (pattern == patternend || str == strend)
    return false;

  // Recursion params
  bool    recursiveMatch = false;
  uint8_t bestRecursiveMatches[256];
  int     bestRecursiveScore = 0;

  // Loop through pattern and str looking for a match
  bool first_match = true;
  while (pattern != patternend && str != strend) {

    // Found match
    if (tolower(*pattern) == tolower(*str)) {

      // Supplied matches buffer was too short
      if (nextMatch >= maxMatches)
        return false;

      // "Copy-on-Write" srcMatches into matches
      if (first_match && srcMatches) {
        memcpy(matches, srcMatches, nextMatch);
        first_match = false;
      }

      // Recursive call that "skips" this match
      uint8_t recursiveMatches[256];
      int     recursiveScore;
      if (fuzzy_match_recursive(
            pattern,
            patternend,
            str + 1,
            strend,
            recursiveScore,
            strBegin,
            matches,
            recursiveMatches,
            sizeof(recursiveMatches),
            nextMatch,
            recursionCount,
            recursionLimit)) {

        // Pick best recursive score
        if (!recursiveMatch || recursiveScore > bestRecursiveScore) {
          memcpy(bestRecursiveMatches, recursiveMatches, 256);
          bestRecursiveScore = recursiveScore;
        }
        recursiveMatch = true;
      }

      // Advance
      matches[nextMatch++] = (uint8_t)(str - strBegin);
      ++pattern;
    }
    ++str;
  }

  // Determine if full pattern was matched
  bool matched = pattern == patternend ? true : false;

  // Calculate score
  if (matched) {
    const int sequential_bonus   = 15; // bonus for adjacent matches
    const int separator_bonus    = 30; // bonus if match occurs after a separator
    const int camel_bonus        = 30; // bonus if match is uppercase and prev is lower
    const int first_letter_bonus = 15; // bonus if the first letter is matched

    const int leading_letter_penalty =
      -5; // penalty applied for every letter in str before the first match
    const int max_leading_letter_penalty = -15; // maximum penalty for leading letters
    const int unmatched_letter_penalty   = -1;  // penalty for every letter that doesn't matter

    // Iterate str to end
    while (str != strend)
      ++str;

    // Initialize score
    outScore = 100;

    // Apply leading letter penalty
    int penalty = leading_letter_penalty * matches[0];
    if (penalty < max_leading_letter_penalty)
      penalty = max_leading_letter_penalty;
    outScore += penalty;

    // Apply unmatched penalty
    int unmatched = (int)(str - strBegin) - nextMatch;
    outScore += unmatched_letter_penalty * unmatched;

    // Apply ordering bonuses
    for (int i = 0; i < nextMatch; ++i) {
      uint8_t currIdx = matches[i];

      if (i > 0) {
        uint8_t prevIdx = matches[i - 1];

        // Sequential
        if (currIdx == (prevIdx + 1))
          outScore += sequential_bonus;
      }

      // Check for bonuses based on neighbor character value
      if (currIdx > 0) {
        // Camel case
        char neighbor = strBegin[currIdx - 1];
        char curr     = strBegin[currIdx];
        if (::islower(neighbor) && ::isupper(curr))
          outScore += camel_bonus;

        // Separator
        bool neighborSeparator = neighbor == '_' || neighbor == ' ';
        if (neighborSeparator)
          outScore += separator_bonus;
      } else {
        // First letter
        outScore += first_letter_bonus;
      }
    }
  }

  // Return best result
  if (recursiveMatch && (!matched || bestRecursiveScore > outScore)) {
    // Recursive score is better than "this"
    memcpy(matches, bestRecursiveMatches, maxMatches);
    outScore = bestRecursiveScore;
    return true;
  } else if (matched) {
    // "this" score is better than recursive
    return true;
  } else {
    // no match
    return false;
  }
}

static bool
fuzzy_match(StringView pattern, StringView str, int& outScore, uint8_t* matches, int maxMatches)
{
  int recursionCount = 0;
  int recursionLimit = 10;
  return fuzzy_match_recursive(
    pattern.data(),
    pattern.data() + pattern.size(),
    str.data(),
    str.data() + str.size(),
    outScore,
    str.data(),
    nullptr,
    matches,
    maxMatches,
    0,
    recursionCount,
    recursionLimit);
}

static bool fuzzy_match(StringView pattern, StringView str, int& outScore)
{
  uint8_t matches[256];
  return fuzzy_match(pattern, str, outScore, matches, sizeof(matches));
}
// fuzzy string match }}}

static Vector<size_t> fuzzy_match_and_argsort(
  StringView                pattern,
  Vector<StringView> const& candidates)
{
  Vector<size_t>                 result;
  Vector<std::pair<int, size_t>> matches;
  for (size_t i = 0; i < candidates.size(); ++i) {
    int score = 0;
    if (fuzzy_match(pattern, candidates[i], score)) {
      matches.emplace_back(score, i);
    }
  }
  std::sort(matches.begin(), matches.end(), std::greater<>());
  result.resize(matches.size());
  std::transform(
    matches.begin(), matches.end(), result.begin(), [](auto const& pair) { return pair.second; });
  return result;
}

static Vector<StringView> fuzzy_match_and_sort(
  StringView                pattern,
  Vector<StringView> const& candidates)
{
  Vector<StringView> result;
  auto               order = fuzzy_match_and_argsort(pattern, candidates);
  if (!order.empty()) {
    result.reserve(order.size());
    for (auto idx : order)
      result.push_back(candidates[idx]);
  }
  return result;
}

} // namespace helper
// }}} helper

// Shared Resource {{{
namespace detail {
void reloadImGuiFonts(
  ImFont*& sansSerif,
  ImFont*& mono,
  ImFont*& icon,
  ImFont*& large,
  ImFont*& largeIcon);
}

ImGuiResource ImGuiResource::instance_;

ImFont* ImGuiResource::getBestMatchingFont(Canvas::TextStyle const& style, float scale) const
{
  float const size = Canvas::floatFontSize(style.size) * scale;
  auto const& ui   = UIStyle::instance();
  if (style.font == Canvas::FontFamily::Icon) {
    if (size >= ui.normalFontSize * 1.4f && largeIconFont)
      return largeIconFont;
    else
      return iconFont;
  } else if (style.font == Canvas::FontFamily::Mono) {
    return monoFont;
  } else {
    if (size >= ui.normalFontSize * 1.4f && largeSansSerifFont)
      return largeSansSerifFont;
    else
      return sansSerifFont;
  }
  return nullptr;
}

void ImGuiResource::reloadFonts()
{
  reloadImGuiFonts(
    instance_.sansSerifFont,
    instance_.monoFont,
    instance_.iconFont,
    instance_.largeSansSerifFont,
    instance_.largeIconFont);
}
// }}} Shared Resource

// Canvas {{{
class ImGuiCanvas : public Canvas
{
  ImDrawList* drawList_;
  Vec2        windowOffset_ = {0, 0};

  friend void setupImGuiCanvas(Canvas*, ImDrawList*);

public:
  ImGuiCanvas() : Canvas(), drawList_(nullptr) {}

  AABB viewport() const override
  {
    return AABB(
      screenToCanvas_.transformPoint(windowOffset_),
      screenToCanvas_.transformPoint(windowOffset_ + viewSize_));
  }
  void updateMatrix() override
  {
    canvasToScreen_ = Mat3::fromSRT(Vec2(viewScale_, viewScale_), 0.f, -viewPos_) *
                      Mat3::fromRTS(Vec2(1, 1), 0, windowOffset_ + viewSize_ * 0.5f);
    screenToCanvas_ = canvasToScreen_.inverse();
  }

  void setCurrentLayer(Layer layer) override
  {
    layer_ = layer;
    drawList_->ChannelsSetCurrent(static_cast<int>(layer));
  }
  void drawLine(Vec2 a, Vec2 b, uint32_t color, float width) const override
  {
    drawList_->AddLine(
      imvec(canvasToScreen_.transformPoint(a)),
      imvec(canvasToScreen_.transformPoint(b)),
      color,
      viewScale_ * width);
  }
  void drawRect(Vec2 topleft, Vec2 bottomright, float cornerradius, ShapeStyle style)
    const override
  {
    if (style.filled)
      drawList_->AddRectFilled(
        imvec(canvasToScreen_.transformPoint(topleft)),
        imvec(canvasToScreen_.transformPoint(bottomright)),
        utils::bswap(style.fillColor),
        cornerradius * viewScale_);
    if (style.strokeWidth * viewScale_ > 0.1f)
      drawList_->AddRect(
        imvec(canvasToScreen_.transformPoint(topleft)),
        imvec(canvasToScreen_.transformPoint(bottomright)),
        utils::bswap(style.strokeColor),
        cornerradius * viewScale_,
        0,
        style.strokeWidth * viewScale_);
  }
  void drawCircle(Vec2 center, float radius, int nsegments, ShapeStyle style) const override
  {
    center = canvasToScreen_.transformPoint(center);
    radius = radius * viewScale_;
    if (style.filled)
      drawList_->AddCircleFilled(imvec(center), radius, utils::bswap(style.fillColor), nsegments);
    if (style.strokeWidth * viewScale_ > 0.1f) {
      drawList_->AddCircle(
        imvec(center),
        radius,
        utils::bswap(style.strokeColor),
        nsegments,
        style.strokeWidth * viewScale_);
    }
  }
  void drawPoly(Vec2 const* pts, sint numpt, bool closed, ShapeStyle style) const override
  {
    int npt = static_cast<int>(numpt);
    assert(sint(npt) == numpt);
    Vector<ImVec2> transformed(numpt);
    for (sint i = 0; i < numpt; ++i) {
      transformed[i] = imvec(canvasToScreen_.transformPoint(pts[i]));
    }

    if (closed && style.filled)
      drawList_->AddConvexPolyFilled(transformed.data(), npt, utils::bswap(style.fillColor));
    if (style.strokeWidth * viewScale_ > 0.1f) {
      auto flags = closed ? ImDrawFlags_Closed : 0;
      drawList_->AddPolyline(
        transformed.data(),
        npt,
        utils::bswap(style.strokeColor),
        flags,
        style.strokeWidth * viewScale_);
    }
  }
  Vec2 measureTextSize(StringView text, TextStyle const& style) const override
  {
    auto*       font      = ImGuiResource::instance().getBestMatchingFont(style, viewScale_);
    float const fontsize  = floatFontSize(style.size);
    auto        textbegin = text.data(), textend = text.data() + text.size();
    auto        size = font->CalcTextSizeA(fontsize, FLT_MAX, 0.f, textbegin, textend);
    return vec(size);
  }
  void drawText(Vec2 pos, StringView text, TextStyle const& style) const override
  {
    // TODO: font / align / size &etc.
    pos                   = canvasToScreen().transformPoint(pos);
    auto*       font      = ImGuiResource::instance().getBestMatchingFont(style, viewScale_);
    float const fontsize  = floatFontSize(style.size) * viewScale_;
    auto        textbegin = text.data(), textend = text.data() + text.size();
    auto        textpos = pos;
    if (style.align != TextAlign::Left || style.valign != TextVerticalAlign::Top) {
      auto size = font->CalcTextSizeA(fontsize, FLT_MAX, 0.f, textbegin, textend);
      if (style.align == TextAlign::Center)
        textpos.x -= size.x / 2.f;
      else if (style.align == TextAlign::Right)
        textpos.x -= size.x;
      if (style.valign == TextVerticalAlign::Center)
        textpos.y -= size.y / 2.f;
      else if (style.valign == TextVerticalAlign::Bottom)
        textpos.y -= size.y;
    }
    drawList_->AddText(
      font, fontsize, imvec(textpos), utils::bswap(style.color), textbegin, textend);
  }
  void drawTextUntransformed(Vec2 pos, StringView text, TextStyle const& style, float scale) const override
  {
    // TODO: font / align / size &etc.
    auto*       font      = ImGuiResource::instance().getBestMatchingFont(style, 1);
    float const fontsize  = floatFontSize(style.size) * scale;
    auto        textbegin = text.data(), textend = text.data() + text.size();
    auto        textpos = pos;
    if (style.align != TextAlign::Left || style.valign != TextVerticalAlign::Top) {
      auto size = font->CalcTextSizeA(fontsize, FLT_MAX, 0.f, textbegin, textend);
      if (style.align == TextAlign::Center)
        textpos.x -= size.x / 2.f;
      else if (style.align == TextAlign::Right)
        textpos.x -= size.x;
      if (style.valign == TextVerticalAlign::Center)
        textpos.y -= size.y / 2.f;
      else if (style.valign == TextVerticalAlign::Bottom)
        textpos.y -= size.y;
    }
    drawList_->AddText(
      font, fontsize, imvec(textpos+windowOffset_), utils::bswap(style.color), textbegin, textend);
  }
};

std::unique_ptr<Canvas> newImGuiCanvas()
{
  return std::make_unique<ImGuiCanvas>();
}
void setupImGuiCanvas(Canvas* c, ImDrawList* d)
{
  auto* ic          = static_cast<ImGuiCanvas*>(c);
  ic->drawList_     = d;
  ic->windowOffset_ = vec(ImGui::GetWindowPos()) + vec(ImGui::GetWindowContentRegionMin());
  ic->setViewSize(vec(ImGui::GetContentRegionAvail()));
  ic->updateMatrix();
}
// }}} Canvas

// Comment Box Impl {{{
void ImGuiCommentBox::onInspect(GraphView* inspector)
{
  ImGui::TextUnformatted("Comment:");
  ImGui::PushItemWidth(-4);
  if (ImGui::InputTextMultiline("##Comment", &text_, ImVec2(0,0), ImGuiInputTextFlags_EnterReturnsTrue))
    if (auto graph = parent())
      if (auto doc = graph->docRoot())
        doc->history().commitIfAppropriate("edit commit");
  ImGui::PopItemWidth();
}
// }}} Comment Box Impl

// CommandManager {{{
CommandManager::CommandManager() {}

// ref: https://zh.wikipedia.org/wiki/ASCII
static inline ImGuiKey asciiToImGuiKey(uint8_t ch)
{
  if (ch >= '0' && ch <= '9') {
    return static_cast<ImGuiKey>(ImGuiKey_0 + ch - '0');
  } else if (ch >= 'A' && ch <= 'Z') {
    return static_cast<ImGuiKey>(ImGuiKey_A + ch - 'A');
  } else if (ch >= 'a' && ch <= 'z') {
    return static_cast<ImGuiKey>(ImGuiKey_A + ch - 'a');
  } else if (ch >= 0xF1 && ch <= 0xFC) { // F1 to F12
    return static_cast<ImGuiKey>(ImGuiKey_F1 + ch-0xF1);
  } else {
    switch (ch) {
    case '\'': return ImGuiKey_Apostrophe;
    case '\t': return ImGuiKey_Tab;
    case '\r': return ImGuiKey_Enter;
    case '\b': return ImGuiKey_Backspace;
    case '\x7f': return ImGuiKey_Delete;
    case '\x1b': return ImGuiKey_Escape;
    case '`': return ImGuiKey_GraveAccent;
    case ' ': return ImGuiKey_Space;
    case ',': return ImGuiKey_Comma;
    case '-': return ImGuiKey_Minus;
    case '.': return ImGuiKey_Period;
    case '/': return ImGuiKey_Slash;
    case '\\': return ImGuiKey_Backslash;
    case ';': return ImGuiKey_Semicolon;
    case '=': return ImGuiKey_Equal;
    case '[': return ImGuiKey_LeftBracket;
    case ']': return ImGuiKey_RightBracket;
    default: return ImGuiKey_None;
    }
  }
  return ImGuiKey_None;
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

bool Shortcut::check(Shortcut const& shortcut)
{
  auto const key       = asciiToImGuiKey(shortcut.key);
  auto const imkeymods = ImGui::GetIO().KeyMods;
  auto       mod       = ModKey::NONE;
  if (imkeymods & ImGuiMod_Ctrl)
    mod = utils::eor(mod, ModKey::CTRL);
  if (imkeymods & ImGuiMod_Shift)
    mod = utils::eor(mod, ModKey::SHIFT);
  if (imkeymods & ImGuiMod_Alt)
    mod = utils::eor(mod, ModKey::ALT);
  if (imkeymods & ImGuiMod_Super)
    mod = utils::eor(mod, ModKey::SUPER);
  if (key != ImGuiKey_None && shortcut.mod == mod && ImGui::IsKeyPressed(key, false))
    return true;
  return false;
}

void CommandManager::checkShortcut(GraphView* view)
{
  if (prompting_) {
    // don't check shortcuts
  } else {
    for (auto&& cmd : commands_) {
      if (cmd->mayModifyGraph() && view->readonly())
        continue;
      auto matchkind = [cmd](GraphView* view){
        auto const& kind = view->kind();
        for (auto&& part: utils::strsplit(cmd->view(), "|"))
          if (part == kind)
            return true;
        return false;
      };
      if (cmd->view() == "*" || view && matchkind(view)) {
        if (Shortcut::check(cmd->shortcut())) {
          auto const key = asciiToImGuiKey(cmd->shortcut().key);
          msghub::infof("shortcut for command {} triggered", cmd->name());
          ImGui::GetIO().AddKeyEvent(key, false);
          ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
          // the callback may bring up file dialogs, which will block key events and mess up the
          // state, we manually add key-up events here
          if (!cmd->hasPrompt())
            cmd->onConfirm(view);
          else
            prompt(cmd, view);
        }
      }
    }
  }
}

void CommandManager::prompt(CommandManager::CommandPtr cmd, GraphView* view)
{
  msghub::infof("open prompt for command {} ...", cmd->name());
  prompting_     = cmd;
  promptingView_ = view->weak_from_this();
  ImGui::OpenPopup("CommandManager.prompt");
  cmd->onOpenPrompt(view);
}

void CommandManager::openPalette()
{
  ImGui::OpenPopup("CommandManager.palette");
  paletteInput_ = "";
}

// Simple Command {{{
void CommandManager::SimpleCommand::onOpenPrompt(GraphView* view)
{
  if (promptDefault_)
    promptInput_ = promptDefault_(view);
}

bool CommandManager::SimpleCommand::onUpdatePrompt(GraphView* view)
{
  if (!onConfirmCallback_)
    return false;
  ImGui::Text("%s: ", argPrompt_.c_str());
  if (ImGui::IsWindowAppearing())
    ImGui::SetKeyboardFocusHere(0);
  bool confirmed =
    ImGui::InputText("##prompt", &promptInput_, ImGuiInputTextFlags_EnterReturnsTrue);
  if (confirmed && view) {
    // as the callback may bring up file dialogs, which will block key events and mess up the
    // state, we manually add key-up events here
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
    onConfirmCallback_(view, promptInput_);
    return false;
  } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    return false;
  }
  return true;
}
// }}} Simple Command

void CommandManager::resetPrompt()
{
  prompting_.reset();
  promptingView_.reset();
}

void CommandManager::update(GraphView* view)
{
  // msghub::debugf("enter is pressed: {}  escape is pressed: {}",
  // ImGui::IsKeyPressed(ImGuiKey_Enter, false), ImGui::IsKeyPressed(ImGuiKey_Escape, false));
  auto const  containerPos  = vec(ImGui::GetWindowContentRegionMin()) + vec(ImGui::GetWindowPos());
  auto const  containerSize = ImGui::GetContentRegionAvail();
  float const popupWidth    = containerSize.x * UIStyle::instance().commandPaletteWidthRatio;

  auto promptingViewPtr = promptingView_.lock();
  if (prompting_) {
    if (!prompting_->hasPrompt()) {
      resetPrompt();
      return;
    }
    if (view != promptingViewPtr.get())
      return;
  }

  ImGui::SetNextWindowPos(
    ImVec2{containerPos.x + containerSize.x / 2.f - popupWidth / 2.f, containerPos.y});
  ImGui::SetNextWindowSize(ImVec2{popupWidth, 0});
  auto& IO = ImGui::GetIO();
  if (prompting_) {
    if (ImGui::BeginPopup("CommandManager.prompt")) {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.f, 4.f});
      ImGui::PushItemWidth(-8);
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
      ImGui::Indent(8);

      if (!prompting_->onUpdatePrompt(promptingViewPtr.get()))
        resetPrompt();

      ImGui::Unindent();
      ImGui::Dummy({0, 8});
      ImGui::PopItemWidth();
      ImGui::PopStyleVar();
      ImGui::EndPopup();
    } else {
      resetPrompt();
    }
  } else if (ImGui::BeginPopup("CommandManager.palette")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.f, 4.f});
    ImGui::PushItemWidth(-8);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    ImGui::Indent(8);
    if (ImGui::IsWindowAppearing())
      ImGui::SetKeyboardFocusHere(0);
    bool confirmed =
      ImGui::InputText("##prompt", &paletteInput_, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Separator();
    Vector<StringView> cmdDescList;
    Vector<CommandPtr> cmdList;
    for (auto&& cmd : commands_) {
      if (cmd->mayModifyGraph() && view->readonly())
        continue;
      auto matchkind = [cmd](GraphView* view){
        auto const& kind = view->kind();
        for (auto&& part: utils::strsplit(cmd->view(), "|"))
          if (part == kind)
            return true;
        return false;
      };
      if (!cmd->hiddenInMenu() && (cmd->view() == "*" || view && matchkind(view))) {
        cmdDescList.push_back(cmd->description());
        cmdList.push_back(cmd);
      }
    }
    auto order = helper::fuzzy_match_and_argsort(paletteInput_, cmdDescList);
    if (paletteInput_.empty()) {
      order = utils::argsort(cmdDescList);
    }
    CommandPtr toExecute = nullptr;
    for (size_t idx : order) {
      auto               cmd = cmdList[idx];
      Vector<StringView> shortcutKeys;
      auto shortcutStr = Shortcut::describ(cmd->shortcut());
      if (
        ImGui::MenuItem(cmd->description().c_str(), shortcutStr.c_str()) ||
        (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
        toExecute = cmd;
        break;
      }
    }
    if (
      toExecute == nullptr && !paletteInput_.empty() && !order.empty() &&
      ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
      toExecute = cmdList[order[0]];
    }

    if (toExecute || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      paletteInput_ = "";
      ImGui::CloseCurrentPopup();
    }
    ImGui::Unindent();
    ImGui::Dummy({0, 8});
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::EndPopup();

    // execute the command
    if (toExecute) {
      msghub::infof("command {} triggered from palette", toExecute->name());
      // as the callback may bring up file dialogs, which will block key events and mess up the
      // state, we manually add key-up events here
      IO.AddKeyEvent(ImGuiKey_Enter, false);

      if (!toExecute->hasPrompt())
        toExecute->onConfirm(view);
      else
        prompt(toExecute, view);

      paletteInput_ = "";
    }
  }
}

void CommandManager::draw(NetworkView* view)
{
  if (prompting_)
    prompting_->draw(view);
}
// }}} CommandManager

// Commands {{{
// Find Node {{{
class FindNodeCommand : public CommandManager::Command
{
  String prompt_;
  bool   fuzzy_ = true;

  struct MatchingItem
  {
    ItemID id;
    String label;
  };
  std::multimap<int, MatchingItem, std::greater<>> matchedNodes_;

public:
  FindNodeCommand(Shortcut shortcut):
    Command("Edit/Find", "Find Node ...", "network", shortcut, false)
  {
    setMayModifyGraph(false);
  }
  void onConfirm(GraphView* view) override
  {
    auto* netview = static_cast<NetworkView*>(view);
    HashSet<ItemID> ids;
    for (auto&& pair: matchedNodes_)
      ids.insert(pair.second.id);
    netview->setSelectedItems(ids);
    netview->zoomToSelected(0.5f);
    matchedNodes_.clear();
  }
  bool hasPrompt() const override { return true; }
  bool onUpdatePrompt(GraphView* view) override
  {
    if (ImGui::IsWindowAppearing())
      ImGui::SetKeyboardFocusHere(0);
    auto recheck = ImGui::InputText("###Name", &prompt_, ImGuiInputTextFlags_AutoSelectAll);
    recheck |= ImGui::Checkbox("Fuzzy Match", &fuzzy_);
    if (prompt_.empty()) return true;
    if (!view) return false;
    auto  graph = view->graph();
    if (!graph) return false;
    auto* netview = static_cast<NetworkView*>(view);
    if (recheck) {
      matchedNodes_.clear();
      for (auto id : graph->items()) {
        if (auto* node = graph->get(id)->asNode()) {
          StringView name = node->label();
          if (name.empty())
            name = node->name();
          if (name.empty())
            name = node->type();
          if (fuzzy_) {
            if (int score; helper::fuzzy_match(prompt_, name, score)) {
              matchedNodes_.insert({score, {id, String(name)}});
            }
          } else {
            if (
              name.size() >= prompt_.size() &&
              name.substr(0, prompt_.size()) == prompt_)
              matchedNodes_.insert({100, {id, String(name)}});
          }
        }
      }
    }
    if (!matchedNodes_.empty())
      ImGui::Separator();
    for (auto&& pair: matchedNodes_) {
      if (auto itemptr = graph->get(pair.second.id)) {
        if (auto* nodeptr = itemptr->asNode()) {
          auto label = fmt::format("{}##{}", pair.second.label, pair.second.id.value());
          bool clicked = ImGui::MenuItem(label.c_str());
          if (ImGui::IsItemHovered() || clicked) {
            if (view->kind()=="network") {
              auto* netview = static_cast<NetworkView*>(view);
              netview->setSelectedItems({pair.second.id});
              netview->zoomToSelected(0.2f, 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter))
              return false;
          }
          if (clicked)
            return false;
        }
      }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      matchedNodes_.clear();
      return false;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
      onConfirm(view);
      return false;
    }
    return true;
  }
  void draw(NetworkView* view) override
  {
    if (!matchedNodes_.empty()) {
      view->canvas()->pushLayer(ImGuiCanvas::Layer::Higher);
      auto const style = Canvas::ShapeStyle{false, 0, 10.f, 0xff0000ff};
      for (auto&& pair: matchedNodes_) {
        if (auto item = view->graph()->tryGet(pair.second.id)) {
          auto bb = item->aabb().expanded(20.f);
          view->canvas()->drawRect(bb.min, bb.max, 16.f, style);
        }
      }
      view->canvas()->popLayer();
    }
  }
};
// }}} Find Node

// Colorize {{{
class ColorizeCommand: public CommandManager::Command
{
  Color           initialColor_;
  HashSet<ItemID> affectings_;
  bool            edited_ = false;
  float           color_[4] = {0};

public:
  ColorizeCommand():
    Command("Edit/Colorize", "Colorize Selection ...", "network", Shortcut{'C'}, false)
  {}
  bool hasPrompt() const override { return true; }
  void onOpenPrompt(GraphView* view)
  {
    assert(view->kind()=="network");
    auto* netview = static_cast<NetworkView*>(view);
    auto const& selection = netview->selectedItems();
    auto graph = view->graph();
    affectings_.clear();
    float avgcolor[4] = {0};
    for (auto id: selection) {
      if (auto* dye = graph->get(id)->asDyeable()) {
        affectings_.insert(id);
        auto c = gmath::toFloatSRGB(dye->color());
        avgcolor[0] += c.r;
        avgcolor[1] += c.g;
        avgcolor[2] += c.b;
        avgcolor[3] += c.a;
      }
    }
    if (!affectings_.empty())
      for (int i=0, n=int(affectings_.size()); i<4; ++i) // averaging sRGB color is wrong, but as they are all UI elements, here maybe makes sense
        avgcolor[i] /= float(n);
    memcpy(color_, avgcolor, sizeof(avgcolor));
    edited_ = false;
  }
  bool onUpdatePrompt(GraphView* view) override
  {
    if (affectings_.empty())
      return false;
    if (ImGui::ColorEdit4(
          "##Color",
          color_,
          ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaPreviewHalf)) {
      edited_ = true;
      auto c = gmath::toSRGB(gmath::FloatSRGBColor{color_[0],color_[1],color_[2],color_[3]});
      for (auto id : affectings_) {
        if (auto item = view->graph()->tryGet(id))
          if (auto dye = item->asDyeable())
            dye->setColor(c);
      }
    }
    if (edited_ && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter)))
      view->doc()->history().commitIfAppropriate("change color");
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
      return false;
    return true;
  }
  void onConfirm(GraphView*) override {}
};
// }}}
// }}}

// Network View {{{
class ImGuiNetworkView : public ImGuiGraphView<ImGuiNetworkView, NetworkView>
{
private:
  bool everDrawn_ = false;
  float previousDPI_ = 1.f;

public:
  ImGuiNetworkView(NodeGraphEditor* editor, NodeGraphDocPtr doc)
      : ImGuiGraphView(editor, doc, newImGuiCanvas())
  {
  }

  bool hasMenu() const override { return true; }
  void updateMenu() override
  {
    if (ImGui::BeginMenuBar()) {
      Vector<GraphPtr> path;
      for (auto cwd = graph(); cwd; cwd = cwd->parent()?cwd->parent()->shared_from_this():nullptr) {
        path.emplace_back(cwd);
      }
      ImGui::TextDisabled("cwd:");
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.f, 4.f});
      for (auto ritr = path.rbegin(); ritr != path.rend(); ++ritr) {
        auto name = fmt::format("{}/", (*ritr)->name());
        if (ImGui::MenuItem(name.c_str())) {
          reset(*ritr);
          break;
        }
      }
      ImGui::PopStyleVar();
      ImGui::EndMenuBar();
    }
  }

  void drawContent()
  {
    if (dpiScale_ != previousDPI_)
      canvas()->setViewScale(canvas()->viewScale() * dpiScale_ / previousDPI_);
    previousDPI_ = dpiScale_;
    ImGui::BeginChild(
      "Canvas", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
    if (!everDrawn_) {
      zoomToSelected(0);
      everDrawn_ = true;
    }

    auto* drawlist = ImGui::GetWindowDrawList();
    drawlist->ChannelsSplit(static_cast<int>(Canvas::Layer::Count));
    setupImGuiCanvas(canvas(), drawlist);
    canvas_->pushLayer(Canvas::Layer::Standard);

    NetworkView::draw();
    editor()->commandManager().draw(this);

    updateAndDrawEffects(dt_);

    canvas_->popLayer();
    drawlist->ChannelsMerge();
    ImGui::EndChild();
  }
};
// }}} NetworkView

// Inspector View {{{
class ImGuiInspectorView : public ImGuiGraphView<ImGuiInspectorView, InspectorView>
{
public:
  ImGuiInspectorView(NodeGraphEditor* editor) : ImGuiGraphView(editor) {}
  void drawContent()
  {
    auto toggle = [](char const* text, char const* icontrue, char const* iconfalse, bool& value, char const* tooltip) {
      if (text) {
        ImGui::TextUnformatted(text);
        ImGui::SameLine();
      }
      ImGui::PushFont(ImGuiResource::instance().iconFont);
      ImGui::PushStyleColor(ImGuiCol_Button, 0x00333333);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x77aaaaaa);
      ImGui::PushStyleColor(ImGuiCol_Text, value? 0xffffffff : 0xffaaaaaa);
      auto pressed = ImGui::Button(value? icontrue : iconfalse);
      ImGui::PopStyleColor(3);
      ImGui::PopFont();
      if (tooltip && ImGui::IsItemHovered())
        ImGui::SetTooltip(tooltip);
      if (pressed)
        value = !value;
      return pressed;
    };

    auto size = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosX(size.x-50.f*dpiScale());
    toggle(nullptr, ICON_FA_LOCK, ICON_FA_UNLOCK, lockOnItem_, "Always inspect this item");
    ImGui::SameLine();
    toggle(nullptr, ICON_FA_LINK, ICON_FA_UNLINK, lockOnView_, "Follow selection from current view only");

    ImGui::Separator();
    Vector<GraphItemPtr> inspectingItemPtrs;
    Vector<GraphItem*>   inspectingItemRawPtrs;

    auto graph = this->graph();
    if (!graph) {
      inspectingItems_.clear();
      return;
    }
    for (auto id: inspectingItems_) {
      if (auto ptr = graph->get(id)) {
        inspectingItemPtrs.push_back(ptr);
        inspectingItemRawPtrs.push_back(ptr.get());
      }
    }
    if (inspectingItemRawPtrs.size() > 0) {
      if (auto responser = editor()->responser()) {
        bool disabled = readonly();
        if (disabled)
          ImGui::BeginDisabled();
        // okay, seems quite easy to inject ... but no sane man would do that right?
        responser->onInspect(this, inspectingItemRawPtrs.data(), inspectingItemRawPtrs.size());
        if (disabled)
          ImGui::EndDisabled();
      }
    }
  }
};
// }}} Inspector View

// Message View {{{
class ImGuiMessageView: public ImGuiGraphView<ImGuiMessageView, GraphView>
{
  String tabToOpen_ = "";
public:
  ImGuiMessageView(NodeGraphEditor* editor) :
    ImGuiGraphView(editor, nullptr)
  {
    setTitle("Messages");
  }
  void onDocModified() override {}
  void onGraphModified() override {}
  Vec2 defaultSize() const override { return {600, 200}; }
  void please(StringView request) override
  {
    auto words = utils::strsplit(request, " ");
    // open XXX tab
    if (words.size() == 3 && words[0] == "open" && words[2] == "tab") {
      tabToOpen_ = String(words[1]);
    }
  }
  void drawContent()
  {
    if (ImGui::BeginTabBar("MessageHub")) {
      auto dumpMessage = [](MessageHub::Category cat){
        ImGui::PushFont(ImGuiResource::instance().monoFont);
        ImGui::BeginChild("Content", {0,0}, true);
        ImGui::PushStyleColor(ImGuiCol_Text, 0xffffffff);

        auto& textColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
        // log verbosity -> color
        static const ImVec4 colorMap[] = {
          {0.5,0.5,0.5,1.0}, // Trace
          {0.0,0.5,0.1,1.0}, // Debug
          {1.0,1.0,1.0,1.0}, // Info
          {1.0,0.5,0.1,1.0}, // Warn
          {1.0,0.0,0.0,1.0}, // Error
          {0.6,0.0,0.0,1.0}, // Fatal
          {1.0,1.0,1.0,1.0}, // Text
        };
        static_assert(sizeof(colorMap)/sizeof(*colorMap) == static_cast<int>(MessageHub::Verbosity::Count),
            "color map missmatch with verbosity count");

        MessageHub::instance().foreach(cat,
          [&textColor](auto const& msg){
            textColor = colorMap[static_cast<int>(msg.verbosity)];
            ImGui::TextUnformatted(msg.content.c_str(), msg.content.c_str()+msg.content.size());
          });
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
          ImGui::SetScrollHereY(1.0f);
        }

        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopFont();
      };
      if (ImGui::BeginTabItem("Log", nullptr, tabToOpen_ == "log" ? ImGuiTabItemFlags_SetSelected : 0)) {
        dumpMessage(MessageHub::Category::Log);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Notice", nullptr, tabToOpen_ == "notice" ? ImGuiTabItemFlags_SetSelected : 0)) {
        dumpMessage(MessageHub::Category::Notice);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Output", nullptr, tabToOpen_ == "output" ? ImGuiTabItemFlags_SetSelected : 0)) {
        dumpMessage(MessageHub::Category::Output);
        ImGui::EndTabItem();
      }
      tabToOpen_ = "";
      ImGui::EndTabBar();
    }
  }
};
// }}} Message View

// Help View {{{
class ImGuiHelpView : public ImGuiGraphView<ImGuiHelpView, GraphView>
{
public:
  ImGuiHelpView(NodeGraphEditor* editor) : ImGuiGraphView(editor, nullptr) {
    setTitle("Help");
  }
  void onDocModified() override {}
  void onGraphModified() override {}
  Vec2 defaultSize() const override { return {600, 400}; }
  void drawContent()
  {
    auto* drawlist = ImGui::GetWindowDrawList();
    auto  windowsize = ImGui::GetContentRegionAvail();
    auto* font = ImGuiResource::instance().largeSansSerifFont;
    auto* title = "NGED - a Node Graph EDitor";
    auto  fontsize = 20.f * dpiScale();
    auto  textsize = font->CalcTextSizeA(fontsize, FLT_MAX, 0.f, title);
    auto  textpos = ImVec2((windowsize / 2).x, fontsize * 1.5) - textsize / 2;
    drawlist->AddText(font, fontsize, textpos + ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin(), 0xffffffff, title);
    ImGui::SetCursorPos(ImVec2(8, textpos.y + fontsize * 4));
    //ImGui::Separator();
    if (ImGui::BeginTabBar("Tabs")) {
      if (ImGui::BeginTabItem("About")) {
        ImGui::TextUnformatted("");
        ImGui::TextUnformatted("Presented to you by iiif.");
        ImGui::TextUnformatted("");
        ImGui::TextUnformatted("With great help of following open source libraries:");
        Vector<String> libs = {"boxer", "doctest", "imgui", "lua", "miniz", "nativefiledialog", "nlohmann json", "parallel_hashmap", "parmscript", "pybind11", "python", "sol2", "spdlog", "subprocess.h", "uuid_v4"};
        for (auto&& lib: libs) {
          ImGui::BulletText("%s", lib.c_str());
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Commands")) {
        auto const& mgr = editor()->commandManager();
        if (ImGui::BeginTable("Commands##cmdtable", 4)) {
          ImGui::TableSetupColumn("Name");
          ImGui::TableSetupColumn("Description");
          ImGui::TableSetupColumn("View");
          ImGui::TableSetupColumn("Shortcut");
          ImGui::TableHeadersRow();
          for (auto&& cmd: mgr.commands()) {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(cmd->name().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(cmd->description().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(cmd->view().c_str());
            ImGui::TableNextColumn();
            auto cmdstr = Shortcut::describ(cmd->shortcut());
            ImGui::TextUnformatted(cmdstr.c_str());
          }
          ImGui::EndTable();
        }
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
};
// }}}

// Default Views {{{
class SimpleViewFactory : public ViewFactory
{
  HashMap<String, GraphViewPtr (*)(NodeGraphEditor*, NodeGraphDocPtr)> factories_;
  SimpleViewFactory(SimpleViewFactory const&) = delete;

public:
  SimpleViewFactory() = default;

  void add(String const& kind, GraphViewPtr (*factory)(NodeGraphEditor*, NodeGraphDocPtr))
  {
    factories_[kind] = factory;
  }
  GraphViewPtr createView(String const& kind, NodeGraphEditor* editor, NodeGraphDocPtr doc) const override
  {
    if (auto itr = factories_.find(kind); itr != factories_.end()) {
      if (auto viewptr = itr->second(editor, doc)) {
        ViewFactory::finalize(viewptr.get(), kind, editor);
        return viewptr;
      }
    }
    return nullptr;
  }
};

ViewFactoryPtr defaultViewFactory()
{
  auto factory = std::make_shared<SimpleViewFactory>();
  factory->add("network", [](NodeGraphEditor* editor, NodeGraphDocPtr doc) -> GraphViewPtr {
    return std::make_shared<ImGuiNetworkView>(editor, doc);
  });
  factory->add("inspector", [](NodeGraphEditor* editor, NodeGraphDocPtr doc) -> GraphViewPtr {
    return std::make_shared<ImGuiInspectorView>(editor);
  });
  factory->add("message", [](NodeGraphEditor* editor, NodeGraphDocPtr doc) -> GraphViewPtr {
    return std::make_shared<ImGuiMessageView>(editor);
  });
  factory->add("help", [](NodeGraphEditor* editor, NodeGraphDocPtr doc) -> GraphViewPtr {
    return std::make_shared<ImGuiHelpView>(editor);
  });
  return factory;
}
// }}} Default Views

// Default Responser {{{
void DefaultImGuiResponser::onInspect(InspectorView* view, GraphItem** items, size_t count)
{
  if (count == 1) {
    if (auto comment = dynamic_cast<ImGuiCommentBox*>(*items)) {
      comment->onInspect(view);
    }
  }
}
// }}} Default Responser

// ImGuiNodeGraphEditor {{{
void ImGuiNodeGraphEditor::initCommands()
{
  NodeGraphEditor::initCommands();
  commandManager().add(new FindNodeCommand(Shortcut{'/'}));
  commandManager().add(new ColorizeCommand);
  commandManager().add(new CommandManager::SimpleCommand{
    "Help/Help",
    "Help ...",
    [](GraphView* view, StringView args){ view->editor()->addView(nullptr, "help"); },
    Shortcut{0xF1}, "*"});
}

void ImGuiNodeGraphEditor::draw()
{
  mainDockID_ = ImGui::DockSpaceOverViewport();
  for (auto&& f : runOnceBeforeDraw_)
    f();
  runOnceBeforeDraw_.clear();

  for (auto view : views()) {
    if (responser())
      responser()->beforeViewDraw(view.get());

    view->draw();

    if (responser())
      responser()->afterViewDraw(view.get());
  }
}

void   ImGuiNodeGraphEditor::setClipboardText(StringView text) const
{
  ImGui::SetClipboardText(text.data());
}

String ImGuiNodeGraphEditor::getClipboardText() const
{
  if (char const* content = ImGui::GetClipboardText())
    return content;
  else
    return "";
}

struct DockLayoutNode
{
  char             split=0; // h: split horizontal, v: split vertical, 0: terminate, otherwise: undefined
  std::string_view name;
  bool             hide_tab_bar=false;
  float            weight=1.f;

  std::vector<DockLayoutNode> children;

  template <class F>
  void foreach (F&& f, DockLayoutNode const* parent = nullptr) const
  {
    f(parent, *this);
    for (auto&& child: children)
      child.foreach(f, this);
  }
};

static DockLayoutNode parseLayoutDescription(std::string_view desc)
{
  auto lines = utils::strsplit(desc, "\n");
  auto nextline = [&lines](size_t &linenb, int& indent, std::string_view& line)->bool{
    if (linenb + 1 >= lines.size())
      return false;
    for(line=lines[++linenb]; line.empty() && linenb < lines.size(); line=lines[linenb])
      ++linenb;
    if (linenb >= lines.size())
      return false;
    for(indent = 0; line[indent]==' '; ++indent)
      ;
    line = line.substr(indent);
    return true;
  };
  std::function<bool(DockLayoutNode&, size_t&, int&)> parsenode;
  parsenode = [&](DockLayoutNode& outnode, size_t& current_line, int& indent){
    int  current_indent = indent;
    auto line = lines[current_line];
    auto parts = utils::strsplit(line.substr(current_indent), ":");
    auto name = parts[0];
    char split = 0;
    if (name == "hsplit") split='h';
    else if (name == "vsplit") split='v';
    float weight = 1.f;
    if (parts.size()>1)
      std::from_chars(&parts[1].front(), &parts[1].back()+1, weight);
    bool hide_tab_bar = false;
    if (parts.size()>2 && parts[2]=="hide_tab_bar")
      hide_tab_bar = true;

    outnode = {split, name, hide_tab_bar, weight };

    if (!nextline(current_line, indent, line))
      return false;
    if (indent <= current_indent)
      return true;
    bool hasnext = true;
    while (indent > current_indent) {
      DockLayoutNode childnode;
      hasnext = parsenode(childnode, current_line, indent);
      outnode.children.push_back(childnode);
      if (!hasnext)
        break;
    }
    if (!outnode.children.empty()) {
      float sumweight = 0.f;
      for (auto&& child : outnode.children)
        sumweight += child.weight;
      for (auto& child : outnode.children)
        child.weight /= sumweight;
    }
    return hasnext;
  };
  size_t firstline = 0;
  int    initial_indent = 0;
  DockLayoutNode root;
  parsenode(root, firstline, initial_indent);
  return root;
}

NodeGraphDocPtr ImGuiNodeGraphEditor::createNewDocAndDefaultViews()
{
  auto doc = docFactory_(nodeFactory(), itemFactory());
  doc->makeRoot();
  doc->history().reset(true);
  doc->history().markSaved();
  doc->setModifiedNotifier([this](Graph* g){notifyGraphModified(g);});
  bool emptyBefore = views().size() == 0 || views().size() == pendingRemoveViews_.size();

  if (!emptyBefore) {
    GraphViewPtr netview = addView(doc, "network");
  } else {
    auto docklayout = parseLayoutDescription(defaultLayoutDesc_);
    HashMap<String, GraphViewPtr> newviews;
    docklayout.foreach ([&newviews, this, doc](DockLayoutNode const* parent, DockLayoutNode const& current) {
      String name = String(current.name);
      if (current.split==0) // leave node
        newviews[name] = addView(doc, name);
    });
    runOnceBeforeDraw_.push_back([this, docklayout=docklayout, newviews](){
      auto dockID = mainDockID_;
      ImGui::DockBuilderRemoveNode(dockID);
      ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_PassthruCentralNode|ImGuiDockNodeFlags_HiddenTabBar);
      ImGui::DockBuilderSetNodeSize(dockID, ImGui::GetWindowSize());

      HashMap<DockLayoutNode const*, ImGuiID> dockIdMap;
      dockIdMap[&docklayout] = dockID;
      docklayout.foreach (
      [&newviews, &dockIdMap](DockLayoutNode const* parent, DockLayoutNode const& current) {
        auto name = String(current.name);
        if (auto itr = dockIdMap.find(&current); itr != dockIdMap.end()) {
          auto currentid = itr->second;
          if (current.split == 0) {
            auto title = dynamic_cast<ImGuiNamedWindow*>(newviews.at(name).get())->titleWithId();
            ImGui::DockBuilderDockWindow(title.c_str(), currentid);
          } else if (current.children.size()>1) {
            auto splitdir = current.split == 'h' ? ImGuiDir_Left : ImGuiDir_Up;
            ImGuiID first, rest;
            ImGui::DockBuilderSplitNode(currentid, splitdir, current.children[0].weight, & first, &rest);
            dockIdMap[&current.children[0]] = first;
            for (int i=2, n=current.children.size(); i<n; ++i) {
              ImGuiID next;
              ImGui::DockBuilderSplitNode(rest, splitdir, current.children[i-1].weight, & next, &rest);
              dockIdMap[&current.children[i-1]] = next;
            }
            dockIdMap[&current.children.back()] = rest;
          }
        }
      });

      ImGui::DockBuilderFinish(dockID);
    });

    /*
    GraphViewPtr msgview = nullptr;
    GraphViewPtr ispview = nullptr;
    msgview = addView(doc, "message");
    ispview = addView(doc, "inspector");

    msghub::trace("dock needs to be setup ...");
    runOnceBeforeDraw_.push_back([this, netview, msgview, ispview](){
      auto netviewTitle = static_cast<ImGuiNetworkView*>(netview.get())->titleWithId();
      auto msgviewTitle = static_cast<ImGuiMessageView*>(msgview.get())->titleWithId();
      auto ispviewTitle = static_cast<ImGuiInspectorView*>(ispview.get())->titleWithId();

      msghub::trace("setting up dock ...");
      ImGuiID dockID = mainDockID_;
      ImGuiID leftID, rightID, upID, downID;
      ImGui::DockBuilderRemoveNode(dockID);
      ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_PassthruCentralNode|ImGuiDockNodeFlags_HiddenTabBar);
      ImGui::DockBuilderSetNodeSize(dockID, ImGui::GetWindowSize());
      ImGui::DockBuilderSplitNode(dockID, ImGuiDir_Up, 0.7f, &upID, &downID);
      ImGui::DockBuilderSplitNode(upID, ImGuiDir_Left, 0.7f, &leftID, &rightID);
      ImGui::DockBuilderDockWindow(netviewTitle.c_str(), leftID);
      ImGui::DockBuilderDockWindow(ispviewTitle.c_str(), rightID);
      ImGui::DockBuilderDockWindow(msgviewTitle.c_str(), downID);
      ImGui::DockBuilderGetNode(downID)->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
      ImGui::DockBuilderFinish(dockID);
    });
    */
  }
  return doc;
}

std::shared_ptr<NodeGraphEditor> newImGuiNodeGraphEditor()
{
  return std::make_shared<ImGuiNodeGraphEditor>();
}
// }}} ImGuiNodeGraphEditor

namespace detail {

// Selection State {{{
bool SelectionState::shouldEnter(NetworkView const* view) const
{
  if (!view->isHovered())
    return false;
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (view->isActive<LinkState>())
      return false;
    if (view->getState<ResizeBoxState>()->shouldEnter(view))
      return false;
    return true;
  }
  return false;
}

bool SelectionState::shouldExit(NetworkView const* view) const
{
  return !(mouseDown_ || shiftDown_ || ctrlDown_) || ImGui::IsKeyDown(ImGuiKey_Escape) ||
         view->isActive<LinkState>() || view->isActive<CreateNodeState>();
}

void SelectionState::onEnter(NetworkView* view)
{
  mouseDown_        = false;
  shiftDown_        = false;
  ctrlDown_         = false;
  isBoxDeselecting_ = false;
  isBoxSelecting_   = false;
  deselectedThisFrame_.clear();
  selectedThisFrame_      = view->selectedItems();
  confirmedItemSelection_ = view->selectedItems();
  viewSize_               = detail::vec(ImGui::GetContentRegionAvail());
}

void SelectionState::onExit(NetworkView* view)
{
  selectedThisFrame_.clear();
  deselectedThisFrame_.clear();
  confirmedItemSelection_.clear();
}

bool SelectionState::update(NetworkView* view)
{
  if (!view->isHovered() && !view->isFocused())
    return false;
  bool mouseDownNow  = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  bool mouseClicked  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

  Vec2 size = vec(ImGui::GetContentRegionAvail());
  if (size != viewSize_) { // resizing, bypass
    if (mouseReleased)
      viewSize_ = size;
    return false;
  }
  auto const s2c = view->canvas()->screenToCanvas();
  auto const pos = s2c.transformPoint(vec(ImGui::GetMousePos()));
  mousePos_      = pos;
  if (mouseDownNow && !mouseDown_) {
    boxSelectionAnchor_ = pos;
  }
  mouseDown_ = mouseDownNow;
  shiftDown_ = ImGui::GetIO().KeyMods == ImGuiMod_Shift;
  ctrlDown_  = ImGui::GetIO().KeyMods == ImGuiMod_Ctrl;
  /*
  msghub::debugf(
    "SelectionState::update: mouseDown: {} shiftDown: {}, ctrlDown: {}, mouseClicked: {},
  mouseReleased: {}", mouseDown_, shiftDown_, ctrlDown_, mouseClicked, mouseReleased);
    */

  bool isReplacingSelection = false;
  if (mouseReleased) {
    msghub::debugf("selection: confirm, {} items selected", selectedThisFrame_.size());
    confirmedItemSelection_ = selectedThisFrame_;
    view->setSelectedItems(confirmedItemSelection_);
    isBoxSelecting_ = isBoxDeselecting_ = false;
    return false;
  } else if (mouseDownNow) {
    if (ctrlDown_) {
      isBoxSelecting_   = false;
      isBoxDeselecting_ = distance2(mousePos_, boxSelectionAnchor_) > 2;
    } else {
      isBoxSelecting_   = distance2(mousePos_, boxSelectionAnchor_) > 2;
      isBoxDeselecting_ = false;
      if (!shiftDown_)
        isReplacingSelection = true;
    }
  } else {
    return false;
  }

  ItemID clickedItem  = ID_None;
  auto   selectionBox = AABB(boxSelectionAnchor_, pos);
  selectedThisFrame_  = confirmedItemSelection_;
  if (selectionBox.width() * selectionBox.height() > 4) {
    if (isReplacingSelection)
      selectedThisFrame_.clear();
    if ((isBoxSelecting_ || isBoxDeselecting_)) {
      view->graph()->forEachItem([&](GraphItemPtr item) {
        if (item->hitTest(selectionBox)) {
          if (isBoxSelecting_)
            selectedThisFrame_.insert(item->id());
          else // deselecting
            selectedThisFrame_.erase(item->id());
        }
      });
      /*
      // when box selecting, don't select links by default
      if (!selectedThisFrame_.empty()) {
        bool hasNode = false;
        for (auto id: selectedThisFrame_) {
          if (auto item = view->graph()->get(id))
            if (item->asNode())
              hasNode = true;
        }
        if (hasNode) {
          for (auto id: selectedThisFrame_) {
            if (auto item = view->graph()->get(id))
              if (item->asLink())
                selectedThisFrame_.erase(id);
          }
        }
      }
      */
    }
  }

  if (mouseClicked) {
    clickedItem = view->hoveringItem();
  }

  if (view->hoveringItem() == ID_None && mouseClicked && isReplacingSelection) {
    // click on blank area deselects all
    confirmedItemSelection_.clear();
    view->setSelectedItems({});
    msghub::debug("selection: deselected all");
  } else if (clickedItem != ID_None) {
    if (shiftDown_) {
      confirmedItemSelection_.insert(clickedItem);
    } else if (ctrlDown_) {
      confirmedItemSelection_.erase(clickedItem);
    } else {
      confirmedItemSelection_ = {clickedItem};
    }
    selectedThisFrame_ = confirmedItemSelection_;
    msghub::debugf("selection: clicked on {}", clickedItem.value());
  }
  for (auto id : view->selectedItems()) {
    if (selectedThisFrame_.find(id) == selectedThisFrame_.end())
      deselectedThisFrame_.insert(id);
  }
  for (auto id : selectedThisFrame_)
    view->hideItemOnce(id);
  for (auto id : deselectedThisFrame_)
    view->hideItemOnce(id);
  isUpdating_ = true;
  return false;
}

void SelectionState::draw(NetworkView* view)
{
  if (!isUpdating_)
    return;

  auto* canvas = view->canvas();
  canvas->pushLayer(Canvas::Layer::High);
  for (auto id : deselectedThisFrame_) {
    view->graph()->get(id)->draw(canvas, GraphItemState::DESELECTED);
  }
  for (auto id : selectedThisFrame_) {
    view->graph()->get(id)->draw(canvas, GraphItemState::SELECTED);
  }
  canvas->popLayer();

  if (view->isActive<MoveState>())
    return;
  // draw (de)selection box
  canvas->pushLayer(Canvas::Layer::Low);
  AABB selbox(boxSelectionAnchor_);
  selbox.merge(mousePos_);
  if (isBoxSelecting_) {
    auto style = Canvas::ShapeStyle{
      true,                                       // filled
      UIStyle::instance().selectionBoxBackground, // fillColor
      0.f,                                        // strokeWidth
      0                                           // strokeColor
    };
    canvas->drawRect(selbox.min, selbox.max, 0, style);
  } else if (isBoxDeselecting_) {
    auto style = Canvas::ShapeStyle{
      true,                                         // filled
      UIStyle::instance().deselectionBoxBackground, // fillColor
      0.f,                                          // strokeWidth
      0                                             // strokeColor
    };
    canvas->drawRect(selbox.min, selbox.max, 0, style);
  }
  canvas->popLayer();
}

void SelectionState::onGraphModified(NetworkView* view)
{
  HashSet<ItemID> validSelection;
  for (auto id : confirmedItemSelection_) {
    if (view->graph()->get(id)) {
      validSelection.insert(id);
    }
  }
  confirmedItemSelection_.swap(validSelection);
  validSelection.clear();
  for (auto id : selectedThisFrame_) {
    if (view->graph()->get(id)) {
      validSelection.insert(id);
    }
  }
  selectedThisFrame_.swap(validSelection);
}
// }}}

// HandleView {{{
bool HandleView::update(NetworkView* view)
{
  using gmath::clamp;
  if (!view->isHovered())
    return false;

  bool  buttonDownNow     = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  bool  buttonReleasedNow = ImGui::IsMouseReleased(ImGuiMouseButton_Middle);
  auto  mouseScreenPos    = vec(ImGui::GetMousePos());
  auto* canvas            = view->canvas();

  auto       s2c          = canvas->screenToCanvas();
  auto       mousepos     = s2c.transformPoint(mouseScreenPos);
  GraphItem* hoveringItem = nullptr;
  view->setHoveringItem(ID_None);
  view->setHoveringPin(PIN_None);
  view->graph()->forEachItem([&mousepos, &hoveringItem, view](GraphItemPtr item) {
    auto biggerBound = item->aabb().expanded(8.f);
    if (biggerBound.contains(mousepos)) {
      if (auto* node = item->asNode()) {
        if (AABB bb; node->mergedInputBound(bb)) {
          if (bb.contains(mousepos)) {
            view->setHoveringPin({item->id(), -1, NodePin::Type::In});
          }
        } else {
          auto ic = node->numMaxInputs();
          assert(ic >= 0 && ic < 100);
          for (int i = 0; i < ic; ++i) {
            if (
              distance(node->inputPinPos(i), mousepos) < UIStyle::instance().nodePinRadius * 1.5f) {
              view->setHoveringPin({item->id(), i, NodePin::Type::In});
              break;
            }
          }
        }

        auto oc = node->numOutputs();
        for (int i = 0; i < oc; ++i) {
          if (
            distance(node->outputPinPos(i), mousepos) < UIStyle::instance().nodePinRadius * 1.5f) {
            view->setHoveringPin({item->id(), i, NodePin::Type::Out});
            break;
          }
        }
      }
      if (
        view->hoveringPin().node == ID_None &&
        item->hitTest(mousepos) &&
        (hoveringItem == nullptr || view->zCompare(hoveringItem, item.get()) <= 0)) {
        hoveringItem = item.get();
        view->setHoveringItem(item->id());
      }
    }
  });
  // scan again for routers, routers have higher priority, otherwise they will likely be blocked by
  // links
  view->graph()->forEachItem([&mousepos, view](GraphItemPtr item) {
    if (auto* router = item->asRouter()) {
      if (router->hitTest(mousepos)) {
        view->setHoveringItem(item->id());
      }
    }
  });

  if (view->hoveringItem() != ID_None) {
    auto item = view->graph()->get(view->hoveringItem());
    if (auto* resp = view->editor()->responser()) {
      int button = -1;
      bool click = false;
      bool dbclick = false;
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        button = 0;
      else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        button = 1;
      else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        button = 2;
      else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        button = 0;
        dbclick = true;
      } else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
        button = 1;
        dbclick = true;
      } else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle)) {
        button = 2;
        dbclick = true;
      }
      if (button>=0) {
        if (dbclick)
          resp->onItemDoubleClicked(view, item.get(), button);
        else
          resp->onItemClicked(view, item.get(), button);
      } else {
        resp->onItemHovered(view, item.get());
      }
    }
  }

  if (!panButtonDown_ && buttonDownNow &&
      (view->hoveringItem() == ID_None ||
       view->graph()->get(view->hoveringItem())->asGroupBox())) {
    mouseAnchor_ = vec(ImGui::GetMousePos());
    viewAnchor_  = canvas->viewPos();
    canPan_      = true;
  } else if (buttonReleasedNow) {
    canPan_ = false;
  }
  panButtonDown_ = buttonDownNow;

  if (panButtonDown_ && canPan_) {
    if (auto delta = s2c.transformPoint(mouseAnchor_) - mousepos;
        length2(delta * canvas->viewScale()) > 1) {
      auto vp = viewAnchor_ + delta * canvas->viewScale();
      canvas->setViewPos(vp);
    }
    return true;
  } else if (auto wheel = ImGui::GetIO().MouseWheel; abs(wheel) > 0.1) {
    auto center = canvas->screenToCanvas().transformPoint(mouseScreenPos);
    auto scale  = canvas->viewScale();
    scale       = clamp(scale + wheel / 20.f, 0.02f, 10.f);
    canvas->setViewScale(scale);
    auto newcenter = canvas->screenToCanvas().transformPoint(mouseScreenPos);
    canvas->setViewPos(canvas->viewPos() - (newcenter - center) * scale);
    return true;
  } else if (ImGui::IsKeyPressed(ImGuiKey_F)) { // focus selected or home all if none selected
    view->zoomToSelected(0.2f);
  }
  return false;
}

void HandleView::draw(NetworkView* view)
{
  if (auto pin = view->hoveringPin(); pin != PIN_None) {
    auto* node = view->graph()->get(pin.node)->asNode();
    assert(node);
    Vec2               pos   = view->graph()->pinPos(pin);
    Canvas::ShapeStyle style = {true, toUint32RGBA(view->graph()->pinColor(pin)), 0.f, 0x0};
    view->canvas()->drawCircle(pos, UIStyle::instance().nodePinRadius*1.5f, 0, style);
  }
}
// }}} HandleView

// MoveState {{{
bool MoveState::shouldEnter(NetworkView const* view) const
{
  if (view->readonly())
    return false;
  auto hovering = view->graph()->get(view->hoveringItem());
  return view->isFocused() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
         ImGui::GetIO().KeyMods == 0 && hovering &&
         !hovering->asLink() &&
         !view->getState<ResizeBoxState>()->shouldEnter(view);
}

void MoveState::onEnter(NetworkView* view)
{
  auto const s2c = view->canvas()->screenToCanvas();
  anchor_        = s2c.transformPoint(vec(ImGui::GetMousePos()));
  if (view->selectedItems().find(view->hoveringItem()) == view->selectedItems().end())
    itemsToMove_ = {view->hoveringItem()};
  else
    itemsToMove_ = view->selectedItems();

  auto graph = view->graph();
  Vector<GroupBox*> groups;
  for (auto id: itemsToMove_) {
    if (auto* group = graph->get(id)->asGroupBox()) {
      groups.push_back(group);
    }
  }
  while (!groups.empty()) {
    auto* group = groups.back();
    groups.pop_back();
    for (auto iid: group->containingItems()) {
      itemsToMove_.insert(iid);
      if (auto* anothergroup = graph->get(iid)->asGroupBox())
        groups.push_back(anothergroup);
    }
  }
  done_            = false;
  moved_           = false;
  movedSinceEnter_ = false;
}

void MoveState::onExit(NetworkView* view)
{
  if (movedSinceEnter_)
    view->doc()->history().commitIfAppropriate("moved items");

  ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
}

bool MoveState::update(NetworkView* view)
{
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    done_ = true;
  
  if (!view->isFocused())
    return false;

  ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

  auto const s2c = view->canvas()->screenToCanvas();
  if (!done_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    auto posnow = s2c.transformPoint(vec(ImGui::GetMousePos()));
    auto delta  = posnow - anchor_;
    if (length2(delta) >= 1) {
      if (!view->graph()->move(itemsToMove_, delta)) {
        moved_ = false;
        done_  = true;
      } else {
        anchor_          = posnow;
        moved_           = true;
        movedSinceEnter_ = true;
      }
    }
  } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    view->editor()->confirmItemPlacements(view->graph().get(), itemsToMove_);
  }

  // if the movement has started, block other messages
  return moved_;
}
// }}} MoveState

// EditArrow {{{
bool EditArrow::shouldEnter(NetworkView const* view) const
{
  if (view->readonly())
    return false;
  return ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
    view->hoveringItem() != ID_None &&
    dynamic_cast<Arrow*>(view->graph()->get(view->hoveringItem()).get());
}
bool EditArrow::shouldExit(NetworkView const* view) const
{
  if (view->readonly())
    return true;
  if (editHandle_ == EditHandle::NONE)
    return true;
  auto arrow = editingArrow_.lock();
  if (!arrow)
    return true;

  return false;
}

void EditArrow::onEnter(NetworkView* view)
{
  msghub::debug("EditArrow::onEnter");
  mousePos_ = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
  auto itemptr = view->graph()->get(view->hoveringItem());
  if (!dynamic_cast<Arrow*>(itemptr.get())) {
    editHandle_ = EditHandle::NONE;
    return;
  }
  editingArrow_ = std::static_pointer_cast<Arrow>(itemptr);
  editHandle_ = EditHandle::SEGMENT;
}

void EditArrow::onExit(NetworkView* view)
{
  editingArrow_.reset();
  editHandle_ = EditHandle::NONE;
}

bool EditArrow::update(NetworkView* view)
{
  auto arrow = editingArrow_.lock();
  if (!arrow)
    return false;
  mousePos_ = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));

  bool const mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  bool const mouseDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);

  if (mouseDown && !mouseClicked) {
    if (editHandle_ == EditHandle::START_POINT) {
      arrow->setStart(mousePos_);
      return true;
    } else if (editHandle_ == EditHandle::END_POINT) {
      arrow->setEnd(mousePos_);
      return true;
    } else if (editHandle_ == EditHandle::SEGMENT) {
      return false;
    }
  }
  Vec2 nearpt;
  float mouseDist = pointSegmentDistance(mousePos_, arrow->start(), arrow->end(), &nearpt);
  auto arrowlen = distance(arrow->start(), arrow->end());
  if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    editHandle_ = EditHandle::NONE;
  else if (mouseClicked) {
    if (distance(mousePos_, arrow->start()) <= 6.f)
      editHandle_ = EditHandle::START_POINT;
    else if (distance(mousePos_, arrow->end()) <= 6.f)
      editHandle_ = EditHandle::END_POINT;
    else if (mouseDist > arrow->thickness()*1.2f+3.f)
      editHandle_ = EditHandle::NONE;
    else 
      editHandle_ = EditHandle::SEGMENT; 
  }
  return false;
}

void EditArrow::draw(NetworkView* view)
{
  auto arrow = editingArrow_.lock();
  if (!arrow)
    return;
  auto const solid = Canvas::ShapeStyle {
    true, 0xffffffff,
    0, 0 };
  auto const holo = Canvas::ShapeStyle {
    false, 0,
    1.4f, 0xffffffff };
  auto startstyle = holo, endstyle = holo;
  if (editHandle_ == EditHandle::START_POINT) {
    startstyle = solid;
  } else if (editHandle_ == EditHandle::END_POINT) {
    endstyle = solid;
  }
  view->canvas()->drawCircle(arrow->start(), 5, 0, startstyle);
  view->canvas()->drawCircle(arrow->end(), 5, 0, endstyle);
}

void EditArrow::onGraphModified(NetworkView* view)
{
  if (editingArrow_.expired())
    editingArrow_.reset();
}
// }}} EditArrow

// Resize Box {{{
bool ResizeBoxState::shouldEnter(NetworkView const* view) const
{
  if (view->readonly())
    return false;
  auto mousepos = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
  auto hovering = view->hoveringItem();
  auto graph = view->graph();
  GraphItem* topgroup = nullptr;
  if (hovering == ID_None) {
    for (auto id: graph->items()) {
      if (auto* group = graph->get(id)->asGroupBox()) {
        if (group->aabb().contains(mousepos)) {
          if (topgroup == nullptr || view->zCompare(topgroup, group) < 0) {
            hovering = group->id();
            topgroup = group;
          }
        }
      }
    }
  }
  if (auto item = graph->get(hovering)) {
    if (auto* resizable = item->asResizable()) {
      auto aabb = resizable->aabb();
      uint8_t nearLeft   = abs(mousepos.x-aabb.min.x) < 4.f ? 1 : 0;
      uint8_t nearRight  = abs(mousepos.x-aabb.max.x) < 4.f ? 2 : 0;
      uint8_t nearTop    = abs(mousepos.y-aabb.min.y) < 4.f ? 4 : 0;
      uint8_t nearBottom = abs(mousepos.y-aabb.max.y) < 4.f ? 8 : 0;
      ResizingLocation location;
      switch (nearLeft|nearRight|nearTop|nearBottom) {
        case 1:   location = Left; break;
        case 2:   location = Right; break;
        case 4:   location = Top; break;
        case 8:   location = Bottom; break;
        case 1|4: location = TopLeft; break;
        case 1|8: location = BottomLeft; break;
        case 2|4: location = TopRight; break;
        case 2|8: location = BottomRight; break;
        default:  location = Nowhere; break;
      }
      updateCursor(location);

      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && location != Nowhere) {
        const_cast<ResizeBoxState*>(this)->activate(item, location);
        return true;
      }
    }
  }
  return false;
}

bool ResizeBoxState::shouldExit(NetworkView const* view) const
{
  if (view->readonly())
    return true;
  if (!view->isFocused() || ImGui::IsMouseReleased(ImGuiMouseButton_Left) || resizingItem_ == ID_None || resizingWhere_ == Nowhere)
    return true;
  else
    return false;
}

void ResizeBoxState::activate(GraphItemPtr item, ResizeBoxState::ResizingLocation where)
{
  auto* self = const_cast<ResizeBoxState*>(this);
  self->resizingItem_  = item->id();
  self->resizingWhere_ = where;
  self->resizingBox_   = item->aabb();
}

void ResizeBoxState::onEnter(NetworkView* view)
{
  resized_ = false;
}

void ResizeBoxState::onExit(NetworkView* view)
{
  ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
  if (resized_)
    view->graph()->docRoot()->history().commitIfAppropriate("resize");
}

void ResizeBoxState::updateCursor(ResizeBoxState::ResizingLocation location) const
{
  static constexpr ImGuiMouseCursor cursors[] = {
    ImGuiMouseCursor_Arrow,
    ImGuiMouseCursor_ResizeNS,   // top
    ImGuiMouseCursor_ResizeNESW, // topright
    ImGuiMouseCursor_ResizeEW,   // right
    ImGuiMouseCursor_ResizeNWSE, // bottomright
    ImGuiMouseCursor_ResizeNS,   // bottom
    ImGuiMouseCursor_ResizeNESW, // bottomleft
    ImGuiMouseCursor_ResizeEW,   // left
    ImGuiMouseCursor_ResizeNWSE, // topleft
  };
  ImGui::SetMouseCursor(cursors[location]);
}

bool ResizeBoxState::update(NetworkView* view)
{
  updateCursor(resizingWhere_);
  auto mousepos = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
  switch (resizingWhere_) {
    case Top:
      resizingBox_.min.y = mousepos.y; break;
    case TopRight:
      resizingBox_.max.x = mousepos.x;
      resizingBox_.min.y = mousepos.y;
      break;
    case Right:
      resizingBox_.max.x = mousepos.x; break;
    case BottomRight:
      resizingBox_.max = mousepos; break;
    case Bottom:
      resizingBox_.max.y = mousepos.y; break;
    case BottomLeft:
      resizingBox_.max.y = mousepos.y;
      resizingBox_.min.x = mousepos.x;
      break;
    case Left:
      resizingBox_.min.x = mousepos.x; break;
    case TopLeft:
      resizingBox_.min = mousepos; break;
    default:
      break;
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (auto item = view->graph()->get(resizingItem_)) {
      if (auto* resizer = item->asResizable()) {
        AABB bb;
        bb.merge(resizingBox_.min);
        bb.merge(resizingBox_.max);
        resizer->setBounds(bb);
      }
    }
  }
  return true;
}

void ResizeBoxState::draw(NetworkView* view)
{
  auto style = Canvas::ShapeStyle {
    false, 0,
    2.f, 0x88888888,
  };
  view->canvas()->pushLayer(Canvas::Layer::Lower);
  view->canvas()->drawRect(resizingBox_.min, resizingBox_.max, 0, style);
  view->canvas()->popLayer();
}
// }}} Resize Box

// LinkState {{{
bool LinkState::activate(NodePin source, NodePin dest)
{
  if (active()) {
    msghub::warn("trying to enter link state while already in");
    return false;
  }

  if (source == PIN_None && dest == PIN_None)
    return false;

  srcPin_          = source;
  dstPin_          = dest;
  manualActivated_ = true;

  return true;
}

void LinkState::onEnter(NetworkView* view)
{
  if (manualActivated_) {
    // pass
  } else if (auto pin = view->hoveringPin(); pin != PIN_None) {
    if (pin.type == NodePin::Type::Out) {
      srcPin_ = view->hoveringPin();
      dstPin_ = PIN_None;
    } else {
      srcPin_ = PIN_None;
      dstPin_ = view->hoveringPin();
    }
  } else {
    Link* link = nullptr;
    if (auto item = view->graph()->get(view->hoveringItem()))
      link = item->asLink();
    if (link == nullptr && pendingEnter_)
      if (auto item = view->graph()->get(pendingLinkID_))
        link = item->asLink();
    if (!link)
      msghub::error("no link to drag");
    auto out = link->input();
    auto in  = link->output();
    srcPin_  = {out.sourceItem, out.sourcePort, NodePin::Type::Out};
    dstPin_  = {in.destItem, in.destPort, NodePin::Type::In};
  }
  hiddenLink_ = ID_None;
  if (dstPin_ != PIN_None) {
    if (auto link = view->graph()->getLink(dstPin_.node, dstPin_.pin))
      hiddenLink_ = link->id();
    else
      hiddenLink_ = ID_None;
    view->hideItem(hiddenLink_);
  }
  manualActivated_ = false;
  pendingEnter_    = false;
  outPath_.clear();
  inPath_.clear();
}

bool LinkState::update(NetworkView* view)
{
  pos_               = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
  auto const dropPin = view->hoveringPin();

  if (srcPin_ != PIN_None) {
    if (dropPin.type == NodePin::Type::Out) {
      outPath_.clear();
    } else {
      auto startPos = view->graph()->pinPos(srcPin_);
      AABB startBB  = view->graph()->get(srcPin_.node)->aabb();
      AABB midBB    = {{0, 0}};
      outPath_ = view->graph()->calculatePath(startPos, pos_, {0, 1}, {0, -1}, startBB, midBB);
    }
  }
  if (dstPin_ != PIN_None) {
    if (dropPin.type == NodePin::Type::In) {
      inPath_.clear();
    } else {
      auto endPos = view->graph()->pinPos(dstPin_);
      AABB midBB  = {{0, 0}};
      AABB endBB  = view->graph()->get(dstPin_.node)->aabb();
      inPath_     = view->graph()->calculatePath(pos_, endPos, {0, 1}, {0, -1}, midBB, endBB);
    }
  }

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    auto* resp      = view->editor()->responser();
    bool  shiftDown = ImGui::GetIO().KeyMods == ImGuiMod_Shift;
    if (srcPin_ != PIN_None && dropPin.type == NodePin::Type::In && dropPin.node != srcPin_.node) {
      auto srcNode = srcPin_.node;
      auto srcPin  = srcPin_.pin;
      auto dstNode = dropPin.node;
      auto dstPin  = dropPin.pin;
      if (shiftDown) {
        view->editor()->swapOutput(view->graph().get(), srcNode, srcPin, dstPin_.node, dstPin_.pin, dstNode, dstPin);
      } else {
        if (dstPin_ != PIN_None) {
          view->editor()->removeLink(view->graph().get(), dstPin_.node, dstPin_.pin);
        }
        view->editor()->setLink(view->graph().get(), view, srcNode, srcPin, dstNode, dstPin);
      }
    }
    if (
      dstPin_ != PIN_None && dropPin.type == NodePin::Type::Out && dropPin.node != dstPin_.node) {
      auto srcNode = dropPin.node;
      auto srcPin  = dropPin.pin;
      auto dstNode = dstPin_.node;
      auto dstPin  = dstPin_.pin;
      if (shiftDown) {
        view->editor()->swapInput(view->graph().get(), srcPin_.node, srcPin_.pin, srcNode, srcPin, dstNode, dstPin);
      } else {
        view->editor()->setLink(view->graph().get(), view, srcNode, srcPin, dstNode, dstPin);
      }
    }
    if (dropPin == PIN_None) {
      auto dropItemID = view->hoveringItem();
      auto item       = view->graph()->get(dropItemID);
      if (item && item->asNode()) {
        auto* node = item->asNode();
        if (srcPin_ != PIN_None) {
          auto pin = node->getPinForIncomingLink(srcPin_.node, srcPin_.pin);
          view->editor()->setLink(view->graph().get(), view, srcPin_.node, srcPin_.pin, dropItemID, pin);
        }
        if (dstPin_ != PIN_None) {
          if (node->numOutputs() > 0)
            view->editor()->setLink(view->graph().get(), view, dropItemID, 0, dstPin_.node, dstPin_.pin);
        }
      } else if (item && item->asRouter()) {
        auto* router = item->asRouter();
        if (srcPin_ != PIN_None)
          view->editor()->setLink(view->graph().get(), view, srcPin_.node, srcPin_.pin, dropItemID, 0);
        if (dstPin_ != PIN_None)
          view->editor()->setLink(view->graph().get(), view, dropItemID, 0, dstPin_.node, dstPin_.pin);
      }
    }
    return true;
  }
  return false;
}

void LinkState::draw(NetworkView* view)
{
  Canvas::ShapeStyle style = {
    false, UIStyle::instance().linkDefaultColor, 1.f, UIStyle::instance().linkDefaultColor};
  if (!outPath_.empty())
    view->canvas()->drawPoly(outPath_.data(), outPath_.size(), false, style);
  if (!inPath_.empty())
    view->canvas()->drawPoly(inPath_.data(), inPath_.size(), false, style);
  style.filled      = true;
  style.strokeWidth = 0;
  view->canvas()->drawCircle(pos_, 3.f, 0, style);
}

void LinkState::onExit(NetworkView* view)
{
  if (view->hoveringItem() == ID_None && view->hoveringPin() == PIN_None) {
    if (auto createNode = view->getState<CreateNodeState>())
      createNode->activate({ srcPin_.node, srcPin_.pin }, { dstPin_.node, dstPin_.pin });
  } else {
    if (hiddenLink_ != ID_None)
      view->unhideItem(hiddenLink_);
    hiddenLink_ = ID_None;
    srcPin_ = PIN_None;
    dstPin_ = PIN_None;
    outPath_.clear();
    inPath_.clear();
  }
  manualActivated_ = false;
  pendingEnter_    = false;
}
// }}} LinkState

// CutLinkState {{{
void CutLinkState::onEnter(NetworkView* view)
{
  done_ = false;
  stroke_.clear();
  stroke_.push_back(view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos())));
}

void CutLinkState::onExit(NetworkView* view)
{
  done_ = false;
  stroke_.clear();
}

bool CutLinkState::update(NetworkView* view)
{
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !ImGui::IsKeyDown(ImGuiKey_Y) || !view->isFocused())
    done_ = true;
  auto newpos = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
  if (distance2(newpos, stroke_.back())>2.f)
    stroke_.push_back(newpos);
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) { // confirm
    HashSet<ItemID> linksToRemove;
    AABB strokeBounds;
    for (auto&& p: stroke_)
      strokeBounds.merge(p);
    view->graph()->forEachLink([this, &linksToRemove, strokeBounds](LinkPtr link){
      if (link->aabb().intersects(strokeBounds)) {
        if (gmath::strokeIntersects(stroke_, link->path()))
          linksToRemove.insert(link->id());
      }
    });
    view->editor()->removeItems(view->graph().get(), linksToRemove);
    done_ = true;
  }
  return true;
}

void CutLinkState::draw(NetworkView* view)
{
  auto style = Canvas::ShapeStyle{
    false, 0,
    3.f, 0xff0000ff
  };
  view->canvas()->drawPoly(stroke_.data(), stroke_.size(), false, style);
}
// }}} CutLinkState

// Shortcuts {{{
bool HandleShortcut::update(NetworkView* view)
{
  if (!view->isHovered())
    return false;
  auto tryEnter = [view](ItemID id) {
    if (auto item = view->graph()->get(id)) {
      if (auto* node = item->asNode()) {
        if (auto* subgraph = node->asGraph()) {
          msghub::debugf("entering subgraph {}", static_cast<void*>(subgraph));
          if (ImGui::GetIO().KeyMods == ImGuiMod_Shift) {
            if (auto network = view->editor()->addView(view->doc(), "network")) {
              network->reset(std::static_pointer_cast<Graph>(subgraph->shared_from_this()));
            }
          } else {
            view->reset(std::static_pointer_cast<Graph>(subgraph->shared_from_this()));
          }
        }
      }
    }
  };
  auto modKeys = ImGui::GetIO().KeyMods;

  GraphItemPtr solyLinkableItem = nullptr;
  for (auto id: view->selectedItems())
    if (auto item = view->graph()->get(id))
      if (item->asNode() && item->asNode()->numOutputs() != 0 || item->asRouter())
        if (solyLinkableItem) { // not the only one
          solyLinkableItem = nullptr;
          break;
        } else {
          solyLinkableItem = item;
        }
  if (view->hoveringItem() != ID_None && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    tryEnter(view->hoveringItem());
  } else if (solyLinkableItem && ImGui::IsKeyPressed(ImGuiKey_Enter) && !view->readonly()) {
    if (modKeys == ImGuiMod_Ctrl) {
      if (auto anim = view->getState<AnimationState>()) {
        auto const mousepos =
          view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
        auto const delta = solyLinkableItem->pos() - mousepos;
        anim->animateTo(view->canvas(), view->canvas()->viewPos() + delta + Vec2{0, 90}, 1.f);
      }
      if (auto link = view->getState<LinkState>()) {
        if (link->activate({solyLinkableItem->id(), 0, NodePin::Type::Out}))
          link->onEnter(view);
      }
      if (auto create = view->getState<CreateNodeState>()) {
        create->activate();
      }
    } else {
      tryEnter(solyLinkableItem->id());
    }
  }
  return false;
}
// }}} Shortcuts

// CreateNodeState {{{
bool CreateNodeState::shouldEnter(NetworkView const* view) const
{
  if (view->readonly())
    return false;
  if (!view->isHovered())
    return false;
  if (ImGui::IsKeyPressed(ImGuiKey_Tab))
    return true;
  if (manualActivated_)
    return true;
  if (view->hoveringItem() == ID_None && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    return true;
  if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
    if (view->selectedItems().size() == 1)
      if (auto* link = view->graph()->get(*view->selectedItems().begin())->asLink()) {
        pendingInputLink_ = link->input();
        pendingOutputLink_ = link->output();
        manualActivated_ = true;
        return true;
      }
  }
  return false;
}

bool CreateNodeState::shouldExit(NetworkView const* view) const
{
  if (view->readonly())
    return true;
  return (isConfirmed_ && isPlaced_) || ImGui::IsKeyPressed(ImGuiKey_Escape);
}

void CreateNodeState::onEnter(NetworkView* view)
{
  input_ = "";
  orderedMatches_.clear();
  confirmedNodeType_ = "";
  confirmedItemType_ = "";
  isConfirmed_       = false;
  isPlaced_          = false;
  hiddenLink_        = ID_None;
  msghub::debug("entering create node state");
  if (manualActivated_) {
    // pass
    // pendingInputLink_ = {manualSourceID_, 0};
  } else {
    pendingInputLink_  = {ID_None, -1};
    pendingOutputLink_ = {ID_None, -1};
    if (auto linkState = view->getState<LinkState>()) {
      if (auto pin = linkState->srcPin(); pin != PIN_None) {
        pendingInputLink_ = {pin.node, pin.pin};
      }
      if (auto pin = linkState->dstPin(); pin != PIN_None) {
        pendingOutputLink_ = {pin.node, pin.pin};
      }
    }
  }

  if (auto link = view->graph()->getLink(pendingOutputLink_.destItem, pendingOutputLink_.destPort))
    hiddenLink_ = link->id();
  else
    hiddenLink_ = ID_None;
  view->hideItem(hiddenLink_);
  if (auto linkState = view->getState<LinkState>())
    linkState->clear();

  msghub::debugf(
    "create node with input = {}[{}], output = {}[{}]",
    pendingInputLink_.sourceItem.value(),
    pendingInputLink_.sourcePort,
    pendingOutputLink_.destItem.value(),
    pendingOutputLink_.destPort);
  manualActivated_ = false;
  ImGui::OpenPopup("CreateNode");
}

void CreateNodeState::onExit(NetworkView* view)
{
  ImGui::CloseCurrentPopup();
  if (pendingItemToPlace_ && !isPlaced_) {
    if (pendingItemToPlace_->asNode()) {
      auto* nodeFactory = view->graph()->nodeFactory();
      nodeFactory->discard(view->graph().get(), pendingItemToPlace_->asNode());
    } else {
      auto  itemFactory = view->editor()->itemFactory();
      itemFactory->discard(view->graph().get(), pendingItemToPlace_.get());
    }
  }
  pendingItemToPlace_ = nullptr;
  pendingInputLink_   = {ID_None, -1};
  if (hiddenLink_ != ID_None)
    view->unhideItem(hiddenLink_);
  pendingOutputLink_ = {ID_None, -1};
  if (auto linkState = view->getState<LinkState>()) {
    linkState->clear();
  }
}

bool CreateNodeState::update(NetworkView* view)
{
  auto* nodeFactory = view->graph()->nodeFactory();
  auto  itemFactory = view->editor()->itemFactory();
  if (!isConfirmed_) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(800, 1024));
    if (ImGui::BeginPopup("CreateNode")) {
      ImGui::PushItemWidth(-1);

      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(0);
      String newInput = input_;
      isConfirmed_ =
        ImGui::InputText("##nodeClass", &newInput, ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::Separator();

      auto itemNames = itemFactory->listNames();
      if (newInput != input_ || (orderedMatches_.empty())) {
        input_ = newInput;
        orderedMatches_.clear();
        if (!input_.empty()) {
          for (String const& name : itemNames) {
            int score = 0;
            if (helper::fuzzy_match(input_, name, score)) {
              orderedMatches_.insert({score, {MatchItem::ITEM, name, name}});
            }
          }
          nodeFactory->listNodeTypes(
            view->graph().get(), this,
            [](void* ctx, StringView cat, StringView type, StringView name) {
              auto* state  = static_cast<CreateNodeState*>(ctx);
              int   score = 0;
              if (helper::fuzzy_match(state->input_, name, score)) {
                state->orderedMatches_.insert({score, {MatchItem::NODE, String(type), String(name)}});
              }
            });
        } else {
          // default order
          tempCounter_ = 0;
          for (String const& name : itemNames) {
            orderedMatches_.insert({tempCounter_--, {MatchItem::ITEM, name, name}});
          }
          nodeFactory->listNodeTypes(
            view->graph().get(), this,
            [](void* ctx, StringView cat, StringView type, StringView name) {
              auto* state = static_cast<CreateNodeState*>(ctx);
              state->orderedMatches_.insert({state->tempCounter_, {MatchItem::NODE, String(type), String(name)}});
              --state->tempCounter_;
            });
        }
      }
      for (auto itr = orderedMatches_.begin(); itr != orderedMatches_.end(); ++itr) {
        if (itr->second.kind==MatchItem::ITEM)
          ImGui::PushStyleColor(ImGuiCol_Text, 0xFF4796D3);
        String menustr = itr->second.name; //fmt::format("{} ({})", itr->second.name, itr->second.type);
        if (
          ImGui::MenuItem(menustr.c_str(), nullptr) ||
          (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
          isConfirmed_ = true;
          if (itr->second.kind==MatchItem::ITEM) {
            confirmedItemType_ = itr->second.type;
            confirmedNodeType_ = input_ = "";
          } else {
            confirmedItemType_ = "";
            confirmedNodeType_ = input_ = itr->second.type;
          }
        }
        if (itr->second.kind==MatchItem::ITEM)
          ImGui::PopStyleColor();
      }
      if (isConfirmed_ && confirmedNodeType_ == "" && confirmedItemType_ == "") {
        if (ImGui::GetIO().KeyMods != ImGuiMod_Ctrl) {
          if (!orderedMatches_.empty()) {
            auto itr = orderedMatches_.begin();
            if (itr->second.kind==MatchItem::ITEM) {
              confirmedItemType_ = itr->second.type;
              confirmedNodeType_ = input_ = "";
            } else {
              confirmedItemType_ = "";
              confirmedNodeType_ = input_ = itr->second.type;
            }
          }
        }
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        input_             = "";
        confirmedNodeType_ = "";
        confirmedItemType_ = "";
        isConfirmed_       = true;
      }

      ImGui::PopItemWidth();
      ImGui::EndPopup();
    } else {
      isConfirmed_ = true;
    }
    ImGui::PopStyleVar();
  }
  if (isConfirmed_ && !isPlaced_) {
    bool justCreated = false;
    if (confirmedNodeType_ == "" && confirmedItemType_ == "" && input_ == "") {
      isPlaced_ = true;
      return false;
    } else if (pendingItemToPlace_ == nullptr) {
      GraphItemPtr item = nullptr;
      if (!confirmedNodeType_.empty()) {
        item = NodePtr(nodeFactory->createNode(view->graph().get(), confirmedNodeType_));
      } else if (!confirmedItemType_.empty()) {
        item = itemFactory->make(view->graph().get(), confirmedItemType_);
      } else {
        item = NodePtr(nodeFactory->createNode(view->graph().get(), input_));
      }
      if (!item) {
        isPlaced_ = true;
        return false;
      } else {
        pendingItemToPlace_ = item;
        justCreated         = true;
      }
    }

    if (pendingItemToPlace_) {
      pendingItemToPlace_->moveTo(
        view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos())));

      if (
        !justCreated &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
        GraphItem* replacement = nullptr;
        if (view->editor()->responser() &&
            !view->editor()->responser()->beforeItemAdded(view->graph().get(), pendingItemToPlace_.get(), &replacement)) {
          auto text = 
            pendingItemToPlace_->asNode()?
              fmt::format("{} cannot be placed here",
                  pendingItemToPlace_->asNode()->name()):
              fmt::format("{} cannot be placed here",
                  view->doc()->itemFactory()->factoryName(pendingItemToPlace_));
          view->addFadingText(text, pendingItemToPlace_->pos());
          return true;
        }
        if (replacement) {
          if (auto* nodeptr = pendingItemToPlace_->asNode()) {
            nodeFactory->discard(view->graph().get(), nodeptr);
          } else {
            itemFactory->discard(view->graph().get(), pendingItemToPlace_.get());
          }
          pendingItemToPlace_ = replacement->shared_from_this();
        }

        auto editgroup = view->graph()->docRoot()->history().editGroup("add item");
        auto id = view->graph()->add(pendingItemToPlace_);
        if (view->editor()->responser())
          view->editor()->responser()->afterItemAdded(view->graph().get(), pendingItemToPlace_.get());
        if (pendingInputLink_.sourceItem != ID_None) {
          if (
              pendingItemToPlace_->asRouter() ||
              pendingItemToPlace_->asNode() && pendingItemToPlace_->asNode()->numMaxInputs() != 0)
            view->editor()->setLink(view->graph().get(), view,
              pendingInputLink_.sourceItem, pendingInputLink_.sourcePort, id, 0);
        }
        if (pendingOutputLink_.destItem != ID_None) {
          if (
            pendingItemToPlace_->asRouter() ||
            pendingItemToPlace_->asNode() && pendingItemToPlace_->asNode()->numOutputs() != 0)
            view->editor()->setLink(view->graph().get(), view,
              id, 0, pendingOutputLink_.destItem, pendingOutputLink_.destPort);
        }
        // reset group boxes
        if (id != ID_None)
          view->editor()->confirmItemPlacements(view->graph().get(), {id}); // to trigger group box update
        isPlaced_ = true;
        msghub::debugf("item {} placed into graph", id.value());
        view->setSelectedItems({id});
      } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (pendingItemToPlace_->asNode()) {
          nodeFactory->discard(view->graph().get(), pendingItemToPlace_->asNode());
        } else {
          itemFactory->discard(view->graph().get(), pendingItemToPlace_.get());
        }
        pendingItemToPlace_ = nullptr;
        isPlaced_           = true;
      }
    }
  }
  return true;
}

void CreateNodeState::draw(NetworkView* view)
{
  if (pendingItemToPlace_) {
    const Canvas::ShapeStyle style = {
      false, UIStyle::instance().linkDefaultColor, 1.f, UIStyle::instance().linkDefaultColor};
    auto drawLink =
      [view, &style](GraphItem* srcitem, sint srcpin, GraphItem* dstitem, sint dstpin) {
        auto const srcnode   = srcitem->asNode();
        auto const dstnode   = dstitem->asNode();
        auto const srcbounds = srcitem->aabb();
        auto const dstbounds = dstitem->aabb();
        auto const srcpos    = srcnode ? srcnode->outputPinPos(srcpin) : srcitem->pos();
        auto const dstpos    = dstnode ? dstnode->inputPinPos(dstpin) : dstitem->pos();
        auto const srcdir    = srcnode ? srcnode->outputPinDir(srcpin) : Vec2(0, 1);
        auto const dstdir    = dstnode ? dstnode->inputPinDir(dstpin) : Vec2(0, -1);

        auto path = view->graph()->calculatePath(srcpos, dstpos, srcdir, dstdir);
        view->canvas()->drawPoly(path.data(), path.size(), false, style);
      };
    if (auto* node = pendingItemToPlace_->asNode()) {
      if (pendingInputLink_.sourceItem != ID_None && node->numMaxInputs() != 0) {
        drawLink(
          view->graph()->get(pendingInputLink_.sourceItem).get(),
          pendingInputLink_.sourcePort,
          node,
          0);
      }
      if (pendingOutputLink_.destItem != ID_None && node->numOutputs() > 0) {
        drawLink(
          node,
          0,
          view->graph()->get(pendingOutputLink_.destItem).get(),
          pendingOutputLink_.destPort);
      }
    } else if (auto* router = pendingItemToPlace_->asRouter()) {
      if (pendingInputLink_.sourceItem != ID_None) {
        drawLink(
          view->graph()->get(pendingInputLink_.sourceItem).get(),
          pendingInputLink_.sourcePort,
          router,
          0);
      }
      if (pendingOutputLink_.destItem != ID_None) {
        drawLink(
          router,
          0,
          view->graph()->get(pendingOutputLink_.destItem).get(),
          pendingOutputLink_.destPort);
      }
    }
    pendingItemToPlace_->draw(view->canvas(), GraphItemState::DISABLED);
  } else {
    // no pending item to place
    // draw links
    auto mousepos    = view->canvas()->screenToCanvas().transformPoint(vec(ImGui::GetMousePos()));
    auto const style = Canvas::ShapeStyle{
      false, UIStyle::instance().linkDefaultColor, 1.f, UIStyle::instance().linkDefaultColor};
    if (auto srcitem = view->graph()->tryGet(pendingInputLink_.sourceItem)) {
      auto start = view->graph()->pinPos({srcitem->id(), pendingInputLink_.sourcePort, NodePin::Type::Out});
      auto bb = srcitem->localBound();
      auto path = view->graph()->calculatePath(start, mousepos, {0,1}, {0,-1}, bb, {{0,0}});
      if (!path.empty())
        view->canvas()->drawPoly(path.data(), path.size(), false, style);
    }
    if (auto destitem = view->graph()->tryGet(pendingOutputLink_.destItem)) {
      auto endpos = view->graph()->pinPos({destitem->id(), pendingOutputLink_.destPort, NodePin::Type::In});
      auto bb = destitem->localBound();
      auto path = view->graph()->calculatePath(mousepos, endpos, {0,1}, {0,-1}, {{0,0}}, bb);
      if (!path.empty())
        view->canvas()->drawPoly(path.data(), path.size(), false, style);
    }
  }
}
// }}} CreateNodeState

} // namespace detail

GraphItemFactoryPtr addImGuiItems(GraphItemFactoryPtr factory)
{
  factory->set("comment", true, [](Graph* parent) -> GraphItemPtr {
    return std::make_shared<ImGuiCommentBox>(parent);
  });
  return factory;
}

void addImGuiInteractions()
{
  NetworkView::registerInteraction<HandleView>();
  NetworkView::registerInteraction<AnimationState>();
  NetworkView::registerInteraction<MoveState>();
  NetworkView::registerInteraction<LinkState>();
  NetworkView::registerInteraction<CutLinkState>();
  NetworkView::registerInteraction<CreateNodeState>();
  NetworkView::registerInteraction<SelectionState>();
  NetworkView::registerInteraction<HandleShortcut>();
  NetworkView::registerInteraction<EditArrow>();
  NetworkView::registerInteraction<ResizeBoxState>();
}

} // namespace nged
