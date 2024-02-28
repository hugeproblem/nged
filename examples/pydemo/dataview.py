from nged import ID_None, Vec2, View, ImGui
from nged.msghub import *
from icondef import *

try:
    import pandas as pd
    df = pd.DataFrame
    has_pandas = True
except ImportError:
    df = None
    has_pandas = False


class DataView(View):
    def __init__(self, editor, doc):
        View.__init__(self, editor, None)
        self.title = 'DataView'
        self.viewingNode = ID_None
        self.autoUpdate = False
        self.lockOnNode = False
        self.windowFlags = ImGui.WindowFlags.HorizontalScrollbar

    def defaultSize(self):
        return Vec2(800, 600)

    def onDocModified(self):
        if not self.graph:
            self.lockOnNode = False
            self.viewingNode = ID_None
            return
        if self.lockOnNode and self.graph.doc.getItem(self.viewingNode) is None:
            self.lockOnNode = False
            self.viewingNode = ID_None

    def onGraphModified(self):
        pass

    def handleSelectionChangeEvent(self, view):
        if self.lockOnNode and self.graph and self.graph.doc.getItem(self.viewingNode) is not None:
            return
        if view.kind == 'network' and view.graph != self.graph:
            self.reset(view.graph)
        selection = view.solySelectedNode
        if selection:
            self.viewingNode = selection.id

    def drawData(self, data):
        ImGui.PushMonoFont()
        if data is None:
            ImGui.Text('None')
        elif type(data) in (tuple, list):
            ImGui.Text(f'{type(data).__name__}: ')
            ImGui.Text('-------------------------------')
            clipper = ImGui.ListClipper()
            clipper.Begin(len(data))
            while clipper.Step():
                for row in range(clipper.DisplayStart, clipper.DisplayEnd):
                    ImGui.Text(str(data[row]))
        elif has_pandas and isinstance(data, df):
            if ImGui.BeginTable("table", data.shape[1], flags=ImGui.TableFlags.BordersInnerV | ImGui.TableFlags.BordersOuterV | ImGui.TableFlags.BordersOuterH | ImGui.TableFlags.Resizable | ImGui.TableFlags.ScrollY | ImGui.TableFlags.ScrollX):
                ImGui.TableSetupScrollFreeze(0, 1)
                for column in data.columns:
                    ImGui.TableSetupColumn(column)
                ImGui.TableHeadersRow()
                clipper = ImGui.ListClipper()
                index = data.index
                numrows = index.size
                clipper.Begin(numrows)
                while clipper.Step():
                    for row in range(clipper.DisplayStart, clipper.DisplayEnd):
                        ImGui.TableNextRow()
                        for column in data.columns:
                            ImGui.TableNextColumn()
                            try:
                                item = str(data[column][index[row]])
                            except:
                                item = 'ERROR N/A'
                            ImGui.Selectable(item, flags=ImGui.SelectableFlags.SpanAllColumns)
                ImGui.EndTable()

        else:
            ImGui.Text(repr(data))
        ImGui.PopFont()

    def draw(self):
        ImGui.PushIconFont()
        togglePlay = ImGui.Button(
            ICON_FA_PAUSE if self.autoUpdate else ICON_FA_PLAY)
        if togglePlay:
            self.autoUpdate = not self.autoUpdate
        doUpdate = self.autoUpdate
        if not self.autoUpdate:
            ImGui.SameLine()
            doUpdate = ImGui.Button(ICON_FA_SYNC)
        ImGui.PopFont()

        if not self.graph:
            return

        node = self.graph.doc.getItem(self.viewingNode)
        ImGui.SameLine()
        if node is not None:
            _, self.lockOnNode = ImGui.Checkbox(
                f'Lock on current node ({node.name})', self.lockOnNode)

        if node is None:
            ImGui.Text("No data to view")
            return

        ctx = node.graph.doc.evalContext
        if ctx is None:
            ImGui.Text("Invalid EvaluationContext")
            return

        if not doUpdate and ctx.isDirty(self.viewingNode):
            ImGui.SameLine()
            ImGui.Text("* Cached Result:")

        ImGui.Separator()

        if doUpdate and not ctx.busy:
            ctx.addDestiny(node.id)
            ctx.evaluate(node.graph.doc)
            # ctx.prepare(node.graph)
            # viewingData = ctx.getResult(node)

        viewingData = ctx.valueCache.get(node.id, None)
        self.drawData(viewingData)


def syncDestinies(editor):
    alltargets = set()
    alldoc = set()
    for view in editor.views():
        if isinstance(view, DataView):
            if view.viewingNode != ID_None:
                alltargets.add(view.viewingNode)
            if view.graph:
                alldoc.add(view.graph.doc)
                alltargets.add(view.graph.doc.root.outputnode.id)
    for doc in alldoc:
        ctx = doc.evalContext
        invalidtargets = set()
        for dst in ctx.destinies:
            if dst not in alltargets:
                invalidtargets.add(dst)
        for dst in invalidtargets:
            ctx.destinies.discard(dst)
            debug(
                f'removing invalid destiny {dst}: {doc.getItem(dst).name if doc.getItem(dst) else "None"}')
