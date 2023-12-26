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

class DummyTypedNode : public nged::TypedNode
{
  int numInput=1;
  int numOutput=1;

public:
  DummyTypedNode(
    int numInput,
    int numOutput,
    nged::Graph* parent,
    std::string const& type,
    std::string const& name,
    nged::Vector<nged::String> intypes,
    nged::Vector<nged::String> outtypes
  ) : nged::TypedNode(parent, type, name, intypes, outtypes)
    , numInput(numInput)
    , numOutput(numOutput)
  {
  }
  nged::sint numMaxInputs() const override { return numInput; }
  nged::sint numOutputs() const override { return numOutput; }
};

struct DummyTypedNodeDef
{
  std::string type;
  int numinput, numoutput;
  nged::Vector<nged::String> intypes, outtypes;
};

static DummyTypedNodeDef defs[] = {
  { "exec", 4, 1, {"func", "any", "any", "any"}, {"any"} },
  { "null", 1, 1, {"any"}, {"any"} },
  { "sumint", 2, 1, {"int", "int"}, {"int"} },
  { "sumfloat", 2, 1, {"float", "float"}, {"float"} },
  { "lambda", 0, 1, {}, {"func"} },
  { "out", 1, 0, {"any"}, {} },
  { "in", 0, 1, {}, {"any"} }
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
    for (auto const& d: defs)
      if (d.type == type)
        return std::make_shared<DummyTypedNode>(d.numinput, d.numoutput, parent, typestr, typestr, d.intypes, d.outtypes);
    return std::make_shared<DummyTypedNode>(4, 1, parent, typestr, typestr, nged::Vector<nged::String>(), nged::Vector<nged::String>());
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

    static const std::map<std::string, uint32_t> colorhints = {
      {"int", 0xFFC107},
      {"float", 0x00ACC1},
      {"any", 0xffffff},
      {"func", 0xF44336},
    };

    for (auto&& pair: colorhints) {
      nged::TypeSystem::instance().registerType(pair.first, "", gmath::fromUint32sRGB(pair.second));
    }
    nged::TypeSystem::instance().setConvertable("int", "float");

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

  char    const* title() { return "Demo"; }
  wchar_t const* wtitle() { return L"Demo"; }
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

