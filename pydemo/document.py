import nged
from evaluation import GraphEvaluationContext


class MyDocument(nged.Document):
    evalContext: GraphEvaluationContext
    functions: dict[str, nged.ItemID]  # list of function definitions

    def __init__(self, nodefactory, itemfactory):
        nged.Document.__init__(self, nodefactory, itemfactory)
        self.evalContext = GraphEvaluationContext()
        self.functions = {}
