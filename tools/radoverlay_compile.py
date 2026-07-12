#!/usr/bin/env python3
import argparse
import json
import os
import struct
import sys
import zlib

PROP_BOOL = 1
PROP_U32 = 2
PROP_STRING = 3
PROP_U32_ARRAY = 4
PROP_STRING_ARRAY = 5


class StringTable:
    def __init__(self):
        self.offsets = {}
        self.data = bytearray()

    def add(self, value):
        value = "" if value is None else str(value)
        if value in self.offsets:
            return self.offsets[value]
        offset = len(self.data)
        self.offsets[value] = offset
        self.data.extend(value.encode("utf-8"))
        self.data.append(0)
        return offset


def node_name(path):
    if path == "/":
        return ""
    return path.rstrip("/").rsplit("/", 1)[-1]


def parent_path(path):
    if path == "/" or "/" not in path.rstrip("/"):
        return ""
    parent = path.rstrip("/").rsplit("/", 1)[0]
    return parent if parent else "/"


def join_path(parent, child):
    if not child:
        return parent or "/"
    if child.startswith("/"):
        return child
    if parent == "/":
        return "/" + child
    return parent.rstrip("/") + "/" + child


def validate_u32(value, name):
    if not isinstance(value, int) or value < 0 or value > 0xFFFFFFFF:
        raise ValueError(f"{name} must be a uint32")
    return value


def add_property(nodes, path, name, value):
    node = nodes.setdefault(path, {})
    if isinstance(value, bool):
        node[name] = (PROP_BOOL, 1 if value else 0)
    elif isinstance(value, int):
        node[name] = (PROP_U32, validate_u32(value, name))
    elif isinstance(value, str):
        node[name] = (PROP_STRING, value)
    elif isinstance(value, list) and all(isinstance(item, int) for item in value):
        node[name] = (PROP_U32_ARRAY, [validate_u32(item, name) for item in value])
    elif isinstance(value, list) and all(isinstance(item, str) for item in value):
        node[name] = (PROP_STRING_ARRAY, list(value))
    else:
        raise ValueError(f"unsupported property type for {path}:{name}")


def add_object_as_tree(nodes, path, obj):
    nodes.setdefault(path, {})
    for key, value in obj.items():
        if isinstance(value, dict):
            add_object_as_tree(nodes, join_path(path, key), value)
        elif isinstance(value, list) and value and all(isinstance(item, dict) for item in value):
            array_root = join_path(path, key)
            nodes.setdefault(array_root, {})
            for index, item in enumerate(value):
                child_name = str(item.get("name", f"{key}@{index}"))
                child_path = join_path(array_root, child_name)
                add_object_as_tree(nodes, child_path, item)
        else:
            add_property(nodes, path, key, value)


def collect_nodes(document):
    nodes = {"/": {}}
    if "name" in document:
        add_property(nodes, "/", "name", document["name"])
    if "compatible" in document:
        add_property(nodes, "/", "compatible", document["compatible"])
    if "boot" in document:
        if not isinstance(document["boot"], dict):
            raise ValueError("boot must be an object")
        add_object_as_tree(nodes, "/boot", document["boot"])
    if "memory" in document:
        add_object_as_tree(nodes, "/memory", {"regions": document["memory"]})

    fragments = document.get("fragments", [])
    if not isinstance(fragments, list):
        raise ValueError("fragments must be an array")
    for fragment in fragments:
        if not isinstance(fragment, dict):
            raise ValueError("fragment must be an object")
        target = fragment.get("target")
        if not isinstance(target, str) or not target.startswith("/"):
            raise ValueError("fragment target must be an absolute path")
        nodes.setdefault(target, {})
        properties = fragment.get("properties", {})
        if properties:
            if not isinstance(properties, dict):
                raise ValueError("fragment properties must be an object")
            for key, value in properties.items():
                add_property(nodes, target, key, value)
        children = fragment.get("children", {})
        if children:
            if not isinstance(children, dict):
                raise ValueError("fragment children must be an object")
            for child_name, child in children.items():
                if not isinstance(child, dict):
                    raise ValueError("child node must be an object")
                child_path = join_path(target, child_name)
                nodes.setdefault(child_path, {})
                for key, value in child.items():
                    add_property(nodes, child_path, key, value)
    return nodes


def build_blob(document):
    if not isinstance(document, dict):
        raise ValueError("overlay root must be an object")
    nodes = collect_nodes(document)
    ordered_paths = sorted(nodes.keys(), key=lambda p: (p.count("/"), p))
    strings = StringTable()
    data = bytearray()
    prop_records = []
    node_records = []

    for path in ordered_paths:
        prop_start = len(prop_records)
        for name, (ptype, value) in sorted(nodes[path].items()):
            name_off = strings.add(name)
            if ptype == PROP_BOOL:
                prop_records.append((len(node_records), name_off, ptype, int(value), 0))
            elif ptype == PROP_U32:
                prop_records.append((len(node_records), name_off, ptype, int(value), 4))
            elif ptype == PROP_STRING:
                encoded = str(value)
                prop_records.append((len(node_records), name_off, ptype, strings.add(encoded), len(encoded)))
            elif ptype == PROP_U32_ARRAY:
                offset = len(data)
                for item in value:
                    data.extend(struct.pack("<I", item))
                prop_records.append((len(node_records), name_off, ptype, offset, len(value) * 4))
            elif ptype == PROP_STRING_ARRAY:
                offset = len(data)
                joined = "\0".join(value).encode("utf-8") + b"\0"
                data.extend(joined)
                prop_records.append((len(node_records), name_off, ptype, offset, len(joined)))
        node_records.append((
            strings.add(path),
            strings.add(node_name(path)),
            strings.add(parent_path(path)),
            prop_start,
            len(prop_records) - prop_start,
        ))

    payload = bytearray(strings.data)
    for record in node_records:
        payload.extend(struct.pack("<IIIII", *record))
    for record in prop_records:
        payload.extend(struct.pack("<IIIII", *record))
    payload.extend(data)
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = struct.pack(
        "<4sHHIIIIII",
        b"RADO",
        1,
        0,
        len(strings.data),
        len(node_records),
        len(prop_records),
        len(data),
        0,
        crc,
    )
    return header + payload


def main():
    parser = argparse.ArgumentParser(description="Compile RAD overlay JSON to .radoverlay")
    parser.add_argument("input")
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()
    with open(args.input, "r", encoding="utf-8") as f:
        document = json.load(f)
    blob = build_blob(document)
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(blob)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"radoverlay_compile: {exc}", file=sys.stderr)
        sys.exit(1)
