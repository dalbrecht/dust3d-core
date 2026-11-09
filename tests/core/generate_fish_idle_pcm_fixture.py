# SPDX-License-Identifier: MIT
"""Emit the data-only header for the unregistered private FishIdle PCM contract."""

import json
from pathlib import Path
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
TOOLKIT = ROOT / "tools" / "agent_modeling"
sys.path.insert(0, str(TOOLKIT))
from dust3d_agent.ds3 import build_document, read_ds3_model_xml, write_ds3
from dust3d_agent.spec import parse_spec

EDGE_BONES = ["Head", "BodyFront", "BodyMid", "BodyRear", "TailStart", "TailEnd"]
PARAMETERS = {"durationSeconds": "1", "frameCount": "32", "driftFactor": "3"}
SURFACE = "Water"

goldfish = json.loads((TOOLKIT / "examples" / "goldfish.json").read_text())
body = next(part for part in goldfish["parts"] if part["name"] == "body")
assert goldfish["rig"] == "Fish" and body["bones"] == EDGE_BONES[:-1]
assert "FishIdle" in goldfish["animations"]
# Keep only the existing body chain, plus one tapered TailEnd edge.
x, y, z, radius = body["nodes"][-1]
spec = parse_spec({
    "name": "fish-idle-pcm-synthetic",
    "rig": goldfish["rig"],
    "parts": [{
        "name": "body",
        "nodes": [*body["nodes"], [x, y, z - 0.09, radius / 2]],
        "bones": EDGE_BONES,
        "rounded": False,
        "subdivided": False,
    }],
    "animations": [{"type": "FishIdle", "params": {**PARAMETERS, "surfaceMaterial": SURFACE}}],
})
model, assets, _ = build_document(spec)
assert not assets
canvas = ET.fromstring(model)
assert canvas.attrib["rigType"] == "Fish"
part, = canvas.findall("parts/part")
component, = canvas.findall("components/component")
assert part.attrib["target"] == "Model" and part.attrib["disabled"] == "false"
assert component.attrib["linkDataType"] == "partId" and component.attrib["linkData"] == part.attrib["id"]
nodes = canvas.findall("nodes/node")
edges = canvas.findall("edges/edge")
assert len(nodes) == 7 and len(edges) == len(EDGE_BONES)
for entity in [part, component, *nodes, *edges]:
    identity = entity.attrib["id"]
    assert identity == "{" + str(uuid.UUID(identity)) + "}"
assert all(node.attrib["partId"] == part.attrib["id"] for node in nodes)
for edge, start, end, bone in zip(edges, nodes, nodes[1:], EDGE_BONES):
    assert edge.attrib["partId"] == part.attrib["id"]
    assert edge.attrib["from"] == start.attrib["id"] and edge.attrib["to"] == end.attrib["id"]
    assert edge.attrib["boneName"] == bone
animation, = canvas.findall("animations/animation")
assert animation.attrib["type"] == "FishIdle"
assert animation.attrib["surfaceMaterial"] == SURFACE
assert all(float(animation.attrib[name]) == float(value) for name, value in PARAMETERS.items())

template = ET.parse(ROOT / "application" / "resources" / "rig_fish.xml").getroot()
assert template.attrib["type"] == canvas.attrib["rigType"]
bones = [(bone.attrib["name"], bone.attrib["parent"]) for bone in template.findall("bone")]
names = [name for name, _ in bones]
assert names[:7] == ["Root", *EDGE_BONES] and len(set(names)) == len(names)
assert [parent for _, parent in bones[:7]] == ["", *names[:6]]
assert all(not parent or parent in names[:index] for index, (_, parent) in enumerate(bones))
assert not template.findall(".//closingAngle") and all(
    set(bone.attrib) == {"name", "parent"} for bone in template.findall("bone")
)

with tempfile.TemporaryDirectory() as scratch:
    path = Path(scratch) / "fish-idle-pcm.ds3"
    write_ds3(str(path), model, assets)
    assert read_ds3_model_xml(str(path)) == model
    archive = path.read_bytes()

print("// Generated source fixture. Do not commit this header.")
print("static constexpr uint8_t fishArchive[] {")
for offset in range(0, len(archive), 16):
    print("    " + ", ".join(f"0x{byte:02x}" for byte in archive[offset:offset + 16]) + ",")
print("};")
print("static constexpr Dust3DGeneratedRigBoneInput fishBones[] {")
for name, parent in bones:
    print(f"    {{ {json.dumps(name)}, {len(name)}, {json.dumps(parent)}, {len(parent)}, 0 }},")
print("};")
print(f'static constexpr char fishSurface[] = {json.dumps(animation.attrib["surfaceMaterial"])};')
print("static constexpr Dust3DExperimentalFishIdleParameter fishParameters[] {")
for name in PARAMETERS:
    value = animation.attrib[name]
    print(f"    {{ {json.dumps(name)}, {len(name)}, {json.dumps(value)}, {len(value)} }},")
print("};")
