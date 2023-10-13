from nged import Command, Shortcut, NetworkView, App, Editor, builtinGraphItemFactory, startApp
from nged.msghub import trace, debug, info, error
from dataview import DataView
from node import nodeFactory
from document import MyDocument
from edresponse import setupCallbacks
from evaluation import GraphEvaluationContext
import nodelib
try:
    import libpandas
except ImportError:
    pass
try:
    import libnumpy
except ImportError:
    pass
import dataview
import time


defaultLayout = '''
hsplit:
  data:3:hide_tab_bar
  vsplit:6
    hsplit:7
      network:5
      inspector:3:hide_tab_bar
    message:3:hide_tab_bar
'''.strip()


def makeSimpleCommand(onconfirm, name, longname, view='*', shortcut='', mayModifyGraph=True):

    class SimpleCommand(Command):
        def __init__(self):
            Command.__init__(self, name, longname, view, Shortcut(shortcut))
            self.mayModifyGraph = mayModifyGraph

        def onConfirm(self, view):
            onconfirm(view)

    return SimpleCommand()


def evalSelectedNode(view):
    assert isinstance(view, NetworkView)
    doc = view.doc or view.graph.doc
    ctx = doc.evalContext
    selection = view.solySelectedNode
    if selection:
        trace(
            f'----------- eval {selection.name} ({selection.uid}) ------------')
        ctx.addDestiny(selection.id)
        ctx.prepare(selection.graph)
        ctx.getResult(selection)


def prepareGraph(view):
    doc = view.doc or view.graph.doc
    ctx = doc.evalContext
    ctx.addDestiny(doc.root.outputnode.id)
    ctx.prepare(doc.root)


def evalGraph(view):
    doc = view.doc or view.graph.doc
    ctx = doc.evalContext
    ctx.addDestiny(doc.root.outputnode.id)
    # test pickle
    # import pickle
    # ctx.prepare(doc.root)
    # print(pickle.dumps(ctx))
    ctx.evaluate(doc)
    # ctx.prepare(doc.root)
    # result = ctx.getResult(doc.root.outputnode)
    # debug(f'root.output = {result}')

def resetEvalContext(view):
    doc = view.doc or view.graph.doc
    doc.evalContext = GraphEvaluationContext()
    info('graph evaluation context is reset')


showDataCmd = makeSimpleCommand(lambda view: view.editor.addView(
    view.doc, 'data'), 'View/Data', 'Show Datasheet', 'network', 'Ctrl+Alt+D', mayModifyGraph=False)
evalSelectedNodeCmd = makeSimpleCommand(
    evalSelectedNode, 'Eval/Selected', 'Evaluate Selected Node', 'network', 'Ctrl+F5')
evalGraphCmd = makeSimpleCommand(
    evalGraph, 'Eval/Graph', 'Evaluate Graph', 'network|inspector', 'F5')
prepareGraphCmd = makeSimpleCommand(
    prepareGraph, 'Eval/PrepareGraph', 'Prepare Graph', 'network|inspector', 'F6')
resetEvalContextCmd = makeSimpleCommand(
    resetEvalContext, 'Eval/ResetContext', 'Reset Evaluation Context', 'network|inspector', 'Alt+F5')


class MyApp(App):
    def __init__(self):
        App.__init__(self)

    def agreeToQuit(self):
        return self.editor.agreeToQuit()

    def title(self):
        return "Visual Python"

    def init(self):
        App.init(self)
        self.editor = Editor(MyDocument, nodeFactory(),
                             builtinGraphItemFactory())
        self.editor.registerView('data', DataView)
        self.editor.defaultLayoutDesc = defaultLayout
        self.editor.addCommand(showDataCmd)
        self.editor.addCommand(evalSelectedNodeCmd)
        self.editor.addCommand(evalGraphCmd)
        self.editor.addCommand(prepareGraphCmd)
        self.editor.addCommand(resetEvalContextCmd)
        self.editor.newDoc()
        setupCallbacks(self.editor)
        self.timestamp = time.process_time()

    def update(self):
        now = time.monotonic()
        deltaTime = now - self.timestamp
        try:
            dataview.syncDestinies(self.editor)
            alldocs = set()
            for view in self.editor.views():
                if view.doc is not None:
                    alldocs.add(view.doc)
            for doc in alldocs:
                if not doc.evalContext.busy and len(doc.evalContext.dirtySources) > 0:
                    doc.evalContext.prepare(doc.root)
                elif doc.evalContext.busy:
                    time.sleep(0.03)  # give some time to eval thread
        except Exception as e:
            error(f'error preparing graph: {e}')
        self.editor.update(deltaTime)
        self.timestamp = time.monotonic()
        self.editor.draw()

    def quit(self):
        self.editor = None


app = MyApp()
startApp(app)
