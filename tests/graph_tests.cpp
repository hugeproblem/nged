#include <doctest/doctest.h>
#include <nged/nged.h>

#include <ostream>

namespace gmath {
std::ostream& operator<<(std::ostream& os, nged::Color const& c)
{
  return os << "Color(sRGB, " << int(c.r) << "," << int(c.g) << "," << int(c.b) << "," << int(c.a) << ")";
}
}

class DummyNode : public nged::Node
{
  int numInput=1;
  int numOutput=1;

public:
  DummyNode(int numInput, int numOutput, nged::Graph* parent, std::string const& type, std::string const& name)
    : nged::Node(parent, type, name)
    , numInput(numInput)
    , numOutput(numOutput)
  {
  }
  nged::sint numMaxInputs() const override { return numInput; }
  nged::sint numOutputs() const override { return numOutput; }
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

TEST_CASE("Graph Creation") {
  auto itemfactory = nged::defaultGraphItemFactory();
  nged::NodeGraphDoc doc(std::make_shared<MyNodeFactory>(), itemfactory.get());
  doc.makeRoot();
  auto graph = doc.root();
  auto id = graph->add(nged::GraphItemPtr(doc.nodeFactory()->createNode(graph.get(), "null")));
  CHECK(id != nged::ID_None);
  CHECK(graph->get(id)->asNode() != nullptr);

  auto nodeptr = graph->createNode("exec");
  CHECK(nodeptr->asNode() != nullptr);
  CHECK(nodeptr->asNode()->numMaxInputs() == 4);
  CHECK(nodeptr->asNode()->numOutputs() == 1);

  graph->setLink(id, 0, nodeptr->id(), 0);
  CHECK(doc.numItems() == 3); // null, exec, link

  auto subgraphnode = graph->createNode("subgraph");
  auto* subgraph = subgraphnode->asGraph();
  CHECK(subgraph != nullptr);
  subgraph->createNode("null");
  CHECK(doc.numItems() == 5); // null, exec, link, subgraph, null

  SUBCASE("Graph Traverse") {
    auto exec = subgraph->createNode("exec");
    auto in1 = subgraph->createNode("null");
    auto in2 = subgraph->createNode("null");
    subgraph->setLink(in1->id(), 0, exec->id(), 0);
    subgraph->setLink(in2->id(), 0, exec->id(), 2);
    nged::GraphTraverseResult tr;
    CHECK(subgraph->travelBottomUp(tr, exec->id()));
    CHECK(tr.size() == 3);
    CHECK(tr.node(0) == exec.get());
    CHECK(tr.inputCount(0) == 3);
    CHECK(tr.inputOf(0, 0) == in1.get());
    CHECK(tr.inputOf(0, 1) == nullptr);
    CHECK(tr.inputOf(0, 2) == in2.get());
  }

  graph->remove({subgraphnode->id()});
  subgraphnode.reset();
  CHECK(doc.numItems() == 3); // subgraph and its content should be gone.
}

struct DummyTypedDef
{
  nged::String type;
  nged::String name;
  nged::Vector<nged::String> inputTypes;
  nged::Vector<nged::String> outputTypes;
};

class DummyTypedNode: public nged::TypedNode
{
public:
  DummyTypedNode(
    nged::Graph* parent,
    DummyTypedDef const& def)
    : nged::TypedNode(parent, def.type, def.name, def.inputTypes, def.outputTypes)
  {
  }

  nged::sint numMaxInputs() const override { return inputTypes_.size(); }
  nged::sint numOutputs() const override { return outputTypes_.size(); }
};

static DummyTypedDef typedDefs[] = {
  { "makeint", "makeint", {}, {"int"} },
  { "makefloat", "makefloat", {}, {"float"} },
  { "sumint", "sumint", { "int", "int" }, { "int" } },
  { "sumfloat", "sumfloat", { "float", "float" }, { "float" } },
  { "makelist", "makelist", { "any", "any", "any" }, { "list" } },
  { "reduce", "reduce", {"func", "list" }, {"any"} }
};

class MyTypedNodeFactory: public nged::NodeFactory 
{
  nged::GraphPtr createRootGraph(nged::NodeGraphDoc* root) const override
  {
    return std::make_shared<nged::Graph>(root, nullptr, "root");
  }
  nged::NodePtr  createNode(nged::Graph* parent, std::string_view type) const override
  {
    std::string typestr(type);
    for (auto const& d: typedDefs)
      if (d.type == type)
        return std::make_shared<DummyTypedNode>(parent, d);
    return nullptr;
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
    for (auto const& d: typedDefs)
      ret(context, "demo", d.type, d.name);
  }
};

TEST_CASE("TypedNode Test") {
  SUBCASE("TypeSystem Test") {
    auto& typesys = nged::TypeSystem::instance();
    typesys.registerType("int", "", {255,255,0,255});
    typesys.registerType("float");
    typesys.registerType("vec2");
    typesys.registerType("vec3");
    typesys.registerType("vec4");
    typesys.registerType("mat2");
    typesys.registerType("mat3");
    typesys.registerType("mat4");
    typesys.registerType("string");
    typesys.registerType("bool");
    typesys.setConvertable("int", "float");
    typesys.setConvertable("float", "vec2");
    typesys.setConvertable("float", "vec3");
    typesys.setConvertable("float", "vec4");
    typesys.setConvertable("int", "string");
    typesys.setConvertable("float", "string");

    CHECK(typesys.isConvertable("int", "float"));
    CHECK(typesys.isConvertable("float", "vec2"));
    CHECK(typesys.isConvertable("float", "vec3"));
    CHECK(!typesys.isConvertable("float", "int"));
    CHECK(!typesys.isConvertable("vec2", "float"));
    CHECK(!typesys.isConvertable("float", "mat4"));

    CHECK(typesys.isConvertable("int", "any"));
    CHECK(!typesys.isConvertable("any", "int"));
  }

  auto itemfactory = nged::defaultGraphItemFactory();
  nged::NodeGraphDoc doc(std::make_shared<MyTypedNodeFactory>(), itemfactory.get());
  doc.makeRoot();
  auto graph = doc.root();
  auto sumint = graph->createNode("sumint");
  CHECK(sumint->id() != nged::ID_None);
  CHECK(graph->get(sumint->id())->asNode() == sumint.get());

  auto sumfloat = graph->createNode("sumfloat");
  CHECK(sumfloat->asNode() != nullptr);
  CHECK(sumfloat->numMaxInputs() == 2);
  CHECK(sumfloat->numOutputs() == 1);

  auto makeint = graph->createNode("makeint");
  auto makefloat = graph->createNode("makefloat");
  CHECK(sumint->acceptInput(0, makeint.get(), 0));
  CHECK(!sumint->acceptInput(1, makefloat.get(), 0));
  CHECK(sumfloat->acceptInput(0, makeint.get(), 0));
  CHECK(sumfloat->acceptInput(1, makefloat.get(), 0));

  CHECK(sumint->getPinForIncomingLink(makefloat->id(), 0) == -1);
  CHECK(sumfloat->getPinForIncomingLink(makeint->id(), 0) == 0);

  CHECK(sumint->outputPinColor(0) == nged::Color{255,255,0,255});
  CHECK(sumint->inputPinColor(0) == nged::Color{255,255,0,255});
}
