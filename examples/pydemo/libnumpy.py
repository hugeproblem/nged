from node import register_node_class, register_function_as_node, nodeFactory
from nged import NodeDesc, IconType
from icondef import *
import numpy as np


@register_function_as_node(
    NodeDesc(
        'np.arange', 'np.arange', iconData='arange', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='float2 "range" {min=0, max=100, ui="drag", default={0,10}} float "step" {default=1, min=-10, max=10}'))
def np_arange(inputs, parms):
    return np.arange(*parms['range'], parms['step'])


@register_function_as_node(
    NodeDesc(
        'np.linspace', 'np.linspace', iconData='linspace', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='float2 "range" {min=0, max=100, ui="drag", default={0,10}} int "num" {default=50, min=1, max=1000, ui="drag"}'))
def np_linspace(inputs, parms):
    return np.linspace(*parms['range'], parms['num'])


@register_function_as_node(
    NodeDesc(
        'np.logspace', 'np.logspace', iconData='logspace', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='float2 "range" {min=0, max=100, ui="drag", default={0,10}} int "num" {default=50, min=1, max=1000, ui="drag"}'))
def np_logspace(inputs, parms):
    return np.logspace(*parms['range'], parms['num'])


@register_function_as_node(
    NodeDesc(
        'np.zeros', 'np.zeros', iconData='zeros', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='int2 "shape" {min=1, max=100, ui="drag", default={4,4}}  menu "dtype" {items={"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"}, default="f32"}'))
def np_zeros(inputs, parms):
    dtypes = [np.int8, np.uint8, np.int16, np.uint16, np.int32, np.uint32, np.int64, np.uint64, np.float32, np.float64]
    return np.zeros(tuple(parms['shape']), dtypes[parms['dtype']])


@register_function_as_node(
    NodeDesc(
        'np.ones', 'np.ones', iconData='ones', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='int2 "shape" {min=1, max=100, ui="drag", default={4,4}}  menu "dtype" {items={"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"}, default="f32"}'))
def np_ones(inputs, parms):
    dtypes = [np.int8, np.uint8, np.int16, np.uint16, np.int32, np.uint32, np.int64, np.uint64, np.float32, np.float64]
    return np.ones(tuple(parms['shape']), dtypes[parms['dtype']])


@register_function_as_node(
    NodeDesc(
        'np.eye', 'np.eye', iconData='eye', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='int "n" {min=1, max=100, default=10, ui="drag"}  menu "dtype" {items={"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"}, default="f32"}'))
def np_eye(inputs, parms):
    dtypes = [np.int8, np.uint8, np.int16, np.uint16, np.int32, np.uint32, np.int64, np.uint64, np.float32, np.float64]
    return np.eye(parms['n'], dtype=dtypes[parms['dtype']])


@register_function_as_node(
    NodeDesc(
        'np.diag', 'np.diag', iconData='diag', iconType=IconType.Text,
        numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
        parms='int "k" {min=-10, max=10, default=0, ui="drag"}'))
def np_diag(inputs, parms):
    return np.diag(inputs[0], k=parms['k'])


@register_function_as_node(
    NodeDesc(
        'np.random.rand', 'np.random.rand', iconData='rand', iconType=IconType.Text,
        numMaxInputs=0, numOutputs=1,
        parms='int2 "shape" {min=1, max=100, default={4,4}, ui="drag"}'))
def np_random_rand(inputs, parms):
    return np.random.rand(*parms['shape'])


@register_function_as_node(
    NodeDesc(
        'np.einsum', 'np.einsum', iconData='einsum', iconType=IconType.Text,
        numMaxInputs=-1, numOutputs=1,
        parms='text "subscripts" {default="...,...->..."}'))
def np_einsum(inputs, parms):
    return np.einsum(parms['subscripts'], *inputs)


@register_function_as_node(
    NodeDesc(
        'np.dot', 'np.dot', iconData='dot', iconType=IconType.Text,
        numMaxInputs=2, numRequiredInputs=2, numOutputs=1))
def np_dot(inputs, parms):
    return np.dot(*inputs)


@register_function_as_node(
    NodeDesc(
        'np.matmul', 'np.matmul', iconData='matmul', iconType=IconType.Text,
        numMaxInputs=2, numRequiredInputs=2, numOutputs=1))
def np_matmul(inputs, parms):
    return np.matmul(*inputs)


@register_function_as_node(
    NodeDesc(
        'np.tensordot', 'np.tensordot', iconData='tensordot', iconType=IconType.Text,
        numMaxInputs=2, numRequiredInputs=2, numOutputs=1,
        parms='int2 "axes" {min=1, max=10, default={1,1}, ui="drag"}'))
def np_tensordot(inputs, parms):
    return np.tensordot(*inputs, axes=parms['axes'])


@register_function_as_node(
    NodeDesc(
        'np.cross', 'np.cross', iconData='cross', iconType=IconType.Text,
        numMaxInputs=2, numRequiredInputs=2, numOutputs=1,
        parms='int "axisa" {min=0, max=10, default=0, ui="drag"} int "axisb" {min=0, max=10, default=0, ui="drag"} int "axisc" {min=0, max=10, default=-1, ui="drag"}'))
def np_cross(inputs, parms):
    return np.cross(*inputs, axisa=parms['axisa'], axisb=parms['axisb'], axisc=parms['axisc'])


@register_function_as_node(
    NodeDesc(
        'np.inner', 'np.inner', iconData='inner', iconType=IconType.Text,
        numMaxInputs=2, numRequiredInputs=2, numOutputs=1))
def np_inner(inputs, parms):
    return np.inner(*inputs)


@register_function_as_node(
    NodeDesc(
        'np.outer', 'np.outer', iconData='outer', iconType=IconType.Text,
        numMaxInputs=2, numRequiredInputs=2, numOutputs=1))
def np_outer(inputs, parms):
    return np.outer(*inputs)


@register_function_as_node(
    NodeDesc(
        'np.cast', 'np.cast', iconData='cast', iconType=IconType.Text,
        numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
        parms='menu "dtype" {items={"i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"}, default="f32"}'))
def np_cast(inputs, parms):
    dtypes = [np.int8, np.uint8, np.int16, np.uint16, np.int32, np.uint32, np.int64, np.uint64, np.float32, np.float64]
    return np.cast[dtypes[parms['dtype']]](inputs[0])


@register_function_as_node(
    NodeDesc(
        'np.reshape', 'np.reshape', iconData='reshape', iconType=IconType.Text,
        numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
        parms='text "shape" {default="4,4"}'))
def np_reshape(inputs, parms):
    return np.reshape(inputs[0], tuple(map(int, parms['shape'].split(','))))

  
@register_function_as_node(
    NodeDesc(
        'np.transpose', 'np.transpose', iconData='T', iconType=IconType.Text,
        numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
        parms='int "axis1" {min=0, max=10, default=1, ui="drag"} int "axis2" {min=0, max=10, default=0, ui="drag"}'))
def np_transpose(inputs, parms):
    return np.transpose(inputs[0], (parms['axis1'], parms['axis2']))


@register_function_as_node(
    NodeDesc(
        'np.flatten', 'np.flatten', iconData='flatten', iconType=IconType.Text,
        numMaxInputs=1, numRequiredInputs=1, numOutputs=1,
        parms='menu "order" {items={"C", "F"}, default="C"}'))
def np_flatten(inputs, parms):
    orders = ['C', 'F']
    return inputs[0].flatten(order=orders[parms['order']])
