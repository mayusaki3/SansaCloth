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
        for index, line in enumerate(lines, 1):
            if SYMBOLS.search(line):
                print(f"OPM001|SYMBOL|{label}|{index}|{line.strip()[:220]}")
    print(f"OPM001|HEADER_COUNT|{found}/{len(TARGETS)}")
    print("OPM001|RESULT|DISCOVERY_ONLY")
    return 0 if found else 1

if __name__ == "__main__":
    sys.exit(main())
