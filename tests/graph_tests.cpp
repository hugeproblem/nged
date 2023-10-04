#include <doctest/doctest.h>
#include <nged.h>

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

  graph->remove({subgraphnode->id()});
  subgraphnode.reset();
  CHECK(doc.numItems() == 3); // subgraph and its content should be gone.
}
