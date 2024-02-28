from nged import Graph, Node, GraphTraverseResult, ItemID, ID_None
from nged.msghub import trace, debug, warn, error
from typing import Optional
from evaluation import GraphEvaluationContext
import json


class MyGraph(Graph):
    dirty: bool
    nodeid: str  # id in parent graph
    outputnode: Optional[Node]
    outputid: str  # uid
    inputnodes: list[Node]
    inputids: list[str]
    numinputs: int
    evalContext: GraphEvaluationContext

    def __init__(self, doc, parent, name):
        Graph.__init__(self, doc, parent, name)
        self.dirty = True
        self.evalContext = GraphEvaluationContext()
        self.inputnodes = []
        self.numinputs = 0
        self.outputnode = None
        self.nodeid = ''
        self.inputids = []
        self.outputid = ''

    def __del__(self):
        trace(f'{self}.__del__()')

    def serialize(self):
        return json.dumps({
            'inputnodes': list(map(lambda n: n.uid, self.inputnodes)),
            'outputnode': self.outputnode.uid if self.outputnode else '',
            'numinputs': self.numinputs})

    def deserialize(self, s):
        conf = json.loads(s)
        self.inputids = conf['inputnodes']
        self.inputnodes = list(map(self.getItemByUID, self.inputids))
        self.outputid = conf.get('outputnode', '')
        self.outputnode = self.getItemByUID(
            self.outputid) if self.outputid else None
        self.numinputs = conf['numinputs']

        self.evalContext.clearDestinies()
        self.evalContext.addDestiny(self.outputnode)
