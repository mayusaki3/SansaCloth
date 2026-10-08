# O3DE PC Runtime Backend Validation / O3DE PC Runtime Backend 検証

> Draft verification plan for the second SansaCloth Runtime Backend target.

## 1. 目的

Unity PCで成立したSansaCloth Runtime Backend境界を、2つ目のBackendであるO3DE PCへ適用できるか確認する。

本検証ではUnity固有実装を共通仕様へ昇格させない。O3DEで同じ意味境界が成立した項目だけを、後続でBackend-neutral contract候補として扱う。

## 2. 前提

- Canonical Lengthはm。
- Canonical axis meaningは +X=right / +Y=up / +Z=forward。
- Canonical handednessは未確定であり、axis名だけから決定しない。
- World GravityはBody rotationから暗黙生成せず、world-space vectorとして明示入力する。
- Fixture Exchange v0とValidation Result artifactはUnity検証で確定したものを変更せず再利用する。
- Reference固有settingをFixture Exchangeへ追加しない。
- O3DE固有都合でCore/SurfaceResponse semanticsを変更しない。

## 3. O3DE公式仕様から確認できる事項

2026-10-07時点のO3DE公式資料では以下を確認できる。

- base measurement unitは1 world unit = 1 m。
- +Zがup axis。
- scene assetのforward/view axisは+Yを基準に扱える。
- Transform translationはm。
- World spaceとlocal spaceを区別できる。
- AZ::Vector3はX/Y/Z basisとCross product APIを持つ。
- O3DE公式資料にはcoordinate systemのhandednessをright-handedとする記述とleft-handedとする記述が混在する。

このためhandednessは文書記述だけで固定せず、runtime probeでrotation / cross product / triangle windingを確認する。

## 4. 初期候補写像

Canonical:

~~~text
+X = right
+Y = up
+Z = forward
~~~

O3DE:

~~~text
+X = right
+Y = forward
+Z = up
~~~

したがってPosition/Direction/Normalの初期候補は:

~~~text
canonical (x, y, z) -> O3DE (x, z, y)
~~~

とする。

これはruntime確認前の候補であり、rotation sign / winding / handednessを含む正式変換ではない。

## 5. Phase OBF-01: Unit / Coordinate Probe

最初のProbeはSolver、SurfaceQuery、Fixture Exchange importerを実装しない。

Editor Python Bindingsを使い、O3DE math/transform runtimeから観測値を出力する。

### OBF-001 Unit Mapping

確認:
- 1 O3DE world unitを1 canonical mとして扱える。
- canonical cloth width 0.20mを0.20 O3DE world unitへ写像する。
- Backend境界で追加length scaleを導入しない。

PASS条件:
- unit mappingがidentity scaleで定義可能。
- asset import側のscale変換とSansaCloth canonical unitを分離できる。

### OBF-002 Axis Basis Mapping

確認:
- O3DE basis X/Y/Zを観測する。
- canonical +X -> O3DE +X候補。
- canonical +Y(up) -> O3DE +Z候補。
- canonical +Z(forward) -> O3DE +Y候補。

PASS条件:
- Position/Direction/Normalのcomponent mappingを一意に定義できる。

### OBF-003 Rotation Mapping

確認:
- O3DE +90/-90 degree rotation around X/Y/Z。
- 特にcanonical +Z rotationに対応するO3DE +Y/-Y rotationの符号。
- canonical +90 Z applied to +X = canonical +Yとの対応。

PASS条件:
- canonical rotationをO3DE rotationへ変換するaxis/sign ruleを一意に定義できる。

### OBF-004 Cross Product Orientation

確認:
- X cross Y
- Y cross Z
- Z cross X

Editor Python BindingからCrossを直接観測できる場合はPython Probeで確認する。

Cross APIがPythonへ露出していない場合は **OPENのまま** とし、値を独自計算してPASS扱いにしない。後続C++ ProbeでAZ::Vector3::Crossを直接確認する。

### OBF-005 Triangle Winding

既知triangleからgeometric normalをO3DE runtime APIまたは最小C++ Probeで確認する。

PASS条件:
- Fixture Exchange triangle windingから期待するSurface Normalへ変換する規則を一意に定義できる。

### OBF-006 World Gravity Independence

検証対象は **SansaCloth Runtime Backend境界で外部入力されるWorld Gravity** であり、O3DE Physics Sceneの既定GravityやPhysXの動作ではない。

検証内容:
- Canonical World Gravity（m/s²）を明示入力し、O3DEのworld-space vectorへ `(x,y,z) -> (x,z,y)` で写像する。
- Body Transformを独立に設定する。GravityをBody rotationから生成・暗黙回転しない。
- Body-local Gravityが必要な場合のみO3DE `Transform::GetInverse().TransformVector()` で変換する。
- O3DE `Transform::TransformVector()` でworldへ戻し、元のworld-space入力と許容差 `1e-5` 以内で一致することを確認する。
- 非自明なrotationではbody-local Gravityがworld-space入力と異なることも確認し、identity変換だけによる見かけのPASSを避ける。
- ゼロGravityを含む4ケースで検証する。

| ケース | Canonical World Gravity (m/s²) | Body Transform | 確認 |
|---|---|---|---|
| C01 | (1.25, -3.75, 2.5) | identity | world/local一致、round-trip |
| C02 | (1.25, -3.75, 2.5) | O3DE Y +90° | localはworldと異なる、round-trip |
| C03 | (0, -9.81, 0) | O3DE X -90° | localはworldと異なる、round-trip |
| C04 | (0, 0, 0) | O3DE Y +90° | zero維持、round-trip |

実装:
- validation-only C++ Gem `SansaClothBackendProbeValidation` の `RunObf006` をEditor Pythonから呼び出す。
- O3DE `AZ::Vector3` / `AZ::Transform` のruntime math APIのみ使用する。
- PhysX5、`AzPhysics::SceneInterface`、Physics Scene、RigidBodyを必要としない。
- ProbeはGravityを外部入力として扱う**座標・意味境界の検証**であり、未実装のSansaCloth production Runtime BackendでGravityが正しく使われることまで証明しない。

2026-10-08の経緯:
- 旧OBF-006はO3DE Physics SceneからGravityを取得しようとしたが、Editor runtimeで `SceneInterface unavailable` となった。
- 検証対象が「外部World Gravity入力」であることを再確認し、Physics Scene方式を撤回した。
- 一時的に追加したPhysX5依存も撤回した。SansaClothのRuntime Backend契約にはPhysX5を要求しない。
- 新方式のC++ ProbeはO3DE 26.05 Editorで2026-10-08にruntime実行済み。C01～C04、OBF-006、Python bridge、OBF-01はすべてPASS。ビルド成功はGemロードとC++関数実行から確認できるが、ビルドログ自体はこの回では提示されていない。

## 6. Phase判定

O3DE BF-001 Unit Mapping:
- OBF-001 PASSでPASS候補。

O3DE BF-002 Coordinate Mapping:
- OBF-002～006をすべてruntime確認してからPASSとする。
- Python Binding不足による未観測項目はC++ Probeへ移し、推測でPASSにしない。

現在状態:
- OBF-001: **PASS** (2026-10-07)
- OBF-002: **PASS** (2026-10-07)
- OBF-003: **PASS** (2026-10-07)
- OBF-004: **PASS** (2026-10-07)
- OBF-005: **PASS** (2026-10-08)
- OBF-006: **PASS** (2026-10-08, external World Gravity runtime math probe, C01～C04)
- BF-001: **PASS** (2026-10-07)
- BF-002: **PASS** (2026-10-08, OBF-002～006 runtime evidence; coordinate/semantic mapping scope)

2026-10-07 runtime evidence:
- OBF-01 Python Probe: PASS。
- O3DE basis X/Y/Z = (1,0,0)/(0,1,0)/(0,0,1)。
- Position/Direction/Normalのcomponent候補写像は canonical (x,y,z) -> O3DE (x,z,y)。
- O3DE +90deg Zは+Xを+Yへ回転。
- canonical +90deg Zは、候補写像後のO3DE空間ではO3DE -90deg Yに対応し、+Xを+Zへ回転することをruntime確認。
- O3DE Vector3.Crossは X cross Y=+Z、Y cross Z=+X、Z cross X=+Y。
- よってO3DE math runtimeは右手系orientationを示す。
- canonical->O3DE component写像はY/Z交換でorientation reversingであるため、rotation axial vectorはpolar vectorと同じ写像にせず、符号を含めて扱う。
- rotation-vector候補写像は canonical (rx,ry,rz) -> O3DE (-rx,-rz,-ry)。
- OBF-005 runtime evidence: canonical +Y normal triangleをcomponent変換すると、同一windingではO3DE normal=(0,0,-1)、winding反転では(0,0,+1)となった。
- expected mapped canonical normal=(0,0,+1)に一致するのはreversed windingのみ。
- よってFixture Exchange triangleをO3DEへ渡す際は、component変換に加えてtriangle windingを反転する。
- OBF-005: PASS。

2026-10-08 OBF-006 runtime evidence (O3DE 26.05 Editor):
- C++ Validation GemがPythonから公開され、`OBF-006.DISCOVERY|CPP_PROBE_AVAILABLE` を確認。
- C01 arbitrary gravity + identity: world=(1.25,2.5,-3.75), local=(1.25,2.5,-3.75), reconstructed=(1.25,2.5,-3.75), PASS。
- C02 arbitrary gravity + O3DE Y +90°: world=(1.25,2.5,-3.75), local=(3.75000024,2.50000024,1.24999976), reconstructed=(1.25000024,2.50000048,-3.75000048), PASS。
- C03 vertical gravity + O3DE X -90°: world=(0,0,-9.81000042), local=(0,9.80999947,0), reconstructed=(0,0,-9.80999947), PASS。
- C04 zero gravity + O3DE Y +90°: world/local/reconstructed=(0,0,0), PASS。
- `OBF-006.RESULT|PASS`, `OBF-006.PYTHON_BRIDGE_RESULT|PASS`, `OBF-01.PYTHON_PROBE_RESULT|PASS` を確認。
- OBF-001～006すべてPASSによりBF-001/BF-002をPASSと判定。
- 本判定はO3DE座標・意味境界の検証に限り、production Runtime Backend / Solver / SurfaceQueryを検証したものではない。

## 7. Phase OBF-02: BF-003 SurfaceReference Mapping

Unity PC BF-003のrigid single-domain fixtureに対応するO3DE validation-only Probe。

論理参照:
- `SurfaceReference = (DomainId, U, V)`
- `DomainId = 1`, `UV = (0.25, 0.75)`
- `SurfaceReference` にtriangle indexを保存しない。
- 0.20m x 0.10mのflat fixture、4 vertices、2 triangles、fixed diagonal V00 -> V11。
- canonical->O3DE軸写像 `(x,y,z)->(x,z,y)` に伴い、triangle windingを反転する。
- O3DE `AZ::Vector2`, `AZ::Vector3`, `AZ::Transform` を用いてUV triangleをbarycentric解決する。

Probe:
- SRF-001: identityでDomainId/UVからsurface local positionを解決する。期待O3DE `(-0.05,0.025,0)`。
- SRF-002: Body rigid transform後も同一DomainId/UVを使う。canonical rotation Z -90°はO3DE Y +90°、canonical translation (0.30,0.20,-0.10)はO3DE (0.30,-0.10,0.20)。期待O3DE world position `(0.30,-0.075,0.25)`。
- SRF-003: triangle配列順序を交換しても、同じDomainId/UVが同じsurface positionを解決する。resolverが返すtriangle indexは変わることを確認する。
- SRF-004: 異なるDomainId=2、範囲外UV=(1.25,0.75)は明示的にrejectする。

検証範囲:
- 本Probeは**O3DE math runtime上のvalidation fixture mapping**。O3DE Mesh APIや実際のskinned/deformed meshを読み取るものではない。
- UV seam/overlap、複数domain、topology更新に対するproduction stable mappingは未確定。
- Triangle配列の**順序変更**を確認するものであり、meshの**topology変更**への追従を保証しない。
- 本validation fixture範囲のBF-003判定は2026-10-08のruntime evidenceに基づきPASSとする。production stable mappingは別途検証する。

実装:
- C++ Gem: `SansaClothBackendProbeValidationSystemComponent.cpp` の `RunBf003`。
- Editor Python: `sansacloth_surface_reference_probe.py`。
- `BuildProbeAssets.ps1` は2本のPython scriptとValidation Gemを配置する。

現在状態:
- BF-003: **PASS** (2026-10-08, O3DE 26.05 Editor, validation-only single-domain rigid fixture; actual Mesh API / deformed mesh未検証)。
- BF-004: OPEN。Surface Normal/signed SeparationはBF-004で検証する。

2026-10-08 BF-003 runtime evidence (O3DE 26.05 Editor):
- `BF-003.DISCOVERY|CPP_PROBE_AVAILABLE` によりC++ Validation GemとPython bridgeの利用を確認。
- DomainId=1、UV=(0.25,0.75)。
- Identity resolved triangle=0、surface position=(-0.0500000007,0.0250000004,0)。
- Body transform後のsurface position=(0.300000012,-0.075000003,0.25)。
- Triangle配列順序変更後のresolved triangle=1、surface position=(-0.0500000007,0.0250000004,0)。
- `BF-003.IDENTITY_RESULT|PASS`、`BF-003.TRANSFORM_RESULT|PASS`、`BF-003.TRIANGLE_REORDER_RESULT|PASS`。
- `BF-003.INVALID_DOMAIN_REJECTED|TRUE`、`BF-003.INVALID_UV_REJECTED|TRUE`。
- `BF-003.RESULT|PASS`、`BF-003.PYTHON_BRIDGE_RESULT|PASS`、`BF-003.PYTHON_PROBE_RESULT|PASS`。
- 以上からvalidation fixture範囲のBF-003はPASS。production Mesh API、skinned/deformed mesh、UV seam/overlap、topology更新は検証範囲外。


## 8. Phase OBF-03: BF-004 SurfaceQuery Feasibility

Unity PC BF-004のrigid flat fixture semanticsをO3DE 26.05 Editorのvalidation-only C++ Gemで検証する。BF-003と同じ0.20m x 0.10m、DomainId=1、UV=(0.25,0.75)、4 vertices / 2 trianglesのfixtureを使用する。

SurfaceQuery input:
- `SurfaceReference = (DomainId, U, V)`
- `Current Position`（O3DE world-space, m）
- `Body Transform`（rigid）

SurfaceQuery output:
- `Surface Position`（UV triangle barycentric interpolation後のworld-space）
- `Surface Normal`（triangle windingから`(V1-V0).Cross(V2-V0)`を計算し、rigid Transformでworldへ写像・正規化）
- `signed Separation = Dot(Current Position - Surface Position, Surface Normal)`（m）

検証ケースと独立した期待値:

| Case | 入力 | 期待値 |
|---|---|---|
| SQF-001 | Identity、UV=(0.25,0.75) | Position=(-0.05,0.025,0), Normal=(0,0,+1) |
| SQF-002 | Identity、Surface+expected Normal×0.01m | Separation=+0.01m |
| SQF-003 | Identity、Surface−expected Normal×0.01m | Separation=−0.01m |
| SQF-004 | O3DE Y +90°、Translation=(0.30,-0.10,0.20) | Position=(0.30,-0.075,0.25), Normal=(+1,0,0) |
| SQF-005 | Transform後、Surface+expected Normal×0.01m | Separation=+0.01m |
| SQF-006 | Transform後、Surface−expected Normal×0.01m | Separation=−0.01m |
| SQF-007 | Identity、Surface+X×0.01m（接線方向） | Separation=0m |

期待位置と期待NormalはQuery計算結果から生成せず、既知fixtureの定数として与える。Normalの符号が逆転した場合に、Separationだけの自己整合性で誤PASSにならないようにする。許容誤差は `1e-5`（Position/Normal/Separationの数値比較）。

実装:
- `RunBf004()`（`SansaClothBackendProbeValidationSystemComponent.cpp`）をBehaviorContextに公開。
- `sansacloth_surface_query_probe.py` からC++ Probeを呼ぶ。
- `BuildProbeAssets.ps1` は3本のPython ProbeとValidation Gemを配置。
- PhysX/Physics Sceneは不要。

検証範囲:
- **O3DE math runtime上のrigid flat validation fixture**。O3DE Mesh API、Skinned Mesh、deformed mesh、non-uniform scale、curved surface、production solverは未検証。
- 完全なSurfaceFrame tangent U/Vは本Checkpointで要求しない。
- BF-004は **PASS** (2026-10-08、O3DE 26.05 Editor runtime、validation-only rigid flat fixture)。C++ Gemがロードされ、7件の数値検証とPython bridgeがすべてPASS。

2026-10-08 BF-004 runtime evidence:
- `BF-004.DISCOVERY|CPP_PROBE_AVAILABLE`
- Identity Surface Position = (-0.0500000007, 0.0250000004, 0)
- Identity Surface Normal = (0, 0, 1)
- Identity outward Separation = +0.00999999978 m
- Identity inward Separation = -0.00999999978 m
- Body Transform後のSurface Position = (0.300000012, -0.075000003, 0.25)
- Body Transform後のSurface Normal = (1, 0, 8.94069672e-08)
- Body Transform後のoutward Separation = +0.00999999046 m
- Body Transform後のinward Separation = -0.00999999046 m
- Identity tangent Separation = 0 m
- `BF-004.SQF-001.RESULT|PASS` ～ `BF-004.SQF-007.RESULT|PASS`
- `BF-004.RESULT|PASS`、`BF-004.PYTHON_BRIDGE_RESULT|PASS`、`BF-004.PYTHON_PROBE_RESULT|PASS`

判定は上記validation fixture範囲に限定する。実O3DE Mesh API、Skinned/deformed mesh、non-uniform scale、curved surface、production solverの動作保証ではない。

## 9. Phase OBF-04: BF-005 / BF-006 SR-001 Semantic Boundary

Unity PCの`UNITY-SR-001-C0-G0`と同じSR-001 Flat入力意味論を、O3DE 26.05 C++ Validation Gemで確認する。

対象Case:
- Body Surface: SF-FLAT-001（validation-only analytic patch）
- Cloth: CF-FLAT-001
- CP: 21 U samples × 7 V samples = 147
- Placement: PL-CONTACT-001（flat contact）
- Anchor: AF-BOTH-EDGES-001（U=0, U=1の14点）
- Gravity: Off = canonical (0,0,0) m/s²。O3DE World Gravityへ明示変換する。
- Conformity: 0
- CollisionTolerance: 0m
- SurfaceReference: DomainId=1 + normalized U/V（triangle indexは含めない）

O3DE座標写像:
- Canonical CP Position = `((u-0.5)×0.20, 0, (v-0.5)×0.10)` [m]
- O3DE CP Position = `((u-0.5)×0.20, (v-0.5)×0.10, 0)` [m]
- Surface Normal = `(0,0,+1)`（canonical +Yから写像）
- Stable CP ID = `vIndex×21 + uIndex`。代表IDは0/73/146。

BF-005 Input Semantics:
- ISF-001: 147 CPの構成。
- ISF-002: Anchorは両端Uの14 CPのみ。
- ISF-003: 147 CPすべてContact input=true。
- ISF-004: Contact-only 133 CPはUnsupported、DirectSupportは14 CP。
- ISF-005: World Gravity=(0,0,0)の明示入力。
- ISF-006: Conformity=0を保持。非ゼロGravity/Conformityは未実装solverのため明示的に拒否する。
- ISF-007: DomainId=1とnormalized UV、Stable IDを各CPに保持。

BF-006 Output Semantics:
- OSF-001: 147 CPのFinal Positionを取得し解析的な期待位置と照合。
- OSF-002: 147 CPのSurfaceReferenceとStable IDを保持。
- OSF-003: 147 CPのSurface Normalを取得し期待(0,0,1)と照合。
- OSF-004: 147 CPのsigned SeparationをSurfaceQueryで取得。
- OSF-005: 147 CPのSupport状態を取得。
- OSF-006: Final Position deviation max=0m（許容差1e-5m）。
- OSF-007: Normal deviation max=0（許容差1e-5）。
- OSF-008: Separation absolute max=0m（許容差1e-5m）。
- OSF-009: SupportCount=14。
- OSF-010: `SeparationDistance <= CollisionTolerance`によるderived ContactCount=147。

Validation-only実装:
- `RunBf005006()` をC++ Gem BehaviorContextへ公開。
- `sansacloth_sr001_boundary_probe.py` がC++ Probeを呼び出す。
- `BuildProbeAssets.ps1` はPython Probe 4本とC++ Validation Gemを配置。
- `SurfaceReference` の解決はBF-003/004と同じ4頂点・2三角形の解析平面。O3DE実Mesh APIやReference JSON importerではない。
- Final PositionはC0/G0のidentity stage。非ゼロGravity/Conformityでは計算せず拒否する。**Reference Solverやproduction Runtime Backendの移植・数値一致の証明ではない。**
- 判定は `BF-005.RESULT`、`BF-006.RESULT`、`SR-001.RESULT` がすべてPASSかつ各ISF/OSF結果PASSの場合のみ行う。

状態: **BF-005 PASS / BF-006 PASS**（2026-10-08、O3DE 26.05 Editor実行ログで確認、validation-only SR-001 Flat / C=0 / G=0）。

実測証拠（2026-10-08 18:03 JST、O3DE Editor / SansaClothBackendProbeValidation Gem）:
- `SR-001.DISCOVERY|CPP_PROBE_AVAILABLE`
- `SR-001.CP_COUNT|147`、`SR-001.ANCHOR_COUNT|14`
- `SR-001.CONTACT_INPUT_COUNT|147`、`SR-001.DIRECT_SUPPORT_COUNT|14`
- `SR-001.CONTACT_ONLY_UNSUPPORTED_COUNT|133`
- `SR-001.OUTPUT_CP_COUNT|147`、`SR-001.DERIVED_CONTACT_COUNT|147`
- `SR-001.WORLD_GRAVITY_INPUT|0,0,0`、`SR-001.CONFORMITY_INPUT|0`、`SR-001.COLLISION_TOLERANCE_M|0`
- `SR-001.FINAL_POSITION_DEVIATION_MAX_M|0`、`SR-001.NORMAL_DEVIATION_MAX|0`、`SR-001.SEPARATION_ABS_MAX_M|0`
- `SR-001.NONZERO_GRAVITY_REJECTED|TRUE`、`SR-001.NONZERO_CONFORMITY_REJECTED|TRUE`
- `SR-001.CP_FIRST|id=0;domain=1;uv=0,0;position=-0.100000001,-0.0500000007,0;normal=0,0,1;separation_m=0;support=Anchor`
- `SR-001.CP_CENTER|id=73;domain=1;uv=0.5,0.5;position=0,0,0;normal=0,0,1;separation_m=0;support=Unsupported`
- `SR-001.CP_LAST|id=146;domain=1;uv=1,1;position=0.100000001,0.0500000007,0;normal=0,0,1;separation_m=0;support=Anchor`
- `BF-005.ISF-001.RESULT|PASS` ～ `BF-005.ISF-007.RESULT|PASS`
- `BF-006.OSF-001.RESULT|PASS` ～ `BF-006.OSF-010.RESULT|PASS`
- `BF-005.RESULT|PASS`、`BF-006.RESULT|PASS`、`SR-001.RESULT|PASS`
- `SR-001.PYTHON_BRIDGE_RESULT|PASS`、`SR-001.PYTHON_PROBE_RESULT|PASS`

判定範囲は解析平面・単一Domain・C0/G0の入力/出力意味論に限定する。非ゼロ入力は明示的拒否が確認されたのみであり、非ゼロGravity/ConformityのSimulation結果、実O3DE Mesh API、Reference JSON importer、production Solverを検証したものではない。

## 10. Phase OBF-05: BF-007 Basic Fixture Mapping

Unity PC BF-007の`SansaClothBasicFixtureProbe.cs`で定義されたSR-001～005解析Fixtureを、O3DE 26.05 EditorのC++ Validation Gemに再現する。既存Unity Probeと同じRaised Cosine式（幅0.10m）を使用する。

- 各Fixture: 21 U × 7 V = 147 vertices / 147 CP、20×6×2 = 240 triangles。
- Canonical `(x,y,z)` → O3DE `(x,z,y)`。Unityの`V00,V01,V11`に相当するTriangleはO3DEでは`V00,V11,V01`としてwindingを反転する。
- SR-003 Sideはcanonical Z -90°に相当するO3DE Y +90°で回転する。
- 5 Fixtureすべてでlocal height range、中央位置、中央analytic normal、最初のTriangle geometric normalを独立期待値と比較する。
- さらに全240 Triangleの非退化と外向き法線成分を検査する。

| Fixture | Kind | Local Height Min / Max (m) | O3DE Center Position (m) | O3DE Center/First Triangle Normal | Anchor |
|---|---|---|---|---|---|
| SR-001.FLAT | Flat | 0 / 0 | (0,0,0) | (0,0,1) | 14 |
| SR-002.CONVEX_UP | Convex 0.03 | 0 / 0.03 | (0,0,0.03) | (0,0,1) | 14 |
| SR-003.CONVEX_SIDE | Convex 0.03, Y +90° | 0 / 0.03 | (0.03,0,0) | (1,0,0) | 7 |
| SR-004.CONCAVE_SHALLOW | Concave 0.02 | -0.02 / 0 | (0,0,-0.02) | (0,0,1) | 14 |
| SR-005.CONCAVE_DEEP | Concave 0.05 | -0.05 / 0 | (0,0,-0.05) | (0,0,1) | 14 |

Validation Case:
- FMP-001 COUNTS (147 vertices, 147 CP, 240 triangles)
- FMP-002 ANCHORS (14 / sideのみ7)
- FMP-003 HEIGHT_RANGE (local min/max)
- FMP-004 CENTER_POSITION
- FMP-005 CENTER_NORMAL
- FMP-006 FIRST_TRIANGLE_NORMAL
- FMP-007 ALL_TRIANGLE_GEOMETRY (240 nondegenerate + outward)

実装:
- C++ `RunBf007()` をBehaviorContextの`azlmbr.sansacloth_probe`に公開。
- Python `sansacloth_basic_fixture_probe.py` から呼び出す。
- `BuildProbeAssets.ps1` は現行6本のPython ProbeとC++ Validation Gemを配置する。
- `BF-007.<Fixture>.RESULT|PASS` 5件と `BF-007.RESULT|PASS`、`BF-007.PYTHON_BRIDGE_RESULT|PASS`、`BF-007.PYTHON_PROBE_RESULT|PASS`を確認する。

**範囲:** 解析式から生成したvalidation-only mesh-like gridの数値写像。O3DE Mesh APIによる実メッシュ読み込み、Reference JSON importer、skinned/deformed mesh、production solverの動作を証明しない。

状態: **BF-007 PASS**（2026-10-08 20:30 JST、O3DE 26.05 Editor runtime、validation-only analytic fixture）。

実測証拠（ユーザー提供のEditorログ、`CPP_PROBE_AVAILABLE`）:
- 5 Fixtureとも `VERTEX_COUNT|147`、`TRIANGLE_COUNT|240`、`CP_COUNT|147`。
- `SR-001.FLAT`: Anchor 14、Local Height 0/0m、Center (0,0,0)、Center/First Triangle Normal (0,0,1)。
- `SR-002.CONVEX_UP`: Anchor 14、Local Height 0/0.0299999993m、Center (0,0,0.0299999993)、Normal (0,0,1)。
- `SR-003.CONVEX_SIDE`: Anchor 7、Local Height 0/0.0299999993m、Center (0.0299999993,0,2.6822089e-09)、Center Normal (1,0,8.94069672e-08)、First Triangle Normal (1,-0,8.94069458e-08)。
- `SR-004.CONCAVE_SHALLOW`: Anchor 14、Local Height -0.0199999996/0m、Center (0,0,-0.0199999996)、Normal (0,0,1)。
- `SR-005.CONCAVE_DEEP`: Anchor 14、Local Height -0.0500000007/0m、Center (0,0,-0.0500000007)、Normal (0,0,1)。
- 5 Fixture × 7 Case（`COUNTS`、`ANCHORS`、`HEIGHT_RANGE`、`CENTER_POSITION`、`CENTER_NORMAL`、`FIRST_TRIANGLE_NORMAL`、`ALL_TRIANGLE_GEOMETRY`）はすべて `RESULT|PASS`。
- `BF-007.SR-001.FLAT.RESULT|PASS`、`BF-007.SR-002.CONVEX_UP.RESULT|PASS`、`BF-007.SR-003.CONVEX_SIDE.RESULT|PASS`、`BF-007.SR-004.CONCAVE_SHALLOW.RESULT|PASS`、`BF-007.SR-005.CONCAVE_DEEP.RESULT|PASS`。
- `BF-007.RESULT|PASS`、`BF-007.PYTHON_BRIDGE_RESULT|PASS`、`BF-007.PYTHON_PROBE_RESULT|PASS`。

前回の `BF-007.DISCOVERY|CPP_GEM_NOT_LOADED` は今回 `CPP_PROBE_AVAILABLE` に変化し解消を確認。原因（Gem有効化・ビルド・別PC等）はログだけでは断定しない。

このPASSは上記validation-only解析Fixtureに限定する。実O3DE Mesh API、Reference JSON importer、skinned/deformed mesh、production solverの動作保証ではない。

## 11. Phase OBF-06: BF-008 Validation Capture

### 11.1 検証範囲

Unity PC BF-008のJSON Lines validation artifactと同じ測定意味を、O3DE PC Validation GemのSR-001 Flat / C=0 / G=0から外部取得する。**Production serializationではない。**

C++ `RunBf008Capture()` がBF-005/006と同じ `MapSr001Output` で147 CPを評価し、`final_cp` 147レコードと`aggregate` 1レコードを生成する。Python Editor scriptがUUID EventIdとUTC Timestampを付け、JSONLをOSの一時フォルダに保存し、**ファイルを再読込して検証する**。PythonはCP測定結果を再計算して生成しない。

- TestId: `O3DE-SR-001-C0-G0`
- Backend: `O3DE PC`
- CP: 21 U × 7 V = 147、DomainId=1、StableControlPointId=0..146
- Coordinate: O3DE `(x,z,y)` からcanonical `(x,y,z)` へ戻して出力する。Flat Normalはcanonical `(0,1,0)`。
- Contact: `SeparationDistance <= CollisionTolerance(0m)`。ContactCount=147。
- Support: Anchor=14、Unsupported=133。SupportCount=14。
- Final Position/SurfaceReference/Normal/Separation/Support/derived ContactをCPごとに記録する。
- Aggregate: ContactCount、SupportCount、MeanSeparation、MaxSeparation、MaxPenetration、MaxPositionDeviation、RMSPositionDeviation。
- Stage Measurementは対象外。

### 11.2 JSONLレコード構成

| Record Type | Count | 内容 |
|---|---:|---|
| `header` | 1 | event_id、timestamp_utc、test_id、backend、cp_count |
| `final_cp` | 147 | stable_id、position_m、surface_reference、surface_normal、separation_m、support、derived_contact |
| `aggregate` | 1 | contact_count、support_count、mean_separation_m、max_separation_m、max_penetration_m、max_position_deviation_m、rms_position_deviation_m |
| 合計 | **149** | JSON Lines UTF-8 |

### 11.3 Validation Cases

- VCF-001: Header EventId/TimestampUtc/TestId/Backend/CPCount。
- VCF-002: Final CP 147 records。
- VCF-003: Stable CP IDが0..146で一意・連続。
- VCF-004: 全CPのDomainId/U/V、Final Position、Normal、Separationがcanonical期待値と一致（許容差1e-5）。
- VCF-005: 全CPのSupport/derived Contactが期待値と一致。
- VCF-006: AggregateのContactCount=147、SupportCount=14、Separation/Deviation統計が全CP測定値から再集計した値と一致（許容差1e-5）。
- VCF-007: 実際に書き出したJSONLを再読込して上記検証にPASS。

PASSにはC++ `BF-008.CPP_MEASUREMENTS_RESULT|PASS`、Python Bridge PASS、VCF-001～007 PASS、`BF-008.RESULT|PASS`、`BF-008.PYTHON_PROBE_RESULT|PASS`、出力ファイル149行を要求する。Gem未ロード/Method未公開はOPEN、測定・書き出し・検証失敗はFAILとする。

実装:
- `engines/o3de/pc/probe/Gem/SansaClothBackendProbeValidation/Source/SansaClothBackendProbeValidationSystemComponent.cpp`
- `engines/o3de/pc/probe/Editor/Scripts/SansaCloth/sansacloth_validation_capture_probe.py`
- `BuildProbeAssets.ps1` は6本のPython ProbeとC++ Validation Gemを配置する。

**範囲:** BF-005/006と同じ解析平面・C0/G0 identity stage。実Mesh API、Solver非ゼロ入力、Reference JSON importer、Production serializationは未検証。

状態: **BF-008 RUNTIME PASS**（2026-10-09 JST / 2026-10-08 15:00:21 UTC、O3DE 26.05 Editor、validation-only）。

ユーザー提供のEditor実測ログ:
- `BF-008.DISCOVERY|CPP_CAPTURE_AVAILABLE`。
- `BF-008.CPP_CP_COUNT|147`、`BF-008.CPP_CONTACT_COUNT|147`、`BF-008.CPP_SUPPORT_COUNT|14`。
- `BF-008.CPP_MEASUREMENTS_RESULT|PASS`、`BF-008.PYTHON_BRIDGE_RESULT|PASS`。
- `BF-008.VCF-001.RESULT|PASS` ～ `BF-008.VCF-007.RESULT|PASS`（7/7）。
- EventId: `0646dfd1-5a68-4cda-8119-d83607a92689`。
- TimestampUtc: `2026-10-08T15:00:21.264564Z`。
- TestId: `O3DE-SR-001-C0-G0`、Backend: `O3DE PC`。
- CapturePath（実行端末の一時ディレクトリ）: `C:\Users\masay\AppData\Local\Temp\SansaClothO3DEValidationCapture-0646dfd1-5a68-4cda-8119-d83607a92689.jsonl`。
- `BF-008.RECORD_COUNT|149`、`BF-008.CP_COUNT|147`、`BF-008.CONTACT_COUNT|147`、`BF-008.SUPPORT_COUNT|14`。
- `MEAN_SEPARATION_M`、`MAX_SEPARATION_M`、`MAX_PENETRATION_M`、`MAX_POSITION_DEVIATION_M`、`RMS_POSITION_DEVIATION_M` はすべて0。
- `BF-008.RESULT|PASS`、`BF-008.PYTHON_PROBE_RESULT|PASS`。

**証拠範囲:** Pythonスクリプトが実際のJSONLファイルを再読込し、149レコードおよびVCF-001～007を検証したことはEditorログから確認した。加えて、2026-10-09 JSTにユーザーが会話へJSONLファイル本体を提出し、独立したJSONパース・全147 CPの数値比較・Aggregate再集計を実施してPASSした。

添付ファイルの独立検査:
- Filename: `SansaClothO3DEValidationCapture-0646dfd1-5a68-4cda-8119-d83607a92689.jsonl`
- Size: **34853 bytes**
- SHA-256: `70ecf22f4967dbf5ddc889de63194525c69fd09832fd36a8e0e28b4e42a2cf92`
- JSONL: Header 1 / Final CP 147 / Aggregate 1 = **149 records**。全行JSONとしてパース成功。
- Header EventId、TimestampUtc、TestId、Backend、CPCountがEditorログと一致。
- Stable ID 0..146、DomainId=1、全CPのUV、canonical Position、Normal、Support、Contact、Separationを独立検査。
- 最大Position期待値差: `7.700000012600405e-09 m`、最大UV差: `2.399999998736746e-08`、最大Normal差: `0`。許容差 `1e-5` 内。
- Anchor=14、Contact=147、Aggregate統計は再集計値と一致。
- **独立検査結果: PASS（検査エラー0件）**。

JSONLファイルは会話に添付されており、GitHubリポジトリへコピーしていない。Production Backendの実Mesh API、Solver、Production serializationは対象外。

## O3DE PC Runtime Backend Feasibility Checkpoint

2026-10-09 JST時点のO3DE 26.05 Editor実測結果:

| Gate | 状態 | 実測範囲 |
|---|---|---|
| BF-001 Unit Mapping | PASS | canonical m ↔ O3DE unit |
| BF-002 Coordinate Mapping | PASS | axes/rotation/winding |
| BF-003 SurfaceReference Mapping | PASS | validation-only single-domain flat |
| BF-004 SurfaceQuery Feasibility | PASS | static rigid flat |
| BF-005 Input Semantics | PASS | SR-001 C0/G0 |
| BF-006 Output Semantics | PASS | SR-001 final CP |
| BF-007 Basic Fixture Mapping | PASS | SR-001～005 analytic grids |
| BF-008 Validation Capture | PASS | JSONL 149 records, read-back VCF-001～007 |

判定: **O3DE PC Runtime Backend Feasibility Checkpoint PASS**（BF-001～008にBLOCKED/OPENなし）。

この判定は既存validation-only fixtureによるBackend境界の実現可能性に限定する。Production Backend完成、Reference Solverとの数値一致、実Mesh API、skinned/deformed mesh stable mapping、性能保証を意味しない。後続はFixture Exchange importerおよびSurfaceResponse比較の検証へ進む。

## 12. Phase OBF-07: Fixture Exchange SR-001 JSON Import

### 12.1 目的と入力

Rust Referenceが生成・commit済みの`reference/validation/fixture-exchange/basic-v1/SR-001-C0-G0.json`をO3DE Editorで読み込む。BF-007の解析式を再利用してFixtureを作り直さず、**JSON resolved dataのみ**からO3DE用のBody頂点/法線/triangle、CP/strip/SurfaceReference/Anchor/Contact、case inputsを構成する。

初期検証は`SR-001-C0-G0` 1ケースだけを対象とする。30-case importer、Reference Resultとの比較、O3DE実Mesh API、Solverへの接続は後続Gateで別途検証する。

- Format: `sansacloth.validation.fixture-exchange/0`、canonical length m、gravity m/s²、X=right/Y=up/Z=forward。
- Body: DomainId=1、147 vertices、240 triangles。頂点position/normal/UVはJSON値を読み込む。
- Cloth: 147 CP、StableId 0..146、7 strips×21、Anchor 14、Contact input 147。ContactはSupportに昇格させない。
- Inputs: World Gravity=(0,0,0)、Conformity=0、CollisionTolerance=0。
- O3DE座標: canonical `(x,y,z)` → O3DE `(x,z,y)`、Triangle `[a,b,c]` → `[a,c,b]`。DomainId/UV/StableIdは変えない。
- Importerは未知field/format、nonfinite、vector長、UV範囲、重複ID/strip-order、無効triangle index/normal、未知domain、範囲外Conformity、負CollisionToleranceを拒否する。

### 12.2 実装前に固定する検証ケース

| ID | Gate | PASS条件 |
|---|---|---|
| OXF-001 | JSON format/coordinate | Rust baselineのformatとcanonical unit/axesが正しい |
| OXF-002 | Body mesh import | 147 vertices/240 triangles、Width 0.20m、Depth 0.10m、normal +Zに写像 |
| OXF-003 | Winding conversion | 全240 triangleが非退化、O3DE outward +Z |
| OXF-004 | SurfaceReference | DomainId=1、全CPのUVを保持、runtime triangle IDへ置換しない |
| OXF-005 | CP identity/strip | 0..146 stable ID、7×21 strip/order、147 CP |
| OXF-006 | Anchor/Contact | Anchor=14、Contact input=147、Contact-only unsupported=133 |
| OXF-007 | Case inputs | G=(0,0,0)、C=0、Tolerance=0を保持 |
| OXF-008 | Negative import | 不正format/domain/UV/index/id/strip-order/normal/scalar/vector/unknown field/nonfiniteをすべて拒否 |
| OXF-009 | Resolved geometry | 全CPのimported PositionとBody同一UVのPosition一致（1e-6m）、中央位置と法線一致 |
| OXF-010 | Deploy/read source | Git管理されたReference baselineを検証Projectに配布し、Editor上で実ファイルを読み込む |

実行時のGate `OXF-001`～`OXF-010`は**RUNTIME PASS（2026-10-09、O3DE Editor、validation-only Python importer）**。各CaseがPASS、`OXF-010.RESULT|PASS`、`OXF.RESULT|PASS`、`OXF.PYTHON_PROBE_RESULT|PASS`を提示された実行ログで確認した。

2026-10-09 OXF runtime evidence（提示されたO3DE Editorログ）:
- Project: `SansaClothBackendProbe`（Windows 11 Pro）。ログ上のEditor起動時刻は2026-10-09 00:48:09 JST。OXF各行の個別時刻は記録されていない。
- `OXF-001.RESULT|PASS` ～ `OXF-010.RESULT|PASS`（OXF-008を含む全10 Gate）。
- `OXF.BODY_VERTEX_COUNT|147`、`OXF.BODY_TRIANGLE_COUNT|240`、`OXF.CP_COUNT|147`。
- `OXF.ANCHOR_COUNT|14`、`OXF.CONTACT_INPUT_COUNT|147`、`OXF.CONTACT_ONLY_UNSUPPORTED_COUNT|133`。
- `OXF-008.REJECTED_CASE_COUNT|14`、`OXF-008.RESULT|PASS`。FORMAT、DOMAIN、UV、TRIANGLE_INDEX、DUPLICATE_STABLE_ID、DUPLICATE_STRIP_ORDER、ZERO_NORMAL、CONFORMITY、NEGATIVE_TOLERANCE、VECTOR_LENGTH、UNKNOWN_FIELD、NONFINITE、DUPLICATE_JSON_KEY、NONFINITE_CONSTANTがすべて`REJECTED`。
- `OXF-010.SOURCE_PATH` はProject内の `Editor/Scripts/SansaCloth/Fixtures/SR-001-C0-G0.json` を示す。
- `OXF-010.SHA256|d63de3a9851bf72bf05c41e61f86955178669e198b7c569dc2b54bf70c2d1833`。
- `OXF.CASE_ID|SR-001-C0-G0`、`OXF.RESULT|PASS`、`OXF.PYTHON_PROBE_RESULT|PASS`。
- 起動ログには `DiffuseProbeGridUpdatePassTemplate` 不在のPassFactoryエラーがあるが、OXFのGateは全件PASS。描画機能への影響は未評価。
- 根拠は提示されたEditorログ。JSONファイル自体の独立した再ハッシュ検証および実行環境での再実行は本記録では行っていない。

判定はRust Referenceの単一resolved JSONをPython importerで読み込むvalidation-only範囲に限る。30-case matrix、C++ Backendへのgeometry投入、実O3DE Mesh API、production solver、およびReference SurfaceResponse数値一致は未検証。

このPhaseはPython Editor Validation-only importerであり、C++ Gem側へimported geometryを渡すことやproduction mesh生成を意味しない。後続Gateで実データのC++境界投入を別途要求する。

## 12A. Phase OBF-08: Fixture Exchange 30-Case Matrix Import（実装前テスト仕様）

### 12A.1 対象・境界

- Gate: OXF-011～OXF-019。**2026-10-09 Editor Runtime PASS**（検証範囲は本節のvalidation-only importer）。
- 入力: `reference/validation/fixture-exchange/basic-v1/` にcommit済みのRust Reference JSON 30件（SR-001～005 × G0/G1 × C0/C0.5/C1）。解析式によるO3DE側Fixture再生成は禁止。
- O3DE Editor Python Bindingsを使用し、Git管理されたJSONを検証Projectへ配布する。既存のOXF-001～010のSR-001-C0-G0単体検証は維持する。
- JSON形式は `sansacloth.validation.fixture-exchange/0`。canonical (x,y,z) → O3DE (x,z,y)、triangle [a,b,c] → [a,c,b]、長さm。StableId、Strip、DomainId、UV、Anchor、Contactの意味論を保持する。
- 本Gateはvalidation-only importer検証であり、C++ Gem境界への実データ投入、実Mesh API、production solver、SurfaceResponse数値一致は対象外。これらは後続Gateとする。

### 12A.2 実装前テストケース

| ID | Gate | PASS条件 |
|---|---|---|
| OXF-011 | Matrix deploy/read | 30件のGit管理baselineをEditorから実ファイルとして読み込み、30件すべてparse/import成功。欠落・重複ファイルはFAIL |
| OXF-012 | Case identity | 30件のcase_idがファイル名と一致し一意。SR-001～005それぞれG0/G1×C0/C0.5/C1の6件が過不足なく存在 |
| OXF-013 | Geometry integrity | 各caseでbody頂点・triangle・normal・UV・cloth CP・stripを妥当性検証。index範囲、非退化triangle、winding変換、StableId重複禁止を確認。SR-001専用の147/240を全Scenarioに強制しない |
| OXF-014 | Scenario invariance | 同一SRの6ケース間でbody/cloth resolved geometry、SurfaceReference、Anchor/Contact、StableId/Stripを比較し不変。Gravity/Conformityのみcase差分として許容（CollisionToleranceはbaselineに従い検証） |
| OXF-015 | SurfaceReference/identity | 全CPのDomainId・normalized UV・StableId・StripId/Orderを保持し、runtime triangle IDに置換しない。canonical→O3DE変換後も参照identityが不変 |
| OXF-016 | Anchor/Contact | 全caseのAnchor/ContactをJSON入力どおり保持。Contact入力を自動的にSupportへ昇格しない。SR-001はAnchor=14/Contact=147、SR-003はAnchor=7を追加確認 |
| OXF-017 | Input matrix | 各case_idのG0/G1とC0/C0.5/C1に対応するWorld Gravity・Conformityを検証し、O3DE変換後のGravityとCollisionToleranceを保持。期待する具体値はReference baselineの仕様・実値に基づき固定し、case名から数値を推測しない |
| OXF-018 | SR-003 baked geometry | SR-003の6件でReference JSONに焼き込まれたConvex-Side形状・法線・座標変換・winding・Anchor=7を検証。O3DE側でrigid transformを二重適用しない |
| OXF-019 | Aggregate verdict | OXF-011～018が全PASS、case 30/30・scenario 5/5・case別結果30件を出力。いずれか失敗で総合FAIL、未配布等の未実行状態をPASS扱いしない |

### 12A.3 実装設計・確認事項

1. 既存単一ケースimporterのschema validation、厳密JSON parser、座標/winding変換を共通利用できる形へ分離する。既存OXF-001～010の挙動と14件のnegative testを退行させない。
2. `SR-001-C0-G0` 固定のcase_id、147/240、平面形状、zero Gravity/Conformityのassertionを汎用import処理から切り離す。Scenario固有assertionは別レイヤーとする。
3. Rust baseline 30ファイルをデプロイする際は元ファイルのSHA-256を検証し、Editor側で読んだファイルのdigestを記録する。コピー先の同名旧ファイル残存による誤検証を防ぐ。
4. 30件の一覧・case別検証結果・aggregate結果を機械判読可能な `SANSA_O3DE|` プレフィックスで出力する。FAIL時はcase_idと失敗したGateを記録する。
5. 検証条件を先に固定し、実装後は静的テストと実O3DE Editor実行ログの両方を確認する。ログ未提示のGateはOPENのままとする。

### 12A.4 Runtime evidenceの受け入れ条件

- `OXF-011.RESULT|PASS` ～ `OXF-019.RESULT|PASS` の全9件を実Editorログで確認。
- Case count 30、Scenario count 5、各caseのPASS 30件、およびaggregate PASSを確認。
- OXF-001～010を再実行し退行がないことを確認。
- 失敗時は検証ID・case_id・理由を記録し、PASS判定を保留する。

### 12A.4a Editor runtime evidence（2026-10-09）

提示されたO3DE Editorログ（Project: `SansaClothBackendProbe`、起動ログ時刻 2026-10-09 06:50:19 JST）で次を確認した。

- `OXF-011.RESULT|PASS` ～ `OXF-019.RESULT|PASS`：全9 Gate PASS。
- `OXF.MATRIX_CASE|<case_id>|<sha256>|PASS`：SR-001～005、各6ケース、計30/30 PASS。Case IDは C0/C05/C1 × G0/G1。
- `OXF-019.CASE_COUNT|30`、`OXF-019.SCENARIO_COUNT|5`、`OXF-019.RESULT|PASS`、`OXF.MATRIX_RESULT|PASS`。
- 既存単一ケースの `OXF-001`～`OXF-010` も同一実行ログ内でPASS。異常系14/14拒否、`OXF.PYTHON_PROBE_RESULT|PASS`。
- `OXF-010.SHA256|d63de3a9851bf72bf05c41e61f86955178669e198b7c569dc2b54bf70c2d1833`。
- Editor起動時に `DiffuseProbeGridUpdatePassTemplate` 不在のPassFactoryエラーあり。OXF結果への影響は観測されていないが描画系の評価は別途必要。

判定：**OXF-011～019 RUNTIME PASS（validation-only Python importer）**。根拠は提示されたEditorログであり、30 JSON本体の独立再ハッシュやC++へのgeometry受け渡しは未実施。

### 12A.5 後続Gate

- C++ Validation Gemへのimported geometry実データ受け渡し。
- Reference Result 30ケースとのGravity/Conformity/Collision/Final SurfaceResponse比較。
- 実O3DE Mesh API・deformed mesh lifecycleの検証。これらは本PhaseのPASS条件に含めない。

## 12B. Phase OBF-09: Python → C++ Fixture Exchange Data Handoff（実装前テスト仕様）

### 12B.1 既存境界と目的

2026-10-09時点のValidation Gem `SansaClothBackendProbeValidationSystemComponent` は `BehaviorContext` で `azlmbr.sansacloth_probe` に `RunObf006`、`RunBf003`、`RunBf004`、`RunBf005006`、`RunBf007`、`RunBf008Capture` を公開する。既存のBF-003/004/005/006/007/008は主にC++内で作成したvalidation fixtureを使用しており、Python importerのresolved JSONデータをC++に渡す経路は未実装。

本Phaseは**Python importerで厳密検証した実データをC++ Validation Gemへ渡し、受け渡し前後の意味論を照合する**。C++側で解析式を使って同等形状を再生成することは禁止。既存BF/OXF probeは維持する。

### 12B.2 Handoff契約（案）

- 公開境界はO3DE `BehaviorContext` のValidation-only APIとする。実際の引数表現はO3DE Python Bindingsで動作検証してから確定する。候補は、version付きの単一JSON文字列（Pythonで検証済みのresolved dataを渡し、C++でも独立parse/validate）またはO3DE BehaviorContextが確実に変換できる明示的な配列・scalar群。Pythonオブジェクトの生ポインタや所有権不明な参照を渡さない。
- 値の基準はcanonical (X右/Y上/Z前)、m、m/s²。変換は**一箇所のみ**で行い、O3DE (X右/Y前/Z上)へのY/Z交換に伴うtriangle winding反転を二重適用しない。
- 必須意味論：case_id、body domain_id、position/normal/UV/triangles、cloth stable_id/strip_id/strip_order/position/SurfaceReference/anchor/contact、World Gravity/Conformity/CollisionTolerance。
- StableId、StripId/Order、DomainId/UVは数値を変更しない。Contact入力はSupport結果ではない。SR-003はReferenceでbaked済みのConvex-Side geometryを二重変換しない。
- C++側は受信データを検証処理中に自己所有の値として保持し、Python呼び出し終了後に借用参照を残さない。返却する比較結果は構造化された検証情報とし、C++がPASSを返すだけで検証内容を隠さない。
- バージョン違い、欠落、余剰、不正数値、index範囲外、重複StableId/Strip、未知DomainIdを拒否する。境界を通した後のC++結果はPython側の入力と照合する。

### 12B.3 実装前テストケース

| ID | Gate | PASS条件 |
|---|---|---|
| OXC-001 | API discovery | Editor PythonからValidation Gemの新規Handoff APIを発見し呼び出せる。未ロード時はOPENでありPASSとしない |
| OXC-002 | Real payload | Git管理の`SR-001-C0-G0.json`からimportした実データを渡し、C++内部の解析Fixture再生成を使わない |
| OXC-003 | Body mapping | bodyの全頂点・法線・UV・triangleを受信し、147頂点/240 triangleとwinding/座標変換を検証 |
| OXC-004 | CP identity | 147 CPのStableId、7×21 strip/order、SurfaceReference DomainId/UVを完全照合 |
| OXC-005 | Anchor/Contact | Anchor 14、Contact input 147、Contact-only unsupported 133を保持し、ContactをSupportへ昇格させない |
| OXC-006 | Inputs | G=(0,0,0)、C=0、CollisionTolerance=0を受信し、単位・座標変換を照合 |
| OXC-007 | Ownership/lifetime | 受信データをC++所有値へコピーし、Python側の元データ破棄・変更後も参照不正がないことを確認 |
| OXC-008 | Negative boundary | 欠落/余剰field、version不一致、非有限値、重複ID、無効triangle/UV/domain等をC++境界で拒否し、Python importerだけの検証に依存しない |
| OXC-009 | 30-case extension | SR-001～005×6の全30ケースをC++へ渡し、case ID・geometry/CP/inputs・Scenario内不変性を照合。SR-003 baked geometry/Anchor=7を含む |
| OXC-010 | Aggregate/evidence | 各Gateと30ケースのPASS/FAIL、件数、source SHA-256、API/format versionをEditorログに記録。失敗があれば総合FAIL |

### 12B.4 実装順序と判定

1. O3DE側で確実に呼び出せるBehaviorContext引数型を最小のspikeで確認する（validation-only試作）。
2. 受け渡し表現と座標変換責任を固定する。C++側の独立validationと所有権を実装する。
3. SR-001-C0-G0単一ケース（OXC-001～008）をEditor runtimeで確認する。
4. 30ケース（OXC-009～010）へ拡張し、既存BF-001～008、OXF-001～019の退行を確認する。
5. 実Editorログの確認までは**OXC-001～010 OPEN**。本Phase PASSでもproduction Mesh API、deformed mesh lifecycle、SurfaceResponse solver/Reference数値一致は未検証とする。

## 13. Probe配布方針

Unity検証と同様、O3DE検証Project自体をSansaCloth repositoryへ含めることは要求しない。

Canonical source:
- engines/o3de/pc/probe/

Deploy target:
- <O3DE project>/Editor/Scripts/SansaCloth/

初期ProbeはEditor Python Bindingsを利用する。検証ProjectではPython Editor Bindings Gemを有効化する。

Probe sourceとdeploy scriptはSansaCloth repositoryをsource of truthとし、検証Project側は再生成可能な作業環境として扱う。

## 14. 後続順序

1. OBF-001～004 Python Probe runtime確認。
2. 必要ならC++ Probeを追加しOBF-004/005を確認。
3. OBF-006 World Gravityを確認。
4. O3DE BF-001/BF-002を判定。
5. BF-003 SurfaceReference Mappingをvalidation fixtureで確認する。
6. BF-004 SurfaceQueryをrigid flat fixtureで検証する。
7. BF-005/006 SR-001-C0-G0 Input/Output Semanticsをvalidation fixtureで検証する。
8. BF-007 Basic Fixture Mapping。
9. BF-008 Validation Capture。
10. Fixture Exchange 30-case import/SurfaceResponse比較へ進む。
11. Unity/O3DE双方で成立した境界だけをBackend-neutral contract候補として整理する。

## 15. 未確定事項

- Canonical handednessの全体仕様確定（O3DE math runtimeのCross orientationおよび変換時winding反転は確認済み）。
- O3DE実メッシュAPIへtriangle winding/geometric normalを適用する統合経路（math probeでのwinding反転は確認済み）。
- 外部World GravityのBackend入力経路（実Runtime Backend統合時に確認）。
- stable SurfaceReference mapping。
- runtime/deformed mesh access。
- Validation Captureのproduction出力経路（BF-008では一時JSONL出力を検証対象とする）。

> SansaCloth > docs > ja-JP > draft > verification > O3DEPC.RuntimeBackend
