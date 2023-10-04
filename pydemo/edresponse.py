# editor responser

from nged import ImGui, Node
from nged.msghub import trace
from node import MyNode
from dataview import DataView
from nodelib import IONode, InputNode, OutputNode, SubgraphNode, FunctionInputNode, DefineFunction
from evaluation import getContext


def beforeItemRemoved(graph, item):
    if isinstance(item, IONode) and item.type in ('input', 'function_input', 'output'):
        return False
    else:
        return True


def beforeItemAdded(graph, item):
    if isinstance(item, InputNode) or isinstance(item, FunctionInputNode):
        if item.nthInput >= graph.numinputs:
            return False
        if item.nthInput < len(graph.inputnodes) \
                and graph.inputnodes[item.nthInput]:
            return graph.inputnodes[item.nthInput]  # replacement
    elif isinstance(item, OutputNode) and graph.outputnode is not None:
        return graph.outputnode
    else:
        return True


def afterItemAdded(graph, item):
    if isinstance(item, Node):
        getContext(graph).addDirtySource(item.id)


def onParmModified(node, parms):
    f = getattr(node, 'onParmModified', None)
    if f:
        f(parms)
    getContext(node.graph).addDirtySource(node.id)


def onInspect(view, items):
    theOnlyNode = None
    hasOnlyOneNode = True
    for item in items:
        if isinstance(item, MyNode):
            if theOnlyNode is not None:
                hasOnlyOneNode = False
            else:
                theOnlyNode = item
    if hasOnlyOneNode and theOnlyNode is not None:
        size = ImGui.GetContentRegionAvail()
        ImGui.Dummy(size)
        if hasattr(theOnlyNode, 'editingExtraParms'):
            if ImGui.CollapsingHeader('Extra Parms'):
                ImGui.PushItemWidth(-8)
                ImGui.PushMonoFont()
                modified, text = ImGui.InputTextMultiline(
                    '##ParmScript', theOnlyNode.editingExtraParms)
                theOnlyNode.editingExtraParms = text
                if ImGui.Button('Apply'):
                    theOnlyNode.setExtraParms(theOnlyNode.editingExtraParms)
                ImGui.Dummy(ImGui.ImVec2(20, 20))
                ImGui.Text(
                    "/* See https://github.com/hugeproblem/parmscript\n   for details about the syntax */")
                ImGui.PopFont()
                ImGui.PopItemWidth()


def onSelectionChanged(view):
    for otherview in view.editor.views():
        if otherview == view:
            continue
        if otherview.kind == 'data':
            assert isinstance(otherview, DataView)
            otherview.handleSelectionChangeEvent(view)


def onLinkModified(link):
    ctx = getContext(link.graph)
    ctx.topoDirty = True
    itemid = link.targetItem
    ctx.addDirtySource(itemid)
    item = link.graph.get(itemid)
    if isinstance(item, SubgraphNode):
        inputnodes = item.subgraph.inputnodes
        inputnode = inputnodes[link.targetPin] \
            if link.targetPin >= 0 and link.targetPin < len(inputnodes) \
            else None
        if inputnode:
            trace(
                f'marking subgraph input{link.targetPin} ({inputnode}) dirty')
            ctx.addDirtySource(inputnode.id)


def resolveInputOutput(graph):
    graph.inputnodes = [None] * graph.numinputs
    graph.outputnode = None
    for item in graph.items():
        if isinstance(item, InputNode) or isinstance(item, FunctionInputNode):
            assert item.nthInput >= 0 and item.nthInput < graph.numinputs,\
                f'input {item.nthInput} out of range [0, {graph.numinputs})'
            assert graph.inputnodes[item.nthInput] is None,\
                f'input {item.nthInput} already set'
            graph.inputnodes[item.nthInput] = item
            trace(f'fount new input {item.nthInput} for graph: {item.uid}')
        elif isinstance(item, OutputNode):
            assert graph.outputnode is None, 'output node already set'
            graph.outputnode = item
            trace(f'found new output for graph: {item.uid}')
        elif isinstance(item, Node):
            subgraph = item.asGraph()
            if subgraph:
                subgraph.resolveInputOutput()


def afterPaste(graph, items):
    for item in items:
        if isinstance(item, Node):
            graph = item.asGraph()
            if graph:
                resolveInputOutput(graph)


def setupCallbacks(editor):
    editor.setOnSelectionChangedCallback(onSelectionChanged)
    editor.setBeforeItemRemovedCallback(beforeItemRemoved)
    editor.setBeforeItemAddedCallback(beforeItemAdded)
    editor.setAfterItemAddedCallback(afterItemAdded)
    editor.setOnLinkSetCallback(onLinkModified)
    editor.setOnLinkRemovedCallback(onLinkModified)
    editor.setOnInspectCallback(onInspect)
    editor.setAfterPasteCallback(afterPaste)
    editor.setParmModifiedCallback(onParmModified)
