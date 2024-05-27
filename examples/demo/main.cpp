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
    constexpr int res = 64;
    uint8_t pixels[res * res * 4] = {0};
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++)
      {
        const int index = (y * res + x) * 4;
        pixels[index + 0] = x * 4; // Red
        pixels[index + 1] = y * 4; // Green
        pixels[index + 2] = rand() & 0xff; // Blue
        float d = gmath::distance(nged::Vec2(x,y), nged::Vec2(res / 2, res / 2)); // Distance to center
        pixels[index + 3] = uint8_t(gmath::clamp((res / 2 - 1 - d) / 4.f, 0.f, 1.f) * 255.f); // Alpha
      }
    icon = nged::Canvas::createImage(pixels, res, res);
  }
  nged::sint numMaxInputs() const override { return numInput; }
  nged::sint numOutputs() const override { return numOutput; }
  bool acceptInput(nged::sint port, nged::Node const* srcNode, nged::sint srcPort) const override
  {
    // to test the sanity check, "picky" node only accept link from another "picky" node
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

    App::init();

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

