"""OPM-001: inspect locally installed O3DE headers; read-only, no SDK assumptions.

Usage: python engines/o3de/pc/probe/tests/opm001_inspect_o3de_headers.py --engine-root D:\\O3DE
"""
import argparse
from pathlib import Path
import re
import sys

TARGETS = {
    "MeshComponentBus.h": "Gems/AtomLyIntegration/CommonFeatures/Code/Include/AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h",
    "Model.h": "Gems/Atom/RPI/Code/Include/Atom/RPI.Public/Model/Model.h",
    "ModelAsset.h": "Gems/Atom/RPI/Code/Include/Atom/RPI.Reflect/Model/ModelAsset.h",
    "ModelLodAsset.h": "Gems/Atom/RPI/Code/Include/Atom/RPI.Reflect/Model/ModelLodAsset.h",
    "ModelAssetHelpers.h": "Gems/Atom/RPI/Code/Include/Atom/RPI.Reflect/Model/ModelAssetHelpers.h",
}
SYMBOLS = re.compile(
    r"GetModelAssetId|GetModelAssetPath|GetModel\\b|OnModelReady|OnModelPreDestroy|"
    r"GetLodAssets|GetLodCount|GetMeshes|GetSemanticBufferAssetView|"
    r"GetIndexBufferAssetView|GetBufferAsset|GetBufferViewDescriptor|GetBuffer\\b|"
    r"GetVertexCount|GetIndexCount|GetRawBuffer|GetBufferData",
    re.I,
)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--context", type=int, default=0, help="Print N surrounding lines for each matched symbol (0: matches only)")
    args = parser.parse_args()
    root = args.engine_root.expanduser().resolve()
    if not root.is_dir():
        print(f"OPM001|ENGINE_ROOT|NOT_FOUND|{root}")
        return 2
    found = 0
    for label, rel in TARGETS.items():
        path = root / rel
        if not path.is_file():
            print(f"OPM001|HEADER|NOT_FOUND|{label}|{rel}")
            continue
        found += 1
        print(f"OPM001|HEADER|FOUND|{label}|{rel}")
        lines = path.read_text(encoding="utf-8-sig", errors="replace").splitlines()
        matches = [i for i, line in enumerate(lines) if SYMBOLS.search(line)]
        if args.context < 0 or args.context > 30:
            parser.error("--context must be between 0 and 30")
        selected = set()
        for i in matches:
            selected.update(range(max(0, i - args.context), min(len(lines), i + args.context + 1)))
        for i in sorted(selected):
            kind = "SYMBOL" if i in matches else "CONTEXT"
            print(f"OPM001|{kind}|{label}|{i + 1}|{lines[i].strip()[:240]}")
    print(f"OPM001|HEADER_COUNT|{found}/{len(TARGETS)}")
    print("OPM001|RESULT|DISCOVERY_ONLY")
    return 0 if found else 1

if __name__ == "__main__":
    sys.exit(main())
