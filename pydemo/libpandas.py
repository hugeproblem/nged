from node import register_node_class, register_function_as_node, nodeFactory
from nged import NodeDesc, IconType
from icondef import *
import pandas as pd


@register_function_as_node(
    NodeDesc('read_csv', 'csv2df', iconData=ICON_FA_FILE_CSV,
             numMaxInputs=0, numOutputs=1,
             parms='''file 'csv' {label='CSV', filters='csv'}'''))
def read_csv(inputs, parms):
    with open(parms['csv'], 'rb') as f:
        df = pd.read_csv(f)
    return df


@register_function_as_node(
    NodeDesc('export_csv', 'df2csv', iconData=ICON_FA_FILE_EXPORT,
             numMaxInputs=1, numOutputs=1,
             parms='''file 'csv' {label='CSV', filters='csv', dialog='save'}'''))
def export_csv(inputs, parms):
    assert type(inputs[0]) == pd.DataFrame
    df:pd.DataFrame = inputs[0]
    df.to_csv(parms['csv'])
    return df


@register_function_as_node(
    NodeDesc('df.sort', 'df.sort', iconData=ICON_FA_SORT,
             numMaxInputs=1, numOutputs=1,
             parms='''text 'keys' {label='Keys', default=''}
                      toggle 'ascending' {label='Ascending', default=false}'''))
def sort_dataframe(inputs, parms):
    df:pd.DataFrame = inputs[0]
    keys = parms['keys'].split(',')
    return df.sort_values(by=keys, ascending=parms['ascending'])


@register_function_as_node(
    NodeDesc('df.merge', 'df.merge', iconData='merge', iconType=IconType.Text,
             numMaxInputs=2, numRequiredInputs=2, numOutputs=1,
             parms='''text 'on' {label='On'}
                      menu 'how' {items={'inner', 'left', 'right', 'outer', 'cross'}, default='inner'}
                      text 'lsuffix' {label='Left Suffix', default='_x'}
                      text 'rsuffix' {label='Right Suffix', default='_y'}'''))
def merge_dataframe(inputs, parms):
    on = parms['on']
    if on == '':
        on = None
    how = ['inner', 'left', 'right', 'outer', 'cross'][parms['how']]
    suffixes = (parms['lsuffix'], parms['rsuffix'])
    return pd.merge(inputs[0], inputs[1], on=on, how=how, suffixes=suffixes)


@register_function_as_node(
    NodeDesc('df.join', 'df.join', iconData='join', iconType=IconType.Text,
             numMaxInputs=2, numRequiredInputs=2, numOutputs=1,
             parms='''text 'on' {label='On'}
                      menu 'how' {items={'inner', 'left', 'right', 'outer', 'cross'}, default='left'}
                      text 'lsuffix' {label='Left Suffix', default='_x'}
                      text 'rsuffix' {label='Right Suffix', default='_y'}'''))
def join_dataframe(inputs: list[pd.DataFrame], parms):
    on = parms['on']
    if on == '':
        on = None
    how = ['inner', 'left', 'right', 'outer', 'cross'][parms['how']]
    return inputs[0].join(inputs[1], on=on, how=how, lsuffix=parms['lsuffix'], rsuffix=parms['rsuffix'])


@register_function_as_node(
    NodeDesc('df.rename', 'df.rename', iconData='rename', iconType=IconType.Text,
             numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
             parms='''list 'renames' {label='Renames', default=1}
                        text 'from'
                        text 'to'
                      endlist 'renames' '''))
def rename_columns(inputs: list[pd.DataFrame], parms):
    df = inputs[0]
    renames = {}
    for d in parms['renames']:
        renames[d['from']] = d['to']
    return df.rename(columns=renames)


@register_function_as_node(
    NodeDesc('df.drop', 'df.drop', iconData='drop', iconType=IconType.Text,
             numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
             parms='''text 'columns' {label='Columns', default=''}'''))
def drop_columns(inputs: list[pd.DataFrame], parms):
    df = inputs[0]
    return df.drop(columns=parms['columns'].split(','))


@register_function_as_node(
    NodeDesc('df.query', 'df.query', iconData='query', iconType=IconType.Text,
             numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
             parms='''text 'expr' {label='Expr', default=''}'''))
def drop_columns(inputs: list[pd.DataFrame], parms):
    df = inputs[0]
    return df.query(parms['expr'])
