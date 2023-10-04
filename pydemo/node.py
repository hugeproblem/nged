from nged import ItemID, Node, NodeDesc, NodeFactory, Canvas, Vec2, msghub
from nged.msghub import trace, debug, warn, error
from graph import MyGraph
from icondef import *
from evaluation import NodeState, GraphEvaluationContext, Executor, getContext
from typing import Callable


class MyNodeFactory(NodeFactory):
    def createRootGraph(self, doc):
        g = MyGraph(doc, None, 'root')
        o = g.createNode('output')
        g.outputnode = o
        return g


_nodeFactory = MyNodeFactory()


def nodeFactory():
    global _nodeFactory
    return _nodeFactory


def register_node_class(cls):
    global _nodeFactory
    _nodeFactory.register(cls.desc, cls)
    return cls


class AdvancedFunctorExecutor(Executor):
    '''deal with context yourself'''
    func: Callable[[Executor, dict[str, any], GraphEvaluationContext], any]

    def __init__(self, nodeid, func: Callable[[Executor, dict[str, any], GraphEvaluationContext], any]):
        Executor.__init__(self, nodeid)
        self.func = func

    def execute(self, parms: dict[str, any], context: GraphEvaluationContext):
        return self.func(self, parms, context)


class ImmediateFunctorExecutor(Executor):
    '''all inputs are evaluated before calling self.func'''
    func: Callable[[list[any], dict[str, any]], any]

    def __init__(self, nodeid, func: Callable[[list[any], dict[str, any]], any]):
        Executor.__init__(self, nodeid)
        self.func = func

    def execute(self, parms: dict[str, any], context: GraphEvaluationContext):
        inputs = tuple(context.fetchInput(self.nodeid, i)
                       for i in range(context.inputCount(self.nodeid)))
        return self.func(inputs, parms)


class DeferredFunctorExecutor(Executor):
    '''inputs are not evaluated, use `fetchInput` to evaluate them when needed,
    useful for conditional expressions'''
    func: Callable[[Callable[[int], any], int, dict[str, any]], any]

    def __init__(self, nodeid, func: [[Callable[[int], any], int, dict[str, any]], any]):
        Executor.__init__(self, nodeid)
        self.func = func

    def execute(self, parms: dict[str, any], context: GraphEvaluationContext):
        def fetchInput(i): return context.fetchInput(self.nodeid, i)
        return self.func(fetchInput, context.inputCount(self.nodeid), parms)


class MyNode(Node):
    errorMarkStyle = Canvas.TextStyle(
        font=Canvas.FontFamily.Icon,
        color=0xF44336FF,
        valign=Canvas.VerticalAlign.Top)
    warningMarkStyle = Canvas.TextStyle(
        font=Canvas.FontFamily.Icon,
        color=0xFB8C00FF,
        valign=Canvas.VerticalAlign.Top)
    dirtyMarkStyle = Canvas.TextStyle(
        font=Canvas.FontFamily.Icon,
        color=0xFFCC8066,
        valign=Canvas.VerticalAlign.Top)
    evaluateMarkStyle = Canvas.TextStyle(
        font=Canvas.FontFamily.Icon,
        color=0xFFEB3BFF,
        valign=Canvas.VerticalAlign.Top)
    uidMarkStyle = Canvas.TextStyle(
        color=0x66666666,
        size=Canvas.FontSize.Small)
    errorTextStyle = Canvas.TextStyle(
        font=Canvas.FontFamily.Sans,
        color=0xF44336FF,
        valign=Canvas.VerticalAlign.Top)

    def __init__(self, parent, desc):
        Node.__init__(self, parent, desc)

    def __del__(self):
        trace(f'{self.name}.__del__()')

    def getExecutor(self) -> Executor:
        return Executor(self.id)

    # called when preparing the graph for evaluation,
    # return True if the node state is changed, and may affect downstream nodes,
    # False otherwise
    def prepare(self):  
        return self.parmDirty()

    def draw(self, canvas, state):
        Node.draw(self, canvas, state)
        iconpos = self.aabb.max + Vec2(8, 0)
        icon = ''
        style = None
        ctx = getContext(self.graph)
        if ctx is not None:
            state = ctx.stateCache.get(self.id, NodeState.normal)
        else:
            state = NodeState.dirty
        if state == NodeState.error:
            icon, style = ICON_FA_EXCLAMATION_TRIANGLE, self.errorMarkStyle
        elif state == NodeState.sourcerrror:
            icon, style = ICON_FA_TIMES, self.warningMarkStyle
        elif state == NodeState.busy:
            icon, style = ICON_FA_HOURGLASS_HALF, self.evaluateMarkStyle
        elif state == NodeState.dirty:
            icon, style = ICON_FA_SYNC, self.dirtyMarkStyle
        if icon and style is not None:
            canvas.drawText(iconpos, icon, style)
        if (msg := ctx.message.get(self.id, '')):
            x = 30 if icon else 8
            canvas.drawText(self.aabb.max + Vec2(x, 0), msg, self.errorTextStyle)


def register_function_as_node(desc, mode='immediate'):
    if mode == 'immediate':
        e = ImmediateFunctorExecutor
    elif mode == 'deferred':
        e = DeferredFunctorExecutor
    elif mode == 'advanced':
        e = AdvancedFunctorExecutor
    else:
        raise NotImplementedError(f'known excution mode: {mode}')

    def func_wrapper(func):
        class FunctorNode(MyNode):
            def __init__(self, parent, desc):
                MyNode.__init__(self, parent, desc)
                self.executor = None

            def settled(self):
                self.executor = e(self.id, func)

            def getExecutor(self):
                return self.executor

        nodeFactory().register(desc, FunctorNode)
        return FunctorNode

    return func_wrapper
