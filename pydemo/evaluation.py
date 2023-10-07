from nged import ItemID, ID_None, GraphTraverseResult
from nged.msghub import trace, debug, warn, error
from typing import Optional
from threading import Thread  # TODO: use multiprocessing instead
from enum import Enum

NodeState = Enum('NodeState', ['normal', 'dirty', 'busy', 'error', 'sourcerrror'])


class SourceError(Exception):
    def __init__(self, *args: object) -> None:
        super().__init__(*args)


def getContext(graph):
    '''subgraph can have their own context, otherwise the doc root holds the context'''
    ctx = None
    while graph.parent:
        ctx = getattr(graph, 'evalContext', None)
        if ctx is not None:
            break
        graph = graph.parent
    if ctx is None:
        ctx = graph.doc.evalContext
    return ctx


class Executor:

    '''this does the real job'''

    nodeid: ItemID         # for node to keep track of evaluation state

    def __init__(self, nodeid):
        self.nodeid = nodeid

    def execute(self, parms: dict[str, any], context: 'GraphEvaluationContext'):
        raise NotImplementedError('executor not defined')


class PreparedNode:

    '''node executor and its dependencies,
    with this, we don't need node its self during evaluation'''

    nodeid: ItemID
    name: str
    executor: Executor
    inputs: list[int]  # index into list of PreparedNode
    parms: dict[str, any]  # freezed parm values

    def __init__(self, gr: GraphTraverseResult, nodeid: ItemID):
        ac = gr.find(nodeid)
        self.nodeid = nodeid
        self.name = ac.node.name
        self.executor = ac.node.getExecutor()
        parms = ac.node.parm("")
        self.parms = {} if parms is None else parms.value()
        self.inputs = [ac.inputIndex(i) for i in range(ac.inputCount)]


class PreparedGraph:

    '''Ready-to-execute graph'''

    nodes: list[PreparedNode]
    nodelut: dict[ItemID, PreparedNode]

    def __init__(self, gr: GraphTraverseResult):
        nodes = []
        lut = {}
        for ac in gr:
            pnode = PreparedNode(gr, ac.node.id)
            nodes.append(pnode)
            lut[ac.node.id] = pnode
        self.nodes = nodes
        self.nodelut = lut

    def getNode(self, nodeid: ItemID) -> PreparedNode:
        return self.nodelut[nodeid]

    def getInputs(self, nodeid: ItemID, padding: int = 0) -> list[PreparedNode]:
        node = self.nodelut[nodeid]
        inputs = [self.nodes[i].executor if i>=0 else None for i in node.inputs]
        if padding > len(inputs):
            for i in range(padding - len(inputs)):
                inputs.append(None)
        return inputs


class GraphEvaluationContext:

    destinies: set[ItemID]
    dirtySources: set[ItemID]
    topoDirty: bool
    preparedGraph: Optional[PreparedGraph]
    stateCache: dict[ItemID, NodeState]
    valueCache: dict[ItemID, any]
    stateStack: list[dict[ItemID, NodeState]]  # for function call & recursion
    valueStack: list[dict[ItemID, any]]        # for function call & recursion
    # set of (targetNode, targetPin) - this is need because otherwise
    # never-evaluated false branch will always mark all downstream nodes dirty
    ignoredInput: set[tuple[ItemID, int]]
    message: dict[ItemID, str]
    busy: bool
    currentNode: ItemID
    evalThread: Optional[Thread]
    isPreparing: bool  # if true, the graph is being prepared, this is to avoid recursive prepare

    def __init__(self):
        self.destinies = set()
        self.dirtySources = set()
        self.topoDirty = True
        # self.traverseResult = None
        self.preparedGraph = None
        self.valueCache = {}
        self.stateCache = {}
        self.message = {}
        self.valueStack = []
        self.stateStack = []
        self.ignoredInput = set()
        self.busy = False
        self.currentNode = ID_None
        self.evalThread = None
        self.isPreparing = False

    def addDestiny(self, destiny):
        if destiny not in self.destinies:
            self.destinies.add(destiny)
            self.topoDirty = True

    def addDirtySource(self, nodeid):
        assert isinstance(nodeid, ItemID), "give me a ItemID"
        self.dirtySources.add(nodeid)
        self.stateCache[nodeid] = NodeState.dirty

    def clearDestinies(self):
        self.destinies = set()

    def clearCache(self, nodeid):
        assert isinstance(nodeid, ItemID), "give me a ItemID"
        if nodeid in self.valueCache:
            del self.valueCache[nodeid]

    def isDirty(self, nodeid, default=True):
        defaultstate = NodeState.dirty if default else NodeState.normal
        return self.stateCache.get(nodeid, defaultstate) == NodeState.dirty

    def updateDirtyFlags(self, graph):
        if len(self.dirtySources) == 0:
            return
        for ds in self.dirtySources:
            trace(f'dirty source: {graph.get(ds)}')
            self.stateCache[ds] = NodeState.dirty
        downstream = graph.traverse(list(self.dirtySources), 'down')
        self.dirtySources = set()
        for ac in downstream:
            node = ac.node
            nodeid = node.id
            imdirty = self.isDirty(nodeid)
            if not imdirty:
                for i in range(ac.inputCount):
                    if (nodeid, i) in self.ignoredInput:
                        continue
                    inode = ac.input(i)
                    if inode and self.isDirty(inode.id):
                        imdirty = True
                        break
                if imdirty:
                    trace(f'mark {ac.node.name} dirty')
                    self.stateCache[nodeid] = NodeState.dirty

    def prepare(self, graph):
        if self.isPreparing:
            return
        if self.busy:
            warn('evaluation is under run, cannot prepare while evaluating')
            return
        self.isPreparing = True
        # debug(f'prepare graph {graph}')
        if self.topoDirty or self.preparedGraph is None:
            # debug(f'traverse from {self.destinies} ...')
            traverseResult = graph.traverse(list(self.destinies))
            if traverseResult is None:
                self.isPreparing = False
                raise RuntimeError('ill-formed graph, failed to prepare')
            self.preparedGraph = PreparedGraph(traverseResult)
            self.topoDirty = False
        self.updateDirtyFlags(graph)
        for pnode in reversed(self.preparedGraph.nodes):
            nodeid = pnode.nodeid
            dirty = self.isDirty(nodeid, True)
            node = graph.doc.getItem(nodeid)
            if node.parmDirty():
                pnode.parms = node.parm("").value()
                node.parmMarkClean()
                dirty = True
            if node.prepare():
                dirty = True
            pnode.executor = node.getExecutor()

            if not dirty:
                for i, idx in enumerate(pnode.inputs):
                    if (nodeid, i) in self.ignoredInput:
                        continue
                    inode = self.preparedGraph.nodes[idx]
                    if inode and self.isDirty(inode.nodeid, True):
                        dirty = True
                        break
            if not dirty:
                node = graph.doc.getItem(nodeid)
                for dep in node.extraDependencies():
                    if self.isDirty(dep, False):
                        dirty = True
            if dirty:
                self.stateCache[nodeid] = NodeState.dirty

        # remove unused cache - for deleted items
        invalidKeys = []
        for key in self.valueCache:
            doc = graph.doc
            if not doc.getItem(key):
                invalidKeys.append(key)
        for key in invalidKeys:
            del self.valueCache[key]
        self.isPreparing = False

    def inputCount(self, nodeid: ItemID):
        return len(self.preparedGraph.getInputs(nodeid))

    def getInputs(self, nodeid: ItemID):
        return self.preparedGraph.getInputs(nodeid)

    def fetchInput(self, nodeid: ItemID, i: int):
        inputs = self.preparedGraph.getInputs(nodeid)
        if inputs[i] is None:
            return None
        return self.getResult(inputs[i].nodeid)

    def ignoreInput(self, nodeid: ItemID, i: int):
        self.ignoredInput.add((nodeid, i))

    def getResult(self, nodeid: ItemID) -> any:
        # trace(f'getResult({nodeid})')
        # ac = self.traverseResult.find(nodeid)
        # assert ac.valid, f'cannot find node {nodeid} in traverse result'
        # node = ac.node
        pnode = self.preparedGraph.getNode(nodeid)
        if self.stateCache.get(nodeid, NodeState.dirty) != NodeState.normal:
            self.stateCache[nodeid] = NodeState.busy
            try:
                executor = pnode.executor
                result = executor.execute(pnode.parms, self)
            except SourceError:
                self.stateCache[nodeid] = NodeState.sourcerrror
                raise
            except Exception as e:
                self.stateCache[nodeid] = NodeState.error
                self.message[nodeid] = str(e)
                raise SourceError(f'error evaluating {pnode.name}: {e}')
            trace(f'eval {pnode.name} -> {result}')
            self.valueCache[nodeid] = result
            self.stateCache[nodeid] = NodeState.normal
        else:
            result = self.valueCache[nodeid]
            trace(f'cached {pnode.name} -> {result}')
        self.stateCache[nodeid] = NodeState.normal
        return result

    def getCache(self, nodeid):
        return self.valueCache.get(nodeid, None)

    def evaluate(self, doc, root=None):
        if self.busy:
            warn('evaluation is under run, cannot evaluate while evaluating')
            return
        rootgraph = root if root is not None else doc.root
        self.message = {}
        self.prepare(rootgraph)
        dirtyDestinies = set()
        for dest in self.destinies:
            if self.isDirty(dest):
                dirtyDestinies.add(doc.getItem(dest).id)
        if len(dirtyDestinies) > 0:
            def updateDestinies():
                try:
                    for nodeid in dirtyDestinies:
                        self.getResult(nodeid)
                except Exception as err:
                    error(f'caught execption:{err}')
                    for ac in self.preparedGraph.nodes:
                        if self.stateCache[ac.nodeid] == NodeState.busy:
                            self.stateCache[ac.nodeid] = NodeState.error
                doc.readonly = False
                self.busy = False
            doc.readonly = True
            self.busy = True
            #self.evalThread = Thread(target=updateDestinies)
            #self.evalThread.start()
            updateDestinies()

    def push(self):
        self.stateStack.append(self.stateCache)
        self.valueStack.append(self.valueCache)
        self.stateCache = {}
        self.valueCache = {}

    def pop(self):
        self.stateCache = self.stateStack.pop()
        self.valueCache = self.valueStack.pop()

    def putValue(self, nodeid: ItemID, value):
        self.valueCache[nodeid] = value
        self.stateCache[nodeid] = NodeState.normal
