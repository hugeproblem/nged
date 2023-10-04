from ngpy import *
from ngpy.msghub import *
import math
import time

class Dot(GraphItem):
    '''A sample GraphItem which draws a dot and a label "dot dot dot ..."'''
    def __init__(self, parent):
        trace(f'{self}.__init__()')
        GraphItem.__init__(self, parent)

    def __del__(self):
        trace(f'{self}.__del__()')

    def hitTest(self, pt):
        if type(pt) == Vec2:
            dx = pt.x - self.pos.x
            dy = pt.y - self.pos.y
            return math.sqrt(dx*dx+dy*dy)<10
        elif type(pt) == AABB:
            return pt.expanded(10).contains(self.pos)
        else:
            return False

    def draw(self, canvas, status):
        if status == GraphItemState.SELECTED:
            color = 0xff0000ff
        else:
            color = 0xffaaaaff
        style = Canvas.ShapeStyle(1, color, 2.0, 0xffffffff)
        canvas.drawCircle(self.pos, 10, 0, style)
        textstyle = Canvas.TextStyle(color = color)
        canvas.drawText(self.pos + Vec2(14, 0), "dot dot dot ...", textstyle)

itemFactory = builtinGraphItemFactory()
itemFactory.register('dot', True, Dot)

################################################################################################

class MyNodeFactory(NodeFactory):
    def createRootGraph(self, doc):
        return Graph(doc, None, 'root')

nodeFactory = MyNodeFactory()

class DummyNode(Node):
    def __init__(self, parent, definition):
        Node.__init__(self, parent, definition)
        trace(f'{self}.__init__()')

    def __del__(self):
        trace(f'{self}.__del__()')

    def onParmModified(self, parms):
        for name in parms:
            info(f'node {self} of type {self.type}: parm "{name}" modified, new value = {self.parm(name).value()}')

        if self.type == 'output' and 'cook' in parms:
            info(f'links: {self.graph.links()}')
            t = self.graph.traverse([self.id])
            info('traverse result:')
            for n in t:
                info(str(n))

nodeDescs = [
  NodeDesc({
    'type': 'dummy',
    'label': 'Dummy',
    'category': 'Dummy Category',
    'description': 'Dummy Description',
    'iconType': IconType.Text,
    'iconData': 'D',
    'numMaxInputs': 2,
    'numRequiredInputs': 1,
    'numOutputs': 1,
    'parms': '''
        float 'x' {default=1, min=0, max=100}
        color 'c'
        toggle 'b' {label='Blah Blue Brilliant!'}
        button 'button' {label='Click Me!'}
    '''
  }),
  NodeDesc(
    type = 'output',
    label = 'Output',
    iconType = IconType.IconFont,
    iconData = b"\xEF\x84\x9E",
    color = 0x689F38FF,
    size = Vec2(50,50),
    numMaxInputs = 1,
    numOutputs = 0,
    parms = "button 'cook'"
  ),
  NodeDesc(
    type = 'input',
    label = 'Input',
    iconData = b"\xEF\x82\xAB",
    size = Vec2(50,50),
    numMaxInputs = 0,
    numOutputs = 1,
    hidden = True
  )
]

for d in nodeDescs:
    nodeFactory.register(d, DummyNode)

class SubgraphNode(Node):
    desc = NodeDesc(
        'subgraph',
        'subgraph',
        iconData = b"\xEF\x81\xBB",
        numMaxInputs = 3,
        numOutputs = 1,
        size = Vec2(50,30))

    def __init__(self, parent, desc):
        Node.__init__(self, parent, desc)
        self.subgraph = Graph(parent.doc, parent, 'subgraph')
        for i in range(desc.numMaxInputs):
            node = self.subgraph.createNode('input')
            node.rename(f'input{i}')
            self.subgraph.move(node.id, Vec2(i*200, -200))
        out = self.subgraph.createNode('output')
        self.subgraph.move(out.id, Vec2(200, 500))
        trace(f'{self}.__init__()')

    def __del__(self):
        trace(f'{self}.__del__()')

    def rename(self, newname):
        if Node.rename(self,newname):
            self.subgraph.rename(newname)
            return newname
        return None

    def asGraph(self):
        return self.subgraph

nodeFactory.register(SubgraphNode.desc, SubgraphNode)


#######################################################################################

class Greet(Command):
    def __init__(self):
        Command.__init__(self, "Python/greet", "Say Hello", "*", Shortcut('Ctrl + G'))
        self.mayModifyGraph = False

    def onConfirm(self, view):
        info('hello from python')

class ShowAbout(Command):
    def __init__(self):
        Command.__init__(self, "Help/About", "About ...", "*", Shortcut('F1'))
        self.mayModifyGraph = False

    def onConfirm(self, view):
        view.editor.addView(None,'about')

def filterInputOutputNodes(graph, item):
    if isinstance(item, DummyNode) and item.type in ('input', 'output'):
        return False
    else:
        return True

class AboutView(View):
    def __init__(self, editor, doc):
        View.__init__(self, editor, doc)
        self.title = 'About'
        self.x = 1
        self.v = (1,0,0)
    def defaultSize(self):
        return Vec2(400,600)
    def onDocModified(self):
        pass
    def onGraphModified(self):
        pass
    def draw(self):
        ImGui.Text("This is a simple node graph editor, present to you by iiif")
        ImGui.Dummy(ImGui.ImVec2(50,50))
        ImGui.Text("Belowing are some UI examples:")
        ImGui.Separator()
        _, self.x = ImGui.DragScalar('X', ImGui.DataType.Float, self.x, 1.0, 0, 100)
        _, self.v = ImGui.SliderScalar('V', ImGui.DataType.Float, self.v, -1, 1)
        if ImGui.BeginTable('A Table', 26)[0]:
            clipper = ImGui.ListClipper()
            clipper.Begin(10000000)
            while clipper.Step():
                for row in range(clipper.DisplayStart, clipper.DisplayEnd):
                    ImGui.TableNextRow()
                    for i in range(26):
                        ImGui.TableNextColumn()
                        ImGui.Text(f'{chr(i+ord("A"))}{row}')
            ImGui.EndTable()


class MyApp(App):
    def __init__(self):
        App.__init__(self)

    def agreeToQuit(self):
        return self.editor.agreeToQuit()

    def title(self):
        return "A Graph Editor"
    
    def init(self):
        App.init(self)
        self.editor = Editor(Document, nodeFactory, itemFactory)
        self.editor.registerView('about', AboutView)
        self.editor.addCommand(Greet())
        self.editor.addCommand(ShowAbout())
        self.editor.newDoc()
        self.editor.setOnItemClickedCallback(lambda view, item, button: info(f'item {item} clicked with button {button} inside view {view.kind}[{view.title}]'))
        self.editor.setBeforeItemRemovedCallback(filterInputOutputNodes)
        self.timestamp = time.process_time()

    def update(self):
        now = time.monotonic()
        deltaTime = now - self.timestamp
        self.editor.update(deltaTime)
        self.timestamp = time.monotonic()
        self.editor.draw()

    def quit(self):
        self.editor = None

app = MyApp()
startApp(app)
