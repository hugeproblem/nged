#include <nged/nged.h>
#include <nged/nged_imgui.h>
#include <nged/style.h>
#include <nged/entry/entry.h>

#include <spdlog/spdlog.h>
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
  { "pow", 2, 1, {"float", "int"}, {"float"} },
  { "makefloat", 0, 1, {}, {"float"}},
  { "makeint", 0, 1, {}, {"int"} },
  { "floor", 1, 1, {"float"}, {"int"} },
  { "ceil", 1, 1, {"float"}, {"int"} },
  { "round", 1, 1, {"float"}, {"int"} },
  { "almost_equal", 2, 1, {"int", "float"}, {"bool"} },
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

    App::init();

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

