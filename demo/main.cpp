// dear imgui - standalone example application for DirectX 11
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.

#include "nged.h"
#include "nged_imgui.h"
#include "style.h"

#include <spdlog/spdlog.h>
#include "entry/entry.h"
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif

#include <nlohmann/json.hpp>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <misc/cpp/imgui_stdlib.cpp>

#include <chrono>

class DummyNode : public nged::Node
{
  int numInput=1;
  int numOutput=1;
  nged::Canvas::ImagePtr icon=nullptr;

public:
  DummyNode(int numInput, int numOutput, nged::Graph* parent, std::string const& type, std::string const& name)
    : nged::Node(parent, type, name)
    , numInput(numInput)
    , numOutput(numOutput)
  {
    uint8_t pixels[64*64*4] = {0};
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++)
      {
        pixels[(y*64+x)*4+0] = x*4;
        pixels[(y*64+x)*4+1] = y*4;
        pixels[(y*64+x)*4+2] = rand()&0xff;
        auto d = gmath::distance(nged::Vec2(x,y), nged::Vec2(32,32));
        pixels[(y*64+x)*4+3] = uint8_t(gmath::clamp((31-d)/4.f, 0.f, 1.f)*255.f);
      }
    icon = nged::Canvas::createImage(pixels, 64, 64);
  }
  nged::sint numMaxInputs() const override { return numInput; }
  nged::sint numOutputs() const override { return numOutput; }
  bool acceptInput(nged::sint port, nged::Node const* srcNode, nged::sint srcPort) const override
  {
    // to test the sanity check
    if (srcNode->type() == "picky" && type() == "picky")
      return false;
    return true;
  }
  void draw(nged::Canvas* canvas, nged::GraphItemState state) const override
  {
    auto left = nged::Vec2(aabb().min.x, pos().y);
    canvas->drawImage(icon, left-nged::Vec2(40,16), left-nged::Vec2(8,-16));
    nged::Node::draw(canvas, state);
  }
};

class SubGraphNode : public DummyNode
{
  nged::GraphPtr subgraph_;

public:
  SubGraphNode(nged::Graph* parent):
    DummyNode(1,1,parent,"subgraph","subgraph")
  {
    subgraph_ = std::make_shared<nged::Graph>(parent->docRoot(), parent, "subgraph");
  }
  virtual nged::Graph* asGraph() override { return subgraph_.get(); }
  virtual nged::Graph const* asGraph() const override { return subgraph_.get(); }
  virtual bool serialize(nged::Json& json) const override { return DummyNode::serialize(json) && subgraph_->serialize(json); }
  virtual bool deserialize(nged::Json const& json) override { return DummyNode::deserialize(json) && subgraph_->deserialize(json); }
};

struct DummyNodeDef
{
  std::string type;
  int numinput, numoutput;
};

static DummyNodeDef defs[] = {
  { "exec", 4, 1 },
  { "null", 1, 1 },
  { "merge", -1, 1 },
  { "split", 1, 2 },
  { "picky", 3, 2 },
  { "out", 1, 0 },
  { "in", 0, 1 }
};

class MyNodeFactory: public nged::NodeFactory 
{
  nged::GraphPtr createRootGraph(nged::NodeGraphDoc* root) const override
  {
    return std::make_shared<nged::Graph>(root, nullptr, "root");
  }
  nged::NodePtr  createNode(nged::Graph* parent, std::string_view type) const override
  {
    std::string typestr(type);
    if (type=="subgraph")
      return std::make_shared<SubGraphNode>(parent);
    for (auto const& d: defs)
      if (d.type == type)
        return std::make_shared<DummyNode>(d.numinput, d.numoutput, parent, typestr, typestr);
    return std::make_shared<DummyNode>(4, 1, parent, typestr, typestr);
  }
  void listNodeTypes(
      nged::Graph* graph,
      void* context,
      void(*ret)(
        void* context,
        nged::StringView category,
        nged::StringView type,
        nged::StringView name)) const override
  {
    ret(context, "subgraph", "subgraph", "subgraph");
    for (auto const& d: defs)
      ret(context, "demo", d.type, d.type);
  }
};


class DemoApp: public nged::App
{
  nged::EditorPtr editor = nullptr;

  void init()
  {
#ifdef _WIN32
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>()));
    spdlog::default_logger()->sinks().emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>()));
#endif
    spdlog::set_level(spdlog::level::trace);

    ImGui::GetIO().IniFilename = nullptr; // disable window size/pos/layout store

    ImGui::StyleColorsDark();
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_FrameBg]            = ImVec4(0.28f, 0.28f, 0.28f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.68f, 0.67f, 0.64f, 0.40f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.45f, 0.45f, 0.45f, 0.67f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_CheckMark]          = ImVec4(0.67f, 0.67f, 0.67f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.47f, 0.46f, 0.45f, 0.40f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.33f, 0.31f, 0.28f, 0.31f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.26f, 0.26f, 0.26f, 0.80f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.57f, 0.59f, 0.61f, 0.78f);
    colors[ImGuiCol_SeparatorActive]    = ImVec4(0.58f, 0.58f, 0.58f, 1.00f);
    colors[ImGuiCol_ResizeGrip]         = ImVec4(0.48f, 0.48f, 0.48f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.60f, 0.60f, 0.60f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.82f, 0.82f, 0.82f, 0.95f);
    colors[ImGuiCol_Tab]                = ImVec4(0.23f, 0.23f, 0.23f, 0.86f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 0.80f);
    colors[ImGuiCol_TabActive]          = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.05f, 0.05f, 0.05f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_DockingPreview]     = ImVec4(0.61f, 0.61f, 0.61f, 0.70f);
    colors[ImGuiCol_TextSelectedBg]     = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
    colors[ImGuiCol_NavHighlight]       = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);

    editor = nged::newImGuiNodeGraphEditor();
    editor->setResponser(std::make_shared<nged::DefaultImGuiResponser>());
    editor->setItemFactory(nged::addImGuiItems(nged::defaultGraphItemFactory()));
    editor->setViewFactory(nged::defaultViewFactory());
    editor->setNodeFactory(std::make_shared<MyNodeFactory>());
    editor->initCommands();
    nged::addImGuiInteractions();

    nged::ImGuiResource::reloadFonts();
    auto doc = editor->createNewDocAndDefaultViews();
    auto root = doc->root();
    doc->root()->createNode("in");
  }

  char const* title() { return "Demo"; }
  bool agreeToQuit()
  {
    return editor->agreeToQuit();
  }
  void update()
  {
    static auto prev= std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev);
    ImGui::PushFont(nged::ImGuiResource::instance().sansSerifFont);
    editor->update(dt.count()/1000.f);
    editor->draw();
    ImGui::PopFont();
    prev = now;
  }
  void quit()
  {
  }
}; // Demo App

int main()
{
  nged::startApp(new DemoApp());
  return 0;
}

