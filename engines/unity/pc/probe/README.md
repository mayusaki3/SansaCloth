# Unity PC Coordinate Probe

## Self-contained Probe Set

Unity検証Project自体はGit管理対象にしない。SansaCloth repository側を正本とし、Probe runtime codeとReference validation baselineから自己完結したUnity `Assets/SansaCloth/Probe` を生成する。

通常の更新・配置はrepository rootから次の1コマンドで行う。

```powershell
.\engines\unity\pc\probe\BuildProbeAssets.ps1 -UnityProjectDirectory "D:\path\to\UnityProbeProject"
```

この処理は以下を行う。

- `engines/unity/pc/probe/*.cs` を `Runtime/` へ収集する。
- `reference/validation/fixture-exchange/basic-v1/` の30 JSONを収集する。
- `reference/validation/surface-response-result/basic-v1/` の30 JSONを収集する。
- `Editor/ProbeSetup.cs` を含める。
- `probe/build/Assets/SansaCloth/Probe/` を毎回クリーン生成する。
- `-UnityProjectDirectory` 指定時は対象Projectの `Assets/SansaCloth/Probe/` だけを置換する。Project内の他のAssetsは変更しない。

Unityのcompile完了後、メニューから次を実行する。

```text
SansaCloth > Probe > Setup
```

Setupは再実行可能で、`SansaClothBackendProbe` GameObjectを作成/再利用し、現在必要なComponentとTextAssetを自動設定する。

現在自動設定する検証:

- FXE-011～014: SR-001-C0-G0を自動割当。
- FXE-016: Fixture Exchange 30件を自動割当。
- FXE-017B: C0 Fixture Exchange 10件 + Reference result 10件を自動割当。

Setup成功時:

```text
SANSA_PROBE|SETUP.FIXTURE_EXCHANGE_COUNT|30
SANSA_PROBE|SETUP.SURFACE_RESPONSE_RESULT_COUNT|30
SANSA_PROBE|SETUP.FXE-011-014|READY
SANSA_PROBE|SETUP.FXE-016|READY
SANSA_PROBE|SETUP.FXE-017B|READY
SANSA_PROBE|SETUP.RESULT|PASS
```

実行もUnityメニューから行える。

```text
SansaCloth > Probe > Run FXE-011-014
SansaCloth > Probe > Run FXE-016
SansaCloth > Probe > Run FXE-017B
SansaCloth > Probe > Run All Ready Probes
```

Reference JSONはProbe配下へ二重管理しない。build/deploy時にcanonical validation baselineからコピーする。生成先 `build/` はGit管理外である。


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


## Fixture Exchange Import Probe

Rust Reference exporterで生成した `reference/validation/fixture-exchange/SR-001-C0-G0.json` をUnityへ渡し、FXE-011～013を確認する。

追加packageは不要。ImporterはValidation-onlyであり、Product serialization format/APIではない。

Unity Projectへ以下をコピーする:

- `SansaClothFixtureExchangeJson.cs`
- `SansaClothFixtureExchangeProbe.cs`
- `reference/validation/fixture-exchange/SR-001-C0-G0.json`

推奨配置:

```text
Assets/SansaCloth/Probe/SansaClothFixtureExchangeJson.cs
Assets/SansaCloth/Probe/SansaClothFixtureExchangeProbe.cs
Assets/SansaCloth/Probe/Fixtures/SR-001-C0-G0.json
```

実行:

1. Unityのcompile errorが0件であることを確認する。
2. `SansaClothBackendProbe` GameObjectへ `Sansa Cloth Fixture Exchange Probe` を追加する。
3. Inspectorの `Fixture Exchange Json` に `SR-001-C0-G0` TextAssetを割り当てる。
4. Component context menuから `Run SansaCloth Fixture Exchange Probe` を実行する。
5. Consoleの `SANSA_FXE|` 行を保存する。

Expected final:

```text
SANSA_FXE|FXE-011.RESULT|PASS
SANSA_FXE|FXE-011.VERTEX_COUNT|147
SANSA_FXE|FXE-011.TRIANGLE_COUNT|240
SANSA_FXE|FXE-011.CP_COUNT|147
SANSA_FXE|FXE-011.WIDTH_M|0.2
SANSA_FXE|FXE-011.DEPTH_M|0.1
SANSA_FXE|FXE-011.MAX_ABS_HEIGHT_M|0
SANSA_FXE|FXE-011.MAX_NORMAL_DEVIATION|0
SANSA_FXE|FXE-012.RESULT|PASS
SANSA_FXE|FXE-012.ANCHOR_COUNT|14
SANSA_FXE|FXE-012.CONTACT_INPUT_COUNT|147
SANSA_FXE|FXE-012.DIRECT_SUPPORT_COUNT|14
SANSA_FXE|FXE-012.CONTACT_ONLY_UNSUPPORTED_COUNT|133
SANSA_FXE|FXE-012.STRIP_COUNT|7
SANSA_FXE|FXE-012.STRIP_LENGTH|21
SANSA_FXE|FXE-012.WORLD_GRAVITY_M_PER_S2|0,0,0
SANSA_FXE|FXE-012.CONFORMITY|0
SANSA_FXE|FXE-012.COLLISION_TOLERANCE_M|0
SANSA_FXE|FXE-013.RESULT|PASS
SANSA_FXE|FXE-013.REJECTED_CASE_COUNT|12
```

FXE-013では各invalid caseについて `SANSA_FXE|FXE-013.<case>|REJECTED` も出力する。

このProbeがPASSしてから、同じimported SR-001を実Solver end-to-end入力へ接続する。


## FXE-014 Imported SR-001 SurfaceResponse E2E

FXE-011～013 PASS後、同じ `SR-001-C0-G0.json` を入力源として、resolved mesh SurfaceQueryとValidation-only SurfaceResponse経路を実行する。

追加ファイル:

- `SansaClothResolvedMeshSurfaceQuery.cs`
- `SansaClothImportedSurfaceResponse.cs`
- 更新版 `SansaClothFixtureExchangeProbe.cs`

`SansaClothFixtureExchangeProbe` の同じcontext menuを再実行すると、FXE-011～013に続いてFXE-014も実行される。

FXE-014 stage order:

```text
Support
-> Bridge
-> Gravity
-> SurfaceQuery
-> Conformity
-> SurfaceQuery
-> Collision
-> SurfaceQuery
```

今回のscopeは `SR-001-C0-G0` のみ。Gravity=0とConformity=0はidentity stageとして扱い、非zero値は未検証のため明示的に拒否する。

Expected:

```text
SANSA_FXE|FXE-014.RESULT|PASS
SANSA_FXE|FXE-014.CP_COUNT|147
SANSA_FXE|FXE-014.SUPPORT_COUNT|14
SANSA_FXE|FXE-014.DERIVED_CONTACT_COUNT|147
SANSA_FXE|FXE-014.MEAN_SEPARATION_M|0
SANSA_FXE|FXE-014.MAX_ABS_SEPARATION_M|0
SANSA_FXE|FXE-014.MAX_PENETRATION_M|0
SANSA_FXE|FXE-014.MAX_POSITION_DEVIATION_M|0
SANSA_FXE|FXE-014.RMS_POSITION_DEVIATION_M|0
```

Unity floatによる丸めについてはmax separation / penetration / position deviation <= 1e-6 mをPASS範囲とする。ログには実測値を出力する。

これはValidation-only E2Eであり、Production Runtime Backend完成を意味しない。


## FXE-016 30-Case Fixture Exchange Matrix Import

Reference baseline `reference/validation/fixture-exchange/basic-v1/` の30 JSONをUnityへコピーし、`SansaClothFixtureExchangeMatrixProbe`へまとめて割り当てる。

追加ファイル:

- `SansaClothFixtureExchangeMatrixProbe.cs`

Unity側の例:

```text
Assets/SansaCloth/Probe/Fixtures/BasicV1/
  SR-001-C0-G0.json
  ...
  SR-005-C1-G1.json
```

空のGameObjectへ `SansaClothFixtureExchangeMatrixProbe` を追加し、`Fixture Exchange Matrix Json` 配列へ30 TextAssetsを割り当てる。配列順は検証に使用しない。case_idで識別する。

Context menu:

```text
Run SansaCloth Fixture Exchange Matrix Probe
```

Expected final summary:

```text
SANSA_FXE|FXE-016.CASE_COUNT|30
SANSA_FXE|FXE-016.SCENARIO_COUNT|5
SANSA_FXE|FXE-016.SR003_BAKED_CONVEX_SIDE|PASS
SANSA_FXE|FXE-016.RESULT|PASS
```

各30 caseについて `FXE-016.CASE.<case_id>|PASS`、各5 scenarioについて6 casesとscenario PASSも出力する。

FXE-016はimport/input matrix検証であり、非zero Gravity / ConformityのSurfaceResponse実行はFXE-017で扱う。
