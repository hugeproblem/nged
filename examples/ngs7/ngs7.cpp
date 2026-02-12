#include "ngs7.h"
#include <nged/nged_imgui.h>
#include <nged/style.h>
#include <nged/res/fa_icondef.h>
#include <nged/utils.h>

#include "s7.h"
#include "s7-extensions.h"

#include <subprocess.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <thread>

#if !defined(_WIN32)
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>

static std::string abs_path_of_current_process()
{
  char pathbuf[PROC_PIDPATHINFO_MAXSIZE] = {0};
  if (proc_pidpath(getpid(), pathbuf, sizeof(pathbuf)) > 0)
    return pathbuf;
  else
    return "";
}
#else // assume to be linux
// readlink from /proc/self/exe
static std::string abs_path_of_current_process()
{
  char pathbuf[PATH_MAX] = {0};
  if (readlink("/proc/self/exe", pathbuf, PATH_MAX-1) != -1)
    return pathbuf;
  else
    return "";
}
#endif
#endif

namespace ngs7 {

using namespace nged;
using msghub = MessageHub;

class S7Doc : public nged::NodeGraphDoc
{
  s7_scheme* s7instance_ = nullptr;

public:
  S7Doc(NodeFactoryPtr nodeFactory, GraphItemFactory const* itemFactory)
      : NodeGraphDoc(nodeFactory, itemFactory)
  {
    s7instance_ = s7_init();
    addS7Extenstions(s7instance_);
  }
  virtual ~S7Doc()
  {
    s7_free(s7instance_);
  }

  s7_scheme* s7() const { return s7instance_; }

  void evalInProcess(StringView code, String& output, String& err, String& result)
  {
    auto strport = s7_open_output_string(s7instance_);
    auto prevport = s7_current_output_port(s7instance_);
    auto strerrport = s7_open_output_string(s7instance_);
    auto preverrport = s7_current_error_port(s7instance_);

    s7_set_current_output_port(s7instance_, strport);
    s7_set_current_error_port(s7instance_, strerrport);
    assert(code.data()[code.size()]==0);
    auto ret = s7_eval_c_string(s7instance_, code.data());
    result = s7_object_to_c_string(s7instance_, ret);

    output = s7_get_output_string(s7instance_, strport);
    err = s7_get_output_string(s7instance_, strerrport);

    s7_close_output_port(s7instance_, strport);
    s7_close_output_port(s7instance_, strerrport);

    s7_set_current_output_port(s7instance_, prevport);
    s7_set_current_error_port(s7instance_, preverrport);
  }

  void evalNewProcess(StringView code, String& output, String& err, String& result)
  {
    String s7epath = "";
#ifndef _WIN32
    String myfullpath = abs_path_of_current_process();
    auto parts = utils::strsplit(myfullpath, "/");
    for (int i=0, n=parts.size(); i+1<n; ++i) {
      s7epath += parts[i];
      s7epath += '/';
    }
#endif
    s7epath += "s7e";

    char const* const cmdline[] = {s7epath.c_str(), "--rep", nullptr};
    struct subprocess_s proc;
    int ret = -1;
    if (0 != subprocess_create(
          cmdline,
          subprocess_option_search_user_path | subprocess_option_no_window | subprocess_option_enable_async,
          &proc)) {
      err = "failed to lanuch process s7e";
      return;
    }
    FILE* proc_stdin = subprocess_stdin(&proc);
    if (!proc_stdin) {
      msghub::error("failed to redirect stdin/stdout for process s7e");
      return;
    }

    auto doRead = [&proc](String* out, unsigned(*reader)(struct subprocess_s*, char*, unsigned)) {
      char buf[1024];
      size_t bytesread = 0;
      do {
        bytesread = reader(&proc, buf, sizeof(buf));
        out->append(buf, bytesread);
      } while (bytesread != 0);
    };
    output.clear();
    err.clear();
    std::thread outReadingThread(doRead, &output, &subprocess_read_stdout);
    std::thread errReadingThread(doRead, &err,    &subprocess_read_stderr);
    outReadingThread.detach();
    errReadingThread.detach();

    fwrite(code.data(), 1, code.size(), proc_stdin);
    fputc(0, proc_stdin);
    fflush(proc_stdin);

    subprocess_join(&proc, &ret);
    if (outReadingThread.joinable())
      outReadingThread.join();
    if (errReadingThread.joinable())
      errReadingThread.join();

    if (ret) {
      result = "error code: " + std::to_string(ret);
    } else {
      result = "success";
    }

    subprocess_destroy(&proc);
  }

  void eval(StringView code, String& output, String& err, String& result)
  {
    evalNewProcess(code, output, err, result);
  }

  String filterFileInput(StringView in) override;
  String filterFileOutput(StringView out) override;
};

class S7Responser : public nged::DefaultImGuiResponser
{
public:
  void onInspect(InspectorView* view, GraphItem** items, size_t numItems) override;
  void onItemHovered(NetworkView* view, GraphItem* item) override;
};

class S7Node : public nged::Node
{
protected:
  sint numDesiredInputs_ = -1;
  sint version_          = 0;
  bool dirty_            = false;
  bool editable_         = true;

  String code_     = "";
  String quote_    = "";
  bool   asSymbol_ = true;

  struct Line
  {
    int    indent;
    String text;
  };

  Vector<Line> codeCache_;

  friend void S7Responser::onInspect(InspectorView*, GraphItem**, size_t);
  friend class S7Graph;

public:
  virtual ~S7Node();
  virtual bool serialize(Json& json) const override
  {
    json["code"]  = code_;
    json["quote"] = quote_;
    json["asSymbol"] = asSymbol_;
    return nged::Node::serialize(json);
  }
  virtual bool deserialize(Json const& json) override
  {
    if (nged::Node::deserialize(json)) {
      code_  = json["code"];
      quote_ = json["quote"];
      asSymbol_ = json.value("asSymbol", true);
      dirty_ = true;
      settle();
      return true;
    }
    return false;
  }
  virtual bool getIcon(IconType& iconType, StringView& iconData) const override
  {
    iconType = IconType::Text;
    if (!quote_.empty())
      iconData = quote_;
    else if (type()=="literal") {
      iconType = IconType::IconFont;
      iconData = ICON_FA_RECEIPT;
    } else if (type()=="call::()")
      iconData = "(  )";
    else if (type().find("subgraph::")==0) {
      iconType = IconType::IconFont;
      iconData = ICON_FA_CODE_BRANCH;
    } else if (type() == "output") {
      iconType = IconType::IconFont;
      iconData = ICON_FA_FLAG_CHECKERED;
    }
    else if (type().find("::")!=String::npos)
      iconData = code_;
    else
      iconData = type();
    return true;
  }
  virtual StringView label() const override
  {
    if (type().find("::")==String::npos || type()=="call::()") { // literal / number / str
      return code_;
    } else {
      return "";
    }
  }

  void settle()
  {
    if (type().find("::")==String::npos || type()=="call::()")
      editable_ = true;
    else
      editable_ = false;
    float width = 50.f;
    float height = aabb_.height();
    IconType   iconType;
    StringView iconData;
    getIcon(iconType, iconData);
    if (numDesiredInputs_ < 0)
      width = 25.f;
    if (type()=="output") {
      width = 40;
      height = width;
      editable_ = false;
      color_ = gmath::fromUint32sRGB(0x2E7D32);
    } else if (type() == "literal") {
      width = 30.f;
      height = width;
    } else if (type().find("subgraph::")==0) {
      width = 30.f;
      height = width;
      editable_ = false;
    } else if (iconType == IconType::Text) {
      width = std::max(20.f, iconData.size() * UIStyle::instance().normalFontSize / 2.f + 8.f);
    } else if (type().find("call::")==0 || type().find("::") == String::npos) {
      width = std::max(20.f, name().size() * UIStyle::instance().normalFontSize / 2.f + 8.f);
    } else if (type().find("quote::") == String::npos) {
      width = 30.f;
    }

    Node::resize(width, height, 15.f, 8.f);
  }

public:
  S7Node(Graph* parent, StringView type, StringView name, StringView code, int numInputs)
      : Node(parent, String(type), String(name)), code_(String(code))
  {
    if (type.find("quote::") == 0) {
      quote_ = code_;
      code_  = "";
    }
    numDesiredInputs_ = numInputs;
    dirty_            = true;
    settle();
  }

  bool        dirty() const { return dirty_; }
  void        setDirty() { dirty_ = true; }
  sint        version() const { return version_; }
  void        setVersion(sint v) { version_ = v; }
  auto const& code() const { return code_; }
  auto const& generatedCode() const { return codeCache_; }

  virtual bool prelude(String& val) const { return false; }
  virtual bool epilogue(String& val) const { return false; }

  virtual bool isFlagApplicatable(uint64_t flags, String* reason) const override
  {
    if (type() == "output" && (flags & NODEFLAG_BYPASS)) {
      if (reason)
        *reason = "output node cannot be bypassed";
      return false;
    }
    return true;
  }

  virtual sint numMaxInputs() const override { return numDesiredInputs_; }
  virtual sint numOutputs() const override { return type() == "output" ? 0 : 1; }
  virtual void sync(S7Node** inputs, int numInputs)
  {
    if (type() == "str") {
      std::string outCode = "\"";
      for (auto c : code_) {
        if (c == '"')
          outCode += "\\\"";
        else if (c == '\\')
          outCode += "\\\\";
        else if (c == '\n')
          outCode += "\\n";
        else if (c == '\r')
          outCode += "\\r";
        else
          outCode += c;
      }
      outCode += "\"";
      codeCache_ = {{0, outCode}};
    } else if (type() == "output") {
      if (numInputs == 1) {
        codeCache_ = inputs[0]->generatedCode();
      } else {
        codeCache_.clear();
      }
    } else if (type().find("call::") == 0) {
      Vector<Line> args;
      bool         multiline = false;
      size_t       linelen   = 0;
      for (int i = 0; i < numInputs; ++i) {
        if (inputs[i]) {
          auto const& lines = inputs[i]->generatedCode();
          if (lines.size() > 1)
            multiline = true;
          for (auto&& line : lines) {
            linelen += line.text.size();
            args.push_back(line);
          }
        }
      }
      if (args.empty() && type() != "call::()" && asSymbol_) {
        codeCache_.clear();
        codeCache_.push_back({0, code_});
        dirty_ = false;
        return;
      }
      linelen += args.size();
      if (linelen + code_.size() >= 60)
        multiline = true;
      if (multiline) {
        codeCache_.clear();
        assert(args.size() > 0);
        int indent = int(quote_.length())+1;
        if (code_.empty()) {
          args[0].text   = fmt::format("{}({}", quote_, args[0].text);
          args[0].indent = -indent;
        } else
          codeCache_.push_back({0, fmt::format("{}({}", quote_, code_)});
        for (auto&& line : args)
          codeCache_.push_back({line.indent + indent, line.text});
        codeCache_.back().text += ')';
      } else {
        if (linelen > 0) {
          Vector<StringView> strArgs;
          for (auto&& line : args)
            strArgs.push_back(line.text);
          codeCache_ = {
            {0,
             fmt::format(
               "{}({}{}{})",
               quote_,
               code_,
               code_.empty() || strArgs.empty() ? "" : " ",
               fmt::join(strArgs, " "))}};
        } else {
          codeCache_ = {{0, fmt::format("{}({})", quote_, code_)}};
        }
      }
    } else {
      if (type().find("quote::") == 0 && numInputs == 1) {
        codeCache_         = inputs[0]->generatedCode();
        codeCache_[0].text = quote_ + codeCache_[0].text;
        for (size_t i=1, n=codeCache_.size(); i<n; ++i)
          codeCache_[i].indent += int(quote_.size());
      } else {
        codeCache_ = {{0, quote_ + code_}};
      }
    }
    dirty_ = false;
  }

  String prettyPrintCode();

};

class S7Graph : public nged::Graph
{
  ItemID outputNodeID_  = ID_None;
  bool   deserializing_ = false;

public:
  S7Graph(NodeGraphDoc* root, Graph* parent, String name) : Graph(root, parent, name) {
    auto outputNode = std::make_shared<S7Node>(this, "output", "output", "", 1);
    outputNodeID_ = docRoot()->addItem(outputNode);
    outputNode->resetID(outputNodeID_);
    items_.insert(outputNodeID_);
  }

  auto outputNode() const { return std::static_pointer_cast<S7Node>(get(outputNodeID_)); }

  virtual bool deserialize(Json const& json) override
  {
    deserializing_ = true;
    auto succeed   = Graph::deserialize(json);
    for (auto id: items_) {
      if (auto* node = get(id)->asNode()) {
        static_cast<S7Node*>(node)->settle();
      }
    }
    deserializing_ = false;
    return succeed;
  }

  virtual LinkPtr setLink(ItemID sourceItem, sint sourcePort, ItemID destItem, sint destPort)
    override
  {
    auto result = Graph::setLink(sourceItem, sourcePort, destItem, destPort);
    if (!deserializing_) { // don't check loop during deserializing
      if (Vector<ItemID> loop; checkLoopBottomUp(destItem, loop)) {
        msghub::error("loop detected, please don't do this");
        removeLink(result->output().destItem, result->output().destPort);
        return nullptr;
      }
      if (auto* node = get(destItem)->asNode())
        static_cast<S7Node*>(node)->settle();
      markNodeAndDownstreamDirty(destItem);
    }
    return result;
  }
  virtual void removeLink(ItemID destItem, sint destPort) override
  {
    if (auto* node = get(destItem)->asNode())
      static_cast<S7Node*>(node)->settle();
    markNodeAndDownstreamDirty(destItem);
    Graph::removeLink(destItem, destPort);
  }

  virtual ItemID add(GraphItemPtr item) override
  {
    if (item->id() != ID_None) // if already added, e.g., output node can be added while it already exists (at creation)
      return item->id();
    return Graph::add(item);
  }
  virtual void remove(HashSet<ItemID> const& items) override
  {
    HashSet<ItemID> itemsToRemove = items;
    itemsToRemove.erase(outputNodeID_); // always keep output node
    Vector<ItemID> dirtySources;
    for (auto id : itemsToRemove)
      if (auto item = get(id)) {
        if (auto* link = item->asLink())
          dirtySources.push_back(link->output().destItem);
        else if (item->asNode())
          dirtySources.push_back(id);
      }
    GraphTraverseResult affected;
    if (travelTopDown(affected, dirtySources)) {
      for (auto affectedItem : affected) {
        static_cast<S7Node*>(affectedItem.node())->setDirty();
      }
    }
    Graph::remove(itemsToRemove);
  }
  virtual void clear() override
  {
    for (auto id : items_)
      if (id != outputNodeID_)
        docRoot()->removeItem(id);
    items_.clear();
    items_.insert(outputNodeID_);
    links_.clear();
    linkIDs_.clear();
  }

  virtual void regulateVariableInput(Node* node) override
  {
    Graph::regulateVariableInput(node);
    if (node)
      markNodeAndDownstreamDirty(node->id());
  }

  void markNodeAndDownstreamDirty(ItemID id)
  {
    GraphTraverseResult affected;
    if (travelTopDown(affected, id)) {
      for (auto affectedItem : affected) {
        static_cast<S7Node*>(affectedItem.node())->setDirty();
      }
    } else {
      msghub::errorf("cannot travel from {} to bottom", id.value());
    }
    if (parent_) {
      for (auto id: parent_->items()) {
        if (auto* node = parent_->get(id)->asNode()) {
          if (node->asGraph()==static_cast<Graph*>(this)) {
            static_cast<S7Graph*>(parent_)->markNodeAndDownstreamDirty(id);
            break;
          }
        }
      }
    }
  }

  void updateCodeFromNode(ItemID id)
  {
    GraphTraverseResult related;
    if (!travelBottomUp(related, id)) {
      if (get(id) && get(id)->asNode())
        msghub::errorf("cannot travel from node {} to top", get(id)->asNode()->name());
      else
        msghub::errorf("cannot travel from node {} to top", id.value());
      return;
    }

    for (size_t i = related.size(); i != 0; --i) {
      auto*           self       = static_cast<S7Node*>(related.node(i - 1));
      bool            inputdirty = false;
      Vector<S7Node*> inputs(related.inputCount(i - 1));
      for (int c = 0, n = related.inputCount(i - 1); c < n; ++c) {
        if (auto* inputnode = related.inputOf(i - 1, c)) {
          inputs[c] = static_cast<S7Node*>(inputnode);
        }
      }

      sint maxInputVersion = -1;
      for (auto* inputnode : inputs) {
        if (!inputnode)
          continue;
        maxInputVersion = std::max(maxInputVersion, inputnode->version());
        if (inputnode->dirty() || inputnode->version() > self->version()) {
          inputdirty = true;
          self->setDirty();
        }
      }
      if (self->dirty()) {
        msghub::infof("updating code for node {}", related.node(i - 1)->name());
        self->sync(inputs.data(), inputs.size());
        self->setVersion(std::max(self->version(), maxInputVersion));
      }
    }
  }
};

class S7Subgraph : public S7Node
{
  std::shared_ptr<S7Graph> subgraph_;

public:
  virtual bool serialize(Json& json) const override
  {
    return S7Node::serialize(json) && subgraph_->serialize(json);
  }
  virtual bool deserialize(Json const& json) override
  {
    return S7Node::deserialize(json) && subgraph_->deserialize(json);
  }

public:
  S7Subgraph(Graph* parent, String type) : S7Node(parent, type, type, "", 0)
  {
    subgraph_ = std::make_shared<S7Graph>(parent->docRoot(), parent, type);
  }
  virtual Graph* asGraph() override
  {
    return subgraph_.get();
  }
  virtual void sync(S7Node** inputs, int numInputs) override
  {
    if (dirty_||subgraph_->outputNode()->dirty()||version_!=subgraph_->outputNode()->version()) {
      subgraph_->updateCodeFromNode(subgraph_->outputNode()->id());
      codeCache_ = subgraph_->outputNode()->generatedCode();
      version_ = subgraph_->outputNode()->version();
    }
    dirty_ = false;
  }
};

String S7Node::prettyPrintCode()
{
  if (dirty() && parent())
    static_cast<S7Graph*>(parent())->updateCodeFromNode(id());
  String ppCode; // pretty print code
  for (auto&& line : codeCache_) {
    ppCode += String(line.indent, ' ') + line.text + '\n';
  }
  return ppCode;
}

String S7Doc::filterFileOutput(StringView out)
{
  auto outnode = static_cast<S7Graph*>(root().get())->outputNode();
  return fmt::format(
      "{}\n;------ BEGIN NODE GRAPH ------\n;{}\n;------ END NODE GRAPH ------",
      outnode->prettyPrintCode(),
      fmt::join(utils::strsplit(out, "\n"), "\n;"));
}

String S7Doc::filterFileInput(StringView in)
{
  static constexpr std::string_view BEGIN_SIG = "-- BEGIN NODE GRAPH --";
  static constexpr std::string_view END_SIG   = "--- END NODE GRAPH ---";
  auto const begin = std::search(in.begin(), in.end(),
      std::boyer_moore_searcher(BEGIN_SIG.begin(), BEGIN_SIG.end()));
  if (begin == in.end())
    return String(in);
  auto end = std::search(begin, in.end(), std::boyer_moore_searcher(END_SIG.begin(), END_SIG.end()));
  auto lines = utils::strsplit(in.substr(begin-in.begin(), end-begin), "\n");
  if (lines.size()<=2)
    return "";
  String json = "";
  for (size_t i=1, n=lines.size()-1; i<n; ++i) {
    if (lines[i].length()>0 && lines[i][0]==';') {
      json += lines[i].substr(1);
      json += '\n';
    }
  }
  return json;
}

void S7Responser::onInspect(InspectorView* view, GraphItem** items, size_t numItems)
{
  bool  handled  = false;
  Node* solyNode = nullptr;
  for (size_t i = 0; i < numItems; ++i) {
    if (auto* node = items[i]->asNode()) {
      if (solyNode) {
        solyNode = nullptr;
        break;
      } else {
        solyNode = node;
      }
    }
  }
  if (auto* node = solyNode) {
    auto* s7node = static_cast<S7Node*>(node);
    bool inputModified = false;
    ImGui::PushItemWidth(-1);
    if (
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow) && !ImGui::IsAnyItemActive() &&
      !ImGui::IsAnyItemFocused() && !ImGui::IsMouseClicked(0))
      ImGui::SetKeyboardFocusHere();

    if (s7node->editable_) {
      if (s7node->type() == "str") {
        inputModified = ImGui::InputTextMultiline(
            "###str",
            &s7node->code_,
            {0, 0},
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
      } else {
        inputModified = ImGui::InputText(
            "###code",
            &s7node->code_,
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
      }
    }
    if (utils::startswith(s7node->type(), "call::") &&
        s7node->type() != "call::()" &&
        ImGui::Checkbox("As symbol if got no input",  &s7node->asSymbol_))
      inputModified = true;
    if (inputModified) {
      s7node->setVersion(s7node->version() + 1);
      view->graph()->docRoot()->history().commit("edit");
      static_cast<S7Graph*>(node->parent())->markNodeAndDownstreamDirty(node->id());
    }
    ImGui::PopItemWidth();
    ImGui::Separator();
    ImGui::PushFont(ImGuiResource::instance().monoFont);
    ImGui::TextUnformatted(s7node->prettyPrintCode().c_str());
    ImGui::PopFont();
    handled = true;
  }

  if (!handled) {
    DefaultImGuiResponser::onInspect(view, items, numItems);
  }

  if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    NetworkView* netviewToFocus = nullptr;
    if (auto lv = view->linkedView()) {
      if (auto* netview = dynamic_cast<NetworkView*>(lv.get()))
        netviewToFocus = netview;
    }
    if (!netviewToFocus && numItems > 0) {
      for (auto v : view->editor()->views()) {
        if (auto* netview = dynamic_cast<NetworkView*>(v.get())) {
          if (!netviewToFocus)
            netviewToFocus = netview;
          else if (netview->graph().get() == items[0]->parent())
            netviewToFocus = netview;
        }
      }
    }
    if (netviewToFocus) {
      if (auto* imguiWindow = dynamic_cast<ImGuiNamedWindow*>(netviewToFocus)) {
        msghub::infof("focusing window {}", imguiWindow->titleWithId());
        ImGui::SetWindowFocus(imguiWindow->titleWithId().c_str());
      }
    }
  }
}

void S7Responser::onItemHovered(NetworkView* view, GraphItem* item)
{
  if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) { // help
    if (auto* node = item->asNode()) {
      auto* s7node = static_cast<S7Node*>(node);
      if (s7node->type().find("call::")==0) {
        auto funcname = s7node->code();
        if (!funcname.empty()) {
          auto* sc = static_cast<S7Doc*>(s7node->parent()->docRoot())->s7();
          if (auto helpstr = s7_help(sc, s7_make_symbol(sc, funcname.c_str()))) {
            String help = helpstr;
            static const std::string lots_of_spaces = "                                 ";
            for (auto pspaces = help.find(lots_of_spaces);
              pspaces != std::string::npos;
              pspaces = help.find(lots_of_spaces)) {
              help.replace(pspaces, lots_of_spaces.size(), "\n");
            }
            ImGui::SetTooltip("%s", help.c_str());
          }
        }
      }
    }
  } // end help
}

// Factory & Builtins {{{
struct S7NodeDef
{
  char const* type;
  char const* name;
  char const* preset;
  int         numInputs;
  bool        terminating; // true->symbol false->list
  bool        newLine;
};

// clang-format off
static const S7NodeDef builtinNodeDefs_[] = {
  {"literal", "literal", "", 0, true, false},
  {"number", "number", "0.0", 0, true, false},
  {"str", "str", "", 0, true, false},

  {"quote::unquote", "unquote (,)", ",", 1, false, true},
  {"quote::unquote splicing", "unquote splicing (,@)", ",@", 1, false, true},
  {"quote::quote", "quote (')", "'", 1, false, true},
  {"quote::quasiquote", "quasiquote (`)", "`", 1, false, true},

  {"call::()", "(  )", "", -1, false, true},
  {"call::car", "car", "car", 1, false, true},
  {"call::cdr", "cdr", "cdr", 1, false, true},
  {"call::cons", "cons", "cons", 2, false, true},
  {"call::copy", "copy", "copy", 1, false, true},
  {"call::append", "append", "append", -1, false, true},
  {"call::list", "list", "list", -1, false, true},
  {"call::list?", "list?", "list?", 1, false, true},
  {"call::set-cdr!", "set-cdr!", "set-cdr!", 2, false, true},
  {"call::set-car!", "set-car!", "set-car!", 2, false, true},
  {"call::tree-copy", "tree-copy", "tree-copy", 1, false, true},
  {"call::list-ref", "list-ref", "list-ref", 2, false, true},
  {"call::list-copy", "list-copy", "list-copy", 1, false, true},
  {"call::vector->list", "vector->list", "vector->list", 1, false, true},
  {"call::string->list", "string->list", "string->list", 1, false, true},
  {"call::sublist", "sublist", "sublist", 3, false, true},
  {"call::list-head", "list-head", "list-head", 2, false, true},
  {"call::list-tail", "list-tail", "list-tail", 2, false, true},
  {"call::length", "length", "length", 1, false, true},
  {"call::reverse", "reverse", "reverse", 1, false, true},
  {"call::pair?", "pair?", "pair?", 1, false, true},
  {"call::sort!", "sort!", "sort!", 1, false, true},
  {"call::for-each", "for-each", "for-each", -1, false, true},
  {"call::vector", "vector", "vector", -1, false, true},
  {"call::vector?", "vector?", "vector?", 1, false, true},
  {"call::vector-copy", "vector-copy", "vector-copy", 1, false, true},
  {"call::list->vector", "list->vector", "list->vector", 1, false, true},
  {"call::make-vector", "make-vector", "make-vector", 2, false, true},
  {"call::make-initialized-vector", "make-initialized-vector", "make-initialized-vector", 2, false, true},
  {"call::vector-grow", "vector-grow", "vector-grow", 2, false, true},
  {"call::vector-length", "vector-length", "vector-length", 1, false, true},
  {"call::vector-ref", "vector-ref", "vector-ref", 2, false, true},
  {"call::vector-set!", "vector-set!", "vector-set!", 3, false, true},
  {"call::subvector", "subvector", "subvector", 3, false, true},
  {"call::vector-head", "vector-head", "vector-head", 2, false, true},
  {"call::vector-tail", "vector-tail", "vector-tail", 2, false, true},
  {"call::vector-fill", "vector-fill", "vector-fill", 2, false, true},
  {"call::filter", "filter", "filter", 2, false, true},
  {"call::map", "map", "map", -1, false, true},
  {"call::lambda", "lambda", "lambda", 2, false, true},
  {"call::lambda*", "lambda*", "lambda*", -1, false, true},
  {"call::call", "call", "call", -1, false, true},
  {"call::call/cc", "call/cc", "call/cc", -1, false, true},
  {"call::apply", "apply", "apply", -1, false, true},
  {"call::eval", "eval", "eval", 1, false, true},
  {"call::display", "display", "display", -1, false, true},
  {"call::define", "define", "define", -1, false, true},
  {"call::define*", "define*", "define*", -1, false, true},
  {"call::define-constant", "define-constant", "define-constant", -1, false, true},
  {"call::define-macro", "define-macro", "define-macro", -1, false, true},
  {"call::define-macro*", "define-macro*", "define-macro*", -1, false, true},
  {"call::macroexpand", "macroexpand", "macroexpand", -1, false, true},
  {"call::quote", "quote", "quote", -1, false, true},
  {"call::do", "do", "do", -1, false, true},
  {"call::begin", "begin", "begin", -1, false, true},
  {"call::cond", "cond", "cond", -1, false, true},
  {"call::if", "if", "if", 3, false, true},
  {"call::and", "and", "and", -1, false, true},
  {"call::or", "or", "or", -1, false, true},
  {"call::case", "case", "case", -1, false, true},
  {"call::when", "when", "when", -1, false, true},
  {"call::unless", "unless", "unless", -1, false, true},
  {"call::let", "let", "let", -1, false, true},
  {"call::let*", "let*", "let*", -1, false, true},
  {"call::letrec", "letrec", "letrec", -1, false, true},
  {"call::letrec*", "letrec*", "letrec*", -1, false, true},
  {"call::set!", "set!", "set!", 2, false, true},
  {"call::string", "string", "string", -1, false, true},
  {"call::string?", "string?", "string?", 1, false, true},
  {"call::string=?", "string=?", "string=?", 2, false, true},
  {"call::string<?", "string<?", "string<?", 2, false, true},
  {"call::string>?", "string>?", "string>?", 2, false, true},
  {"call::string-ci=?", "string-ci=?", "string-ci=?", 2, false, true},
  {"call::string-ci<?", "string-ci<?", "string-ci<?", 2, false, true},
  {"call::string-ci>?", "string-ci>?", "string-ci>?", 2, false, true},
  {"call::string-compare", "string-compare", "string-compare", 2, false, true},
  {"call::string-compare-ci", "string-compare-ci", "string-compare-ci", 2, false, true},
  {"call::string-position", "string-position", "string-position", 2, false, true},
  {"call::strstr", "strstr", "string-position", 2, false, true},
  {"call::make-string", "make-string", "make-string", 2, false, true},
  {"call::list->string", "list->string", "list->string", 1, false, true},
  {"call::string-append", "string-append", "string-append", -1, false, true},
  {"call::string-ref", "string-ref", "string-ref", 2, false, true},
  {"call::substring", "substring", "substring", 3, false, true},
  {"call::substring?", "substring?", "substring?", 2, false, true},
  {"call::string-head", "string-head", "string-head", 2, false, true},
  {"call::string-tail", "string-tail", "string-tail", 2, false, true},
  {"call::string->number", "string->number", "string->number", 2, false, true},
  {"call::number->string", "number->string", "number->string", 2, false, true},
  {"call::random", "random", "random", 2, false, true},
  {"call::lcm", "lcm", "lcm", -1, false, true},
  {"call::gcd", "gcd", "gcd", -1, false, true},
  {"call::magnitude", "magnitude", "magitude", 2, false, true},
  {"call::sin", "sin", "sin", 1, false, true},
  {"call::cos", "cos", "cos", 1, false, true},
  {"call::tan", "tan", "tan", 1, false, true},
  {"call::sqrt", "sqrt", "sqrt", 1, false, true},
  {"call::floor", "floor", "floor", 1, false, true},
  {"call::ceiling", "ceiling", "ceiling", 1, false, true},
  {"call::round", "round", "round", 1, false, true},
  {"call::truncate", "truncate", "truncate", 1, false, true},
  {"call::char-upcase", "char-upcase", "char-upcase", 1, false, true},
  {"call::char-downcase", "char-downcase", "char-downcase", 1, false, true},
  {"call::char->integer", "char->integer", "char->integer", 1, false, true},
  {"call::integer->char", "integer->char", "integer->char", 1, false, true},
  {"call::char-whitespace?", "char-whitespace?", "char-whitespace?", 1, false, true},
  {"call::char-upper-case?", "char-upper-case?", "char-upper-case?", 1, false, true},
  {"call::char-lower-case?", "char-lower-case?", "char-lower-case?", 1, false, true},
  {"call::char-numeric?", "char-numeric?", "char-numeric?", 1, false, true},
  {"call::char-alphabic?", "char-alphabic?", "char-alphabic?", 1, false, true},

  {"call::empty?", "empty?", "empty?", 1, false, true},
  {"call::curry", "curry", "curry", -1, false, true},
  {"call::combine", "combine", "combine", -1, false, true},
  {"call::directory?", "directory?", "directory?", 1, false, true},
  {"call::make-dirs", "make-dirs", "make-dirs", 1, false, true},
  {"call::file-exists?", "file-exists?", "file-exists?", 1, false, true},
  {"call::delete-file", "delete-file", "delete-file", 1, false, true},
  {"call::getenv", "getenv", "getenv", 1, false, true},
  {"call::system", "system", "system", 2, false, true},
  {"call::directory->list", "directory->list", "directory->list", 2, false, true},
  {"call::file-mtime", "file-mtime", "file-mtime", 1, false, true},
  {"call::format-time", "format-time", "format-time", 3, false, true},
  {"call::path-root-name", "path-root-name", "path-root-name", 1, false, true},
  {"call::path-root-directory", "path-root-directory", "path-root-directory", 1, false, true},
  {"call::path-root-path", "path-root-path", "path-root-path", 1, false, true},
  {"call::path-relative-path", "path-relative-path", "path-relative-path", 1, false, true},
  {"call::path-parent-path", "path-parent-path", "path-parent-path", 1, false, true},
  {"call::path-filename", "path-filename", "path-filename", 1, false, true},
  {"call::path-extension", "path-extension", "path-extension", 1, false, true},
  {"call::path-stem", "path-stem", "path-stem", 1, false, true},
  {"call::path-absolute?", "path-absolute?", "path-absolute?", 1, false, true},
  {"call::path-relative?", "path-relative?", "path-relative?", 1, false, true},
  {"call::path-has-root-path?", "path-has-root-path?", "path-has-root-path?", 1, false, true},
  {"call::path-has-root-name?", "path-has-root-name?", "path-has-root-name?", 1, false, true},
  {"call::path-has-root-directory?", "path-has-root-directory?", "path-has-root-directory?", 1, false, true},
  {"call::path-has-relative-path?", "path-has-relative-path?", "path-has-relative-path?", 1, false, true},
  {"call::path-has-parent-path?", "path-has-parent-path?", "path-has-parent-path?", 1, false, true},
  {"call::path-has-filename?", "path-has-filename?", "path-has-filename?", 1, false, true},
  {"call::path-has-stem?", "path-has-stem?", "path-has-stem?", 1, false, true},
  {"call::path-has-extension?", "path-has-extension?", "path-has-extension?", 1, false, true},

  {"call::regex-match", "regex-match", "regex-match", 2, false, true},
  {"call::regex-replace", "regex-replace", "regex-replace ", 3, false, true},

  {"call::string-starts-with", "string-starts-with", "string-starts-with", 2, false, true},
  {"call::string-ends-with", "string-ends-with", "string-ends-with", 2, false, true},
  {"call::string-split", "string-split", "string-split", 2, false, true},
  {"call::sprintf", "sprintf", "sprintf", -1, false, true},
  {"call::fprintf", "fprintf", "fprintf", -1, false, true},
  {"call::printf", "printf", "printf", -1, false, true},
};
// clang-format on

class S7NodeFactory : public nged::NodeFactory
{
public:
  nged::GraphPtr createRootGraph(nged::NodeGraphDoc* doc) const override
  {
    return std::make_shared<S7Graph>(doc, nullptr, "root");
  }
  nged::NodePtr createNode(nged::Graph* parent, std::string_view type) const override
  {
    if (type == "subgraph::any") {
      return std::make_shared<S7Subgraph>(parent, String(type));
    } else if (type == "output") {
      return static_cast<S7Graph*>(parent)->outputNode();
    }
    for (auto* def = builtinNodeDefs_;
         def < builtinNodeDefs_ + sizeof(builtinNodeDefs_) / sizeof(*builtinNodeDefs_);
         ++def) {
      if (type == def->type)
        return std::make_shared<S7Node>(parent, type, def->name, def->preset, def->numInputs);
    }
    if (utils::startswith(type, "custom::"))
      return std::make_shared<S7Node>(parent, "call::()", type.substr(8), type.substr(8), -1);
    else if (utils::startswith(type, "literal::"))
      return std::make_shared<S7Node>(parent, "literal", type.substr(9), type.substr(9), 0);
    //else if (utils::startswith(type, "number::"))
    //  return std::make_shared<S7Node>(parent, "number", type.substr(8), type.substr(8), 0);
    //else if (utils::startswith(type, "str::"))
    //  return std::make_shared<S7Node>(parent, "str", type.substr(5), type.substr(5), 0);

    if (utils::startswith(type, "(")) {
      auto name = utils::strstrip(type, "() ");
      return std::make_shared<S7Node>(parent, "call::()", name, name, -1);
    }
    return std::make_shared<S7Node>(parent, "literal", type, type, 0);
  }
  // clang-format off
  void listNodeTypes(
      Graph* graph,
      void* context,
      void (*ret)(void* context, nged::StringView category, nged::StringView type, nged::StringView name)) const override
  {
    ret(context, "subgraph", "subgraph::any", "subgraph");
    ret(context, "output", "output", "output");
    for(auto* def = builtinNodeDefs_;
        def<builtinNodeDefs_+sizeof(builtinNodeDefs_)/sizeof(*builtinNodeDefs_);
        ++def) {
      ret(context, def->terminating?"symbol":"list", def->type, def->name);
    }

    // hint for existing names
    if (graph) {
      HashSet<String> listed;
      for (auto id: graph->items()) {
        auto itemptr = graph->get(id);
        if (auto* nodeptr = itemptr->asNode()) {
          auto* s7node = static_cast<S7Node*>(nodeptr);
          String name = "";
          String label = "";
          if (!utils::strstrip(s7node->code(), " \t\r\n").empty()) {
            if (s7node->type() == "call::()") {
              name = fmt::format("custom::{}", s7node->code());
              label = fmt::format("({})", s7node->code());
            } else if (s7node->type() == "literal") {
              name = fmt::format("literal::{}", s7node->code());
              label = s7node->code();
            }
            //else if (s7node->type() == "number")
            //  name = fmt::format("number::{}", s7node->code());
            //else if (s7node->type() == "str")
            //  name = fmt::format("str::{}", s7node->code());
            if (label!="" && listed.find(name)==listed.end()) {
              ret(context, "custom", name, label);
              listed.insert(name);
            }
          }
        }
      }
    }
  }
  // clang-format on
};
// }}} Factory & Builtins

S7Node::~S7Node()
{
}

// ------------------------------------------

static void evalNodeAndPrintToOutput(NodeGraphEditor* editor, S7Doc* doc, S7Node* node)
{
  String output, err, result;
  String src = node->prettyPrintCode();
  doc->eval(src, output, err, result);
  msghub::output("-------------------------");
  if (!output.empty())
    msghub::outputf("outputs:\n{}", output);
  else
    msghub::output("<no output>");
  if (!err.empty()) {
    msghub::error(err);
    editor->switchMessageTab("log");
  } else {
    editor->switchMessageTab("output");
  }
  msghub::outputf("returns:\n{}", result);
}

void addExtraCommands(NodeGraphEditor* editor)
{
  editor->commandManager().add(new CommandManager::SimpleCommand{
    "Edit/Parameters",
    "Edit Parameters",
    [](GraphView* view, StringView args) {
      NetworkView* netview = static_cast<NetworkView*>(view);
      InspectorView* viewToFocus = nullptr;

      auto mapeq = [](HashSet<ItemID> const& a, HashSet<ItemID> const& b) {
        if (a.size() != b.size())
          return false;
        for (auto&& id: a) {
          if (b.find(id) == b.end())
            return false;
        }
        return true;
      };
      for (auto v : view->editor()->views()) {
        if (auto* inspector = dynamic_cast<InspectorView*>(v.get())) {
          if (mapeq(inspector->inspectingItems(), netview->selectedItems())) {
            if (inspector->lockOnItem())
              viewToFocus = inspector;
            else if (!viewToFocus)
              viewToFocus = inspector;
          }
          if (viewToFocus) { // more than one inspector view is open, choose the best matching one
                             // (TODO)
          } else if (!(inspector->lockOnView() && inspector->linkedView().get() != netview)) {
            viewToFocus = inspector;
          }
        }
      }
      if (auto* imguiWindow = dynamic_cast<ImGuiNamedWindow*>(viewToFocus)) {
        msghub::infof("focusing window {}", imguiWindow->titleWithId());
        ImGui::SetWindowFocus(imguiWindow->titleWithId().c_str());
      }
    },
    Shortcut{'E'},
    "network"});

  editor->commandManager().add(new CommandManager::SimpleCommand{
    "Execute/RunGraph",
    "Run This Graph",
    [](GraphView* view, StringView args) {
      auto doc = static_cast<S7Doc*>(view->graph()->docRoot());
      auto node = static_cast<S7Graph*>(doc->root().get())->outputNode();
      evalNodeAndPrintToOutput(view->editor(), doc, node.get());
    },
    Shortcut{0xF5},
    "network"}).setMayModifyGraph(false);
    
  editor->commandManager().add(new CommandManager::SimpleCommand{
    "Execute/RunNode",
    "Run To Selected Node",
    [](GraphView* view, StringView args) {
      auto* netview = static_cast<NetworkView*>(view);
      if (auto* node = netview->solySelectedNode()) {
        auto doc = static_cast<S7Doc*>(view->graph()->docRoot());
        evalNodeAndPrintToOutput(view->editor(), doc, static_cast<S7Node*>(node));
      }
    },
    Shortcut{0xF5, ModKey::SHIFT},
    "network"}).setMayModifyGraph(false);
}

void initEditor(NodeGraphEditor* editor)
{
  addImGuiInteractions();

  editor->setDocType<S7Doc>();
  editor->setResponser(std::make_shared<S7Responser>());

  auto itemFactory = addImGuiItems(defaultGraphItemFactory());
  auto viewFactory = defaultViewFactory();

  //initItemFactory(&itemFactory);
  editor->setItemFactory(itemFactory);
  //initViewFactory(&viewFactory);
  editor->setViewFactory(std::move(viewFactory));
  editor->setNodeFactory(std::make_shared<S7NodeFactory>());

  editor->initCommands();
  addExtraCommands(editor);

  editor->setFileExt("scm");
  ImGuiResource::reloadFonts();
}

} // namespace ngs7
