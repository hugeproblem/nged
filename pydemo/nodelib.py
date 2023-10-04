from node import MyNode, ImmediateFunctorExecutor, DeferredFunctorExecutor, register_node_class, register_function_as_node, nodeFactory
from graph import MyGraph
from nged import IconType, Node, NodeDesc, Vec2
from evaluation import Executor, GraphEvaluationContext, NodeState
from icondef import *
from nged.msghub import trace, debug, info, warn, error
from typing import Optional, Callable
import datetime
import json
import math
import os
import random
import time


def make_fixed_code_node(name, code, numinput, numoutput, parms=''):
    desc = NodeDesc(
        name, name,
        iconData=name,
        iconType=IconType.Text,
        numMaxInputs=numinput, numOutputs=numoutput,
        size=Vec2(max(50, len(name)*10+15), 20),
        parms=parms)

    class FixedCodeNode(MyNode):
        def __init__(self, parent, desc):
            MyNode.__init__(self, parent, desc)

        def settled(self):
            process = eval(
                compile('lambda inputs, parms:' + code, name, 'eval'))
            self.executor = ImmediateFunctorExecutor(self.id, process)

        def getExecutor(self):
            return self.executor

    nodeFactory().register(desc, FixedCodeNode)


make_fixed_code_node('os.stat', 'list(map(os.stat, inputs[0]))', 1, 1)
make_fixed_code_node('datetime.now', 'datetime.datetime.now()', 0, 1)
make_fixed_code_node('make_tuple', 'tuple(inputs)', -1, 1)
make_fixed_code_node('make_list', 'list(inputs)', -1, 1)
make_fixed_code_node('map', 'list(map(eval(compile(parms["lambda"], "lambda", "eval")), inputs[0]))',
                     1, 1, 'text "lambda" {default="lambda x:x", font="mono"}')
make_fixed_code_node('any', 'any(inputs[0])', 1, 1)
make_fixed_code_node('all', 'all(inputs[0])', 1, 1)
make_fixed_code_node('str', 'str(inputs[0])', 1, 1)
make_fixed_code_node('range', 'list(range(parms["range"][0], parms["range"][1], parms["step"]))',
                     0, 1, 'int2 "range" {min=0, max=100, default={0,10}} int "step" {default=1, min=-10, max=10}')


@register_function_as_node(
    NodeDesc('filter', 'filter', iconData=ICON_FA_FILTER,
             numMaxInputs=1, numOutputs=1,
             parms='text "lambda" {default="lambda x:True", font="mono"}'))
def do_filter(inputs, parms):
    return list(filter(eval(compile(parms['lambda'], 'condition', 'eval')),
                       inputs[0]))


@register_function_as_node(
    NodeDesc('readfile', 'readfile',
             iconData=ICON_FA_FILE_IMPORT,
             numMaxInputs=0, numOutputs=1,
             parms='''file "file"
             toggle "binary"
             button "reload"'''))
def readfile(inputs, parms):
    mode = 'rb' if parms['binary'] else 'r'
    with open(parms['file'], mode) as f:
        return f.read()


@register_function_as_node(
    NodeDesc('str.split', 'split',
             iconType=IconType.Text, iconData='split',
             numMaxInputs=1, numOutputs=1,
             parms='text "separator" {default=" ", font="mono"}'),
    mode='immediate')
def str_split(inputs, parms):
    assert type(inputs[0]) == str, 'feed me string pleeease'
    return inputs[0].split(
        parms['separator'].replace('\\n', '\n').replace('\\t', '\t'))


@register_function_as_node(
    NodeDesc(type='condition', label='if', iconType=IconType.Text,
             iconData='if', numMaxInputs=3, numOutputs=1),
    mode='advanced')
def conditional_expr(executor, parms, context):
    if context.fetchInput(executor.nodeid, 0):
        context.ignoreInput(executor.nodeid, 2)
        return context.fetchInput(executor.nodeid, 1)
    else:
        context.ignoreInput(executor.nodeid, 1)
        return context.fetchInput(executor.nodeid, 2)


@register_function_as_node(
    NodeDesc(
        'os.listdir', 'os.listdir',
        iconType=IconType.Text, iconData='listdir',
        numMaxInputs=0, numOutputs=1,
        parms='''dir "dir"
        toggle "recursive" {default = false}
        toggle "with_files" {default=true}
        toggle "with_dirs" {default=false}
        button "touch" {label="Trigger Reload"}'''),
    mode='immediate')
def listdir(inputs, parms):
    path = parms['dir']
    recursive = parms['recursive']
    with_files = parms['with_files']
    with_dirs = parms['with_dirs']
    result = []
    if not with_files and not with_dirs:
        return result
    if recursive:
        for root, dirs, files in os.walk(path):
            if with_dirs:
                for name in dirs:
                    result.append(os.path.join(root, name))
            if with_files:
                for name in files:
                    result.append(os.path.join(root, name))
    else:
        nofilter = with_files and with_dirs
        for name in os.listdir(path):
            fullpath = os.path.join(path, name)
            if nofilter or\
                    (with_dirs and os.path.isdir(fullpath)) or\
                    (with_files and os.path.isfile(fullpath)):
                result.append(fullpath)
    return result


# ================= CodeSnippet ====================


@register_node_class
class SimpleExpr(MyNode):
    desc = NodeDesc(
        type='expression',
        label='lambda',
        iconType=IconType.Text,
        iconData='λ',
        numMaxInputs=4,
        numOutputs=1,
        parms='''
            label 'Expr'
            separator()
            label 'lambda a, b, c, d, parms:'
            label '  ' {joinnext=true}
            text 'expr' {label='##expr', default='None', font='mono', width=-1}
        ''')

    executor: Optional[Executor]
    editingExtraParms: str

    def __init__(self, parent, desc):
        MyNode.__init__(self, parent, desc)
        self.executor = None
        self.editingExtraParms = ''

    def deserialize(self, _):
        self.editingExtraParms = self.getExtraParms()

    @staticmethod
    def callWithExpandedArgs(func):
        def f(inputs, parms):
            a, b, c, d = (inputs[i] if i < len(inputs)
                          else None for i in range(4))
            return func(a, b, c, d, parms)
        return f

    def getExecutor(self):
        if self.executor is None:
            src = self.parm('expr').value() or 'None'
            src = 'lambda a, b, c, d, parms: ' + src
            info(f'compiling source:\n{src}')
            compiled = compile(
                src, f'{self.name}_expr', 'eval', dont_inherit=True)
            func = eval(compiled)
            self.executor = ImmediateFunctorExecutor(
                self.id, SimpleExpr.callWithExpandedArgs(func))
        return self.executor

    def onParmModified(self, parms):
        if 'expr' in parms:
            self.executor = None  # need recompile


@register_node_class
class CodeSnippet(MyNode):
    desc = NodeDesc(
        type='code_snippet',
        label='code',
        iconType=IconType.IconFont,
        iconData=b"\xEF\x87\x89",
        numMaxInputs=4,
        numOutputs=1,
        color=0x81D4FAFF,
        size=Vec2(50, 40),
        parms="""
            label 'Code Snippet'
            separator()
            label 'def process(fetch, numinput, parms):' {font='mono'}
            label '  ' {joinnext=true, font='mono'}
            code 'code' {label='##code', multiline=true, font='mono', width=-1,
                         default='return fetch(0)'}
            separator()
        """)

    executor: Optional[Executor]
    editingExtraParms: str

    def __init__(self, parent, desc):
        MyNode.__init__(self, parent, desc)
        self.executor = None
        self.editingExtraParms = ''

    def __del__(self):
        pass

    def settled(self):
        self.editingExtraParms = self.getExtraParms()

    def getExecutor(self):
        if self.executor is None:
            src = self.parm("code").value()
            vars = dict()
            src = 'def process(fetch, numinput, parms):\n' + \
                '\n'.join(map(lambda s: '    '+s, src.split('\n')))
            info(f'compiling source:\n{src}')
            self.compiled = compile(
                src, f'{self.name}_code', 'exec', dont_inherit=True)
            exec(self.compiled, vars)
            self.executor = DeferredFunctorExecutor(self.id, vars['process'])
        return self.executor

    def onParmModified(self, parms):
        if 'code' in parms:
            self.executor = None  # need recompile


# ================= SubGraph ====================


class PortalExecutor(Executor):
    '''fetch result from another node'''

    def __init__(self, nodeid, srcnodeid):
        Executor.__init__(self, nodeid)
        self.srcnodeid = srcnodeid

    def execute(self, parms, context):
        return context.getResult(self.srcnodeid)


class FetchInputExecutor(Executor):
    '''fetch input from another node'''

    def __init__(self, nodeid, srcnodeid, srcpin):
        Executor.__init__(self, nodeid)
        self.srcnodeid = srcnodeid
        self.srcpin = srcpin

    def execute(self, parms, context):
        return context.fetchInput(self.srcnodeid, self.srcpin)


class ValueCacheExecutor(Executor):
    '''return a cached value withoud any evaluation'''

    value: any  # cached value

    def __init__(self, nodeid):
        Executor.__init__(self, nodeid)
        self.value = None

    def setValue(self, value):
        self.value = value

    def execute(self, parms, context):
        return self.value


@register_node_class
class SubgraphNode(MyNode):
    desc = NodeDesc(
        'subgraph',
        'subgraph',
        iconData=b"\xEF\x81\xBB",
        numMaxInputs=3,
        numOutputs=1,
        color=0xFFF59DFF,
        size=Vec2(50, 30))

    subgraph: MyGraph

    def __init__(self, parent, desc):
        MyNode.__init__(self, parent, desc)
        self.subgraph = MyGraph(parent.doc, parent, 'subgraph')
        self.subgraph.numinputs = desc.numMaxInputs
        self.subgraph.nodeid = self.uid
        trace(f'set subgraph.nodeid = {self.uid}')
        for i in range(desc.numMaxInputs):
            node = self.subgraph.createNode('input')
            node.rename(f'input{i}')
            node.nthInput = i
            self.subgraph.move(node.id, Vec2(i*200, -200))
            self.subgraph.inputnodes.append(node)
        out = self.subgraph.createNode('output')
        self.subgraph.outputnode = out
        self.subgraph.move(out.id, Vec2(200, 500))

    def settled(self):
        self.subgraph.nodeid = self.uid
        trace(f'set subgraph.nodeid = {self.uid}')

    def getExtraDependencies(self):
        return [self.subgraph.outputnode.id]

    def getExecutor(self):
        # return self.subgraph.outputnode.getExecutor()
        return PortalExecutor(self.id, self.subgraph.outputnode.id)

    def __del__(self):
        trace(f'{self}.__del__()')

    def rename(self, newname):
        if Node.rename(self, newname):
            self.subgraph.rename(newname)
            return newname
        return None

    def asGraph(self):
        return self.subgraph


class IONode(MyNode):
    def __init__(self, parent, desc):
        MyNode.__init__(self, parent, desc)


@register_node_class
class InputNode(IONode):
    desc = NodeDesc(
        'input', 'Input',
        iconData=b"\xEF\x82\xAB",
        numMaxInputs=0, numOutputs=1,
        size=Vec2(30, 30),
        hidden=True)

    nthInput: int

    def __init__(self, parent, desc):
        IONode.__init__(self, parent, desc)

    def getExtraDependencies(self):
        if self.graph.parent:
            node = self.graph.parent.getItemByUID(self.graph.nodeid)
            src = self.graph.parent.getLinkSource(node.id, self.nthInput)
            if src is not None and src[0] is not None:
                return [src[0]]
        return None

    def getExecutor(self):
        node = self.graph.parent.getItemByUID(self.graph.nodeid)
        return FetchInputExecutor(self.id, node.id, self.nthInput)

    def serialize(self):
        return f'{self.nthInput}'

    def deserialize(self, data):
        self.nthInput = int(data)


@register_node_class
class OutputNode(IONode):
    desc = NodeDesc(
        'output', 'Output',
        iconData=b"\xEF\x84\x9E",
        color=0x689F38FF,
        numMaxInputs=1, numOutputs=0,
        size=Vec2(50, 50),
        hidden=True)

    def __init__(self, parent, desc):
        IONode.__init__(self, parent, desc)

    @staticmethod
    def headof(inputs, parms):
        return inputs[0]

    def getExecutor(self):
        return ImmediateFunctorExecutor(self.id, OutputNode.headof)


# ================= Func Define ====================


def increaseSuffix(name: str) -> str:
    suffix = ''
    for i in range(len(name)):
        if name[-i-1] in '0123456789':
            suffix += name[-i-1]
    if suffix == '':
        return name + '1'
    else:
        return name[:-len(suffix)] + str(int(suffix[::-1])+1)

@register_node_class
class FunctionInputNode(IONode):
    desc = NodeDesc(
        'function_input', 'Input',
        iconData=b"\xEF\x82\xAB",
        numMaxInputs=0, numOutputs=1,
        size=Vec2(30, 30),
        hidden=True)

    nthInput: int
    executor: ValueCacheExecutor

    def __init__(self, parent, desc):
        IONode.__init__(self, parent, desc)

    def settled(self):
        self.executor = ValueCacheExecutor(self.id)

    def getExecutor(self):
        return self.executor

    def serialize(self):
        return f'{self.nthInput}'

    def deserialize(self, data):
        self.nthInput = int(data)


@register_node_class
class DefineFunction(MyNode):
    desc = NodeDesc(
        'define_function', 'function',
        iconType=IconType.Text, iconData='def',
        numMaxInputs=0, numOutputs=0)

    subgraph: MyGraph
    # evalContext: GraphEvaluationContext  # local context for this function
    invoker: 'FunctionCallExecutor'

    def __init__(self, parent, desc):
        MyNode.__init__(self, parent, desc)
        self.subgraph = MyGraph(parent.doc, parent, 'def')
        self.subgraph.numinputs = 4
        for i in range(4):
            node = self.subgraph.createNode('function_input')
            node.rename(f'input{i}')
            node.nthInput = i
            self.subgraph.move(node.id, Vec2(i*200, -200))
            self.subgraph.inputnodes.append(node)
        out = self.subgraph.createNode('output')
        self.subgraph.outputnode = out
        self.subgraph.move(out.id, Vec2(300, 500))

        self.subgraph.evalContext = GraphEvaluationContext()
        self.invoker = None

    def __del__(self):
        if self.graph is not None and hasattr(self.graph.doc, "functions"):
            self.graph.doc.functions.pop(self.name, None)
        return super().__del__()

    def asGraph(self):
        return self.subgraph

    def settled(self):
        doc = self.graph.doc
        name = self.name
        while name in doc.functions:
            name = increaseSuffix(name)
        if name != self.name:
            Node.rename(self, name)
            self.subgraph.rename(name)
        doc.functions[name] = self.id

    def rename(self, newname):
        name = self.name
        if name == newname:
            return newname
        doc = self.graph.doc
        if name in doc.functions and doc.functions[name] == self:
            del doc.functions[name]
        while newname in doc.functions:
            newname = increaseSuffix(newname)
        if newname != self.name:
            Node.rename(self, newname)
        doc.functions[newname] = self.id
        self.subgraph.rename(newname)
        return newname

    def prepare(self):
        outputid = self.subgraph.outputnode.id
        context = self.subgraph.evalContext
        context.destinies = [outputid]
        context.prepare(self.subgraph)
        dirty = context.isDirty(outputid)
        if dirty:
            for id in context.stateCache:
                context.stateCache[id] = NodeState.normal
        
        return dirty


class FunctionCallExecutor(Executor):
    def __init__(self, id, funcnode: DefineFunction):
        Executor.__init__(self, id)
        self.funcname = funcnode.name
        self.inputids = [node.id for node in funcnode.subgraph.inputnodes]
        self.inputexecutors: list[ValueCacheExecutor] = [node.getExecutor() for node in funcnode.subgraph.inputnodes]
        self.outputid = funcnode.subgraph.outputnode.id
        self.subcontext = funcnode.subgraph.evalContext
 
    def execute(self, parms, context: GraphEvaluationContext):
        numinputs = context.inputCount(self.nodeid)
        inputs = tuple(
            context.fetchInput(self.nodeid, i) if i<numinputs else None
            for i in range(4))
        subcontext = self.subcontext
        for i in range(4):
            subcontext.putValue(self.inputids[i], inputs[i]) 
            self.inputexecutors[i].setValue(inputs[i])
        subcontext.push()
        output = subcontext.getResult(self.outputid)
        subcontext.pop()
        return output


@register_node_class
class InvokeFunction(MyNode):
    desc = NodeDesc(
        'invoke_function', 'invoke',
        iconType=IconType.Text, iconData='f(x)',
        numMaxInputs=4, numOutputs=1,
        parms='''text "function" {default="function", font="mono"}''')

    executor: Optional[Executor]

    def __init__(self, parent, desc):
        MyNode.__init__(self, parent, desc)
        self.executor = None

    def prepare(self):
        doc = self.graph.doc
        if self.parm('function').value() in doc.functions:
            funcnode = doc.getItem(doc.functions[self.parm('function').value()])
            return funcnode.prepare()
        elif self.executor is not None:
            return True
        else:
            return False
        # executor = self.executor
        # if self.parm('function').value() in doc.functions:
        #     funcnode = doc.functions[self.parm('function').value()]
        #     if funcnode.prepare():
        #         executor = FunctionCallExecutor(self.id, funcnode)
        # else:
        #     executor = None

        # modified = self.executor is not executor
        # self.executor = executor
        # return modified

    def getExecutor(self):
        doc = self.graph.doc
        funcname = self.parm('function').value()
        funcnode = doc.getItem(doc.functions[funcname])
        assert funcnode is not None, f'cannot find definition of function "{funcname}"'
        self.executor = FunctionCallExecutor(self.id, funcnode)
        return self.executor
    
    def onParmModified(self, parms):
        self.executor = None
