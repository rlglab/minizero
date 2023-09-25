#!/usr/bin/env python

from fileinput import filename
import os
from glob import glob
from turtle import goto
from typing import Tuple, List, Dict
import re


class Node:
    nextId: int = 0

    def __init__(self, filename: str, path: str, content: str):
        self.filename = filename
        self.path: str = path
        self.connections: List[str] = []
        self.content: str = content
        self.id: int = Node.nextId
        Node.nextId += 1


nodes: Dict[str, Node] = {}

# Find files.
dd: Tuple[str, List[str], List[str]]
for dd in os.walk("minizero"):
    for ff in dd[2]:
        if ff.endswith(".cpp") or ff.endswith(".h"):
            if ff in nodes:
                Exception("Two files with same filename found.")
            else:
                path: str = dd[0] + "/" + ff
                with open(path) as file:
                    nodes[ff] = Node(ff, path, file.read())

# Find connections.
ii: int = 0
nodesList: List[Node] = list(nodes.values())
while ii < len(nodesList):
    nn: Node = nodesList[ii]
    includes: List[str] = [
        re.split("[\"<>]", ll)[-2] for ll in nn.content.split('\n')
        if ll.startswith("#include") and not ll.endswith(">")
    ]
    includeFileNames: List[str] = [ff.split("/")[-1] for ff in includes]
    jj: int = 0
    for jj in range(len(includeFileNames)):
        fn: str = includeFileNames[jj]
        path: str = includes[jj]
        nn.connections.append(fn)
        if fn not in nodes:
            nodes[fn] = Node(fn, path, "")
    ii += 1
# Create graphml
result = ["""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!--Created by yFiles for HTML 2.5-->
<graphml xsi:schemaLocation="http://graphml.graphdrawing.org/xmlns http://www.yworks.com/xml/schema/graphml.html/2.0/ygraphml.xsd " xmlns="http://graphml.graphdrawing.org/xmlns" xmlns:demostyle2="http://www.yworks.com/yFilesHTML/demos/FlatDemoStyle/2.0" xmlns:demostyle="http://www.yworks.com/yFilesHTML/demos/FlatDemoStyle/1.0" xmlns:icon-style="http://www.yworks.com/yed-live/icon-style/1.0" xmlns:bpmn="http://www.yworks.com/xml/yfiles-bpmn/2.0" xmlns:demotablestyle="http://www.yworks.com/yFilesHTML/demos/FlatDemoTableStyle/1.0" xmlns:uml="http://www.yworks.com/yFilesHTML/demos/UMLDemoStyle/1.0" xmlns:GraphvizNodeStyle="http://www.yworks.com/yFilesHTML/graphviz-node-style/1.0" xmlns:VuejsNodeStyle="http://www.yworks.com/demos/yfiles-vuejs-node-style/1.0" xmlns:explorer-style="http://www.yworks.com/data-explorer/1.0" xmlns:y="http://www.yworks.com/xml/yfiles-common/3.0" xmlns:x="http://www.yworks.com/xml/yfiles-common/markup/3.0" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    <key id="d0" for="node" attr.type="int" attr.name="zOrder" y:attr.uri="http://www.yworks.com/xml/yfiles-z-order/1.0/zOrder"/>
    <key id="d1" for="node" attr.type="boolean" attr.name="Expanded" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/folding/Expanded">
        <default>true</default>
    </key>
    <key id="d2" for="node" attr.type="string" attr.name="url"/>
    <key id="d3" for="node" attr.type="string" attr.name="description"/>
    <key id="d4" for="node" attr.name="NodeLabels" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/NodeLabels"/>
    <key id="d5" for="node" attr.name="NodeGeometry" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/NodeGeometry"/>
    <key id="d6" for="all" attr.name="UserTags" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/UserTags"/>
    <key id="d7" for="node" attr.name="NodeStyle" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/NodeStyle"/>
    <key id="d8" for="node" attr.name="NodeViewState" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/folding/1.1/NodeViewState"/>
    <key id="d9" for="edge" attr.type="string" attr.name="url"/>
    <key id="d10" for="edge" attr.type="string" attr.name="description"/>
    <key id="d11" for="edge" attr.name="EdgeLabels" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/EdgeLabels"/>
    <key id="d12" for="edge" attr.name="EdgeGeometry" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/EdgeGeometry"/>
    <key id="d13" for="edge" attr.name="EdgeStyle" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/EdgeStyle"/>
    <key id="d14" for="edge" attr.name="EdgeViewState" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/folding/1.1/EdgeViewState"/>
    <key id="d15" for="port" attr.name="PortLabels" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/PortLabels"/>
    <key id="d16" for="port" attr.name="PortLocationParameter" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/PortLocationParameter">
        <default>
            <x:Static Member="y:FreeNodePortLocationModel.NodeCenterAnchored"/>
        </default>
    </key>
    <key id="d17" for="port" attr.name="PortStyle" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/PortStyle">
        <default>
            <x:Static Member="y:VoidPortStyle.Instance"/>
        </default>
    </key>
    <key id="d18" for="port" attr.name="PortViewState" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/folding/1.1/PortViewState"/>
    <key id="d19" attr.name="SharedData" y:attr.uri="http://www.yworks.com/xml/yfiles-common/2.0/SharedData"/>
    <data key="d19">
        <y:SharedData/>
    </data>
    <graph id="G" edgedefault="directed">"""]

# Create nodes.
for nn in nodes.values():
    if nn.filename.endswith(".cpp") and (nn.filename[:-4] + ".h") in nodes:
        continue
    if nn.filename.endswith(".h") and (nn.filename[:-2] + ".cpp") in nodes:
        nn2: Node = nodes[nn.filename[:-2] + ".cpp"]
        result.append("""
        <node id="n{0}">
			<data key="d5">
				<y:RectD X="110.875" Y="26.5" Width="300" Height="300"/>
			</data>
			<graph id="n1:" edgedefault="directed">
                <node id="n{1}">
                    <data key="d5">
                        <y:RectD X="0" Y="0" Width="30" Height="30"/>
                    </data>
                    <data key="d4">
                        <x:List>
                            <y:Label>
                                <y:Label.Text>{2}</y:Label.Text>
                            </y:Label>
                        </x:List>
                    </data>
                </node>
                <node id="n{3}">
                    <data key="d5">
                        <y:RectD X="0" Y="0" Width="30" Height="30"/>
                    </data>
                    <data key="d4">
                        <x:List>
                            <y:Label>
                                <y:Label.Text>{4}</y:Label.Text>
                            </y:Label>
                        </x:List>
                    </data>
                </node>
			</graph>
		</node>""".format(Node.nextId, nn.id, nn.filename, nn2.id, nn2.filename))
        Node.nextId += 1
    else:
        result.append("""
        <node id="n{0}">
            <data key="d5">
                <y:RectD X="0" Y="0" Width="30" Height="30"/>
            </data>
            <data key="d4">
                <x:List>
                    <y:Label>
                        <y:Label.Text>{1}</y:Label.Text>
                    </y:Label>
                </x:List>
            </data>
        </node>""".format(nn.id, nn.filename))
# Create Connections.
ind = 0
for nn in nodes.values():
    for otherFilename in nn.connections:
        if otherFilename in nodes:
            otherNode = nodes[otherFilename]
            result.append("""
        <edge id="e{0}" source="n{1}" target="n{2}"></edge>""".format(ind, nn.id, otherNode.id))
            ind += 1

# Append ending.
result.append("""
    </graph>
</graphml>""")

# print("".join(result))

with open("tools/dependency_graph_generator/dependency_graph.graphml", "w") as ff:
    ff.write("".join(result))

print("Generated file \"tools/dependency_graph_generator/dependency_graph.graphml\"")
print("Load this file to https://www.yworks.com/yed-live/ and press the automatic layout button.")
