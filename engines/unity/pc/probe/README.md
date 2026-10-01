# Unity PC Coordinate Probe

Runtime Backend Feasibility CheckpointのBF-001 Unit Mapping / BF-002 Coordinate Mapping / BF-003 SurfaceReference Mapping / BF-004 SurfaceQuery Feasibilityを確認する最小Probe。

これはSansaCloth SolverのUnity移植ではない。

## 対象

- UBF-001～004
- CBF-001～008
- BF-003 single-domain SurfaceReference
- BF-004 static fixture SurfaceQuery
- Unity PC

## 実行

1. Unity PC用の空Projectを作成する。
2. `SansaClothCoordinateProbe.cs` と `SansaClothSurfaceQueryProbe.cs` をProjectの `Assets/` 以下へコピーする。
3. 空GameObjectへ両Componentを追加する。
4. Componentのcontext menuから `Run SansaCloth Coordinate Probe` を実行する。
5. 続けて `Run SansaCloth Surface Query Probe` を実行する。
6. Consoleの `SANSA_BF|` で始まる行を保存する。

Scene assetやPrefabをrepositoryへ追加する必要はない。

## 出力

`SANSA_BF|<Probe Key>|<Value>`

Vectorは `x,y,z`、scalarは単一値。小数点はInvariant Cultureで出力する。

## 判定

Reference側の既知basis/rotation/windingと比較する。

特に以下を観測する:
- Unity world unitとcanonical mの1:1運用が可能か
- +X/+Y/+Z basis
- +Z周りの+90/-90 degree rotation
- V00,V01,V11 windingのgeometric normal
- basis cross products
- Body rotationとWorld Gravityの独立性
- domain_id + UVがBody Transform後も同じ論理表面を参照すること
- Surface Position / Surface Normal / signed Separationを取得できること

BF-003/004の初期Probeは単一domain・重複なしUV・static fixtureに限定する。Productionのskinned/deformed mesh mappingを完成させるものではない。

handednessはProbe結果を確認する前に確定しない。

## Checkpoint全体の非対象

BF-001～008 ProbeでSurfaceQueryとSR-001～005 fixture mappingまでは検証する。

以下はCheckpoint全体の非対象:
- Production Backend実装
- Skinned/deformed mesh production mapping
- 30 Basic Runsの数値一致
- LOD
- Performance threshold
- Mobile/Quest最適化
- Bake


## BF-005 / BF-006 SR-001 Boundary Probe

BF-001～004確認後、`SansaClothSr001BoundaryProbe.cs` も同じUnity Projectの `Assets/SansaCloth/Probe/` へコピーする。

1. `SansaClothBackendProbe` GameObjectへ `Sansa Cloth Sr001 Boundary Probe` を追加する。
2. Component context menuから `Run SansaCloth SR-001 Boundary Probe` を実行する。
3. Consoleの `SANSA_BF|SR-001.` で始まる行を保存する。

Expected aggregate:

```text
SR-001.CP_COUNT = 147
SR-001.ANCHOR_COUNT = 14
SR-001.CONTACT_INPUT_COUNT = 147
SR-001.DIRECT_SUPPORT_COUNT = 14
SR-001.CONTACT_ONLY_UNSUPPORTED_COUNT = 133
SR-001.FINAL_POSITION_DEVIATION_MAX_M = 0
SR-001.NORMAL_DEVIATION_MAX = 0
SR-001.SEPARATION_ABS_MAX_M = 0
SR-001.DERIVED_CONTACT_COUNT = 147
SR-001.WORLD_GRAVITY_INPUT = 0,0,0
SR-001.CONFORMITY_INPUT = 0
```

代表CPとしてstable id 0 / 73 / 146も出力する。

このProbeはReference SolverのUnity移植ではない。C=0/G=0/Flatで変形不要なCaseを使い、Input/Output semantic boundaryだけを検証する。


## BF-007 Basic Fixture Mapping Probe

`SansaClothBasicFixtureProbe.cs` を同じUnity Projectの `Assets/SansaCloth/Probe/` へコピーする。

1. `SansaClothBackendProbe` GameObjectへ `Sansa Cloth Basic Fixture Probe` を追加する。
2. Component context menuから `Run SansaCloth Basic Fixture Probe` を実行する。
3. Consoleの `SANSA_BF|SR-00` で始まるfixture出力を保存する。

Expected:
- 全fixture: VERTEX_COUNT=147, TRIANGLE_COUNT=240, CP_COUNT=147
- SR-001 Flat: center=(0,0,0), anchors=14
- SR-002 Convex-Up: center=(0,0.03,0), anchors=14
- SR-003 Convex-Side: center=(0.03,0,0)近傍, anchors=7
- SR-004 Concave-Shallow: center=(0,-0.02,0), anchors=14
- SR-005 Concave-Deep: center=(0,-0.05,0), anchors=14
- non-side center/first-triangle normal: +Y
- Convex-Side center/first-triangle normal: +X

Unity floatの丸め差は許容する。


## BF-008 Validation Capture Probe

`SansaClothValidationCaptureProbe.cs` を同じUnity Projectの `Assets/SansaCloth/Probe/` へコピーする。

1. `SansaClothBackendProbe` GameObjectへ `Sansa Cloth Validation Capture Probe` を追加する。
2. Component context menuから `Run SansaCloth Validation Capture Probe` を実行する。
3. Consoleの `SANSA_BF|BF-008.` 行を確認する。
4. `CAPTURE_PATH` に出力された `SansaClothValidationCapture.jsonl` を確認する。

Expected:
- RECORD_COUNT=149
- CP_COUNT=147
- CONTACT_COUNT=147
- SUPPORT_COUNT=14
- Mean/Max Separation=0
- Max Penetration=0
- Max Position Deviation=0
- JSONL = header 1 + final_cp 147 + aggregate 1

Capture fileは `Application.temporaryCachePath` に置くvalidation-only artifactであり、Production serialization formatではない。


## Checkpoint Result

2026-10-01 Unity PC実測でBF-001～008はすべてPASS。

次段階ではこのProbe群をProduction Backendとして拡張せず、Reference BasicとBackend間の最小fixture exchange formatを先に定義する。
