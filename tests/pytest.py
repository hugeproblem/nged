from ngpy import *

class DummyNode(Node):
    def __init__(self, parent, definition):
        Node.__init__(self, parent, definition)
        print(f'{self}.__init__()')

    def __del__(self):
        print(f'{self}.__del__()')

class MyNodeFactory(NodeFactory):
    def createRootGraph(self, doc):
        return Graph(doc, None, 'root')


class DummyItem(GraphItem):
    def __init__(self, parent):
        print(f'{self}.__init__()')
        GraphItem.__init__(self, parent)

    def __del__(self):
        print(f'{self}.__del__()')

    def hitTest(self, pt):
        print(f'{self}.hitTest({pt})')
        return False


print('----init factory----')

itemFactory = builtinGraphItemFactory()
itemFactory.set('xx', True, DummyItem)

nodeFactory = MyNodeFactory()

dummyDef = NodeDef({
        'type': 'dummy',
        'label': 'Dummy',
        'category': 'Dummy Category',
        'description': 'Dummy Description',
        'numMaxInputs': 4,
        'numRequiredInputs': 1,
        'numOutputs': 1
    })

nodeFactory.register(dummyDef, DummyNode)

print(itemFactory.listNames())

print('----init doc----')
doc = Document(nodeFactory, itemFactory)
print(doc)

print('----make dummmy item----')
xx = itemFactory.make(doc.root, 'xx')
print(f'xx.factory = {itemFactory.factoryName(xx)}')

#print('----hittest----')
#hitTest(xx, Vec2(0,0))

print('----make dummy node----')
yy = doc.root.createNode('dummy')
zz = doc.root.createNode('dummy')

print('----graph api----')
yy.rename('yy')
zz.rename('zz')
link = doc.root.setLink(yy.id, 0, zz.id, 0)
print(f'link = {link}')
print(f'get({yy.id}) = {doc.root.get(yy.id)}')
tr = doc.root.traverse([zz.id])
for i in range(len(tr)):
    print(f'{i}: {tr[i]}')

print('----clean up---')
doc = None
xx = None
yy = None

