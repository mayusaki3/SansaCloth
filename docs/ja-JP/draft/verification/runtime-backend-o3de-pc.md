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

## 10. Probe配布方針

Unity検証と同様、O3DE検証Project自体をSansaCloth repositoryへ含めることは要求しない。

Canonical source:
- engines/o3de/pc/probe/

Deploy target:
- <O3DE project>/Editor/Scripts/SansaCloth/

初期ProbeはEditor Python Bindingsを利用する。検証ProjectではPython Editor Bindings Gemを有効化する。

Probe sourceとdeploy scriptはSansaCloth repositoryをsource of truthとし、検証Project側は再生成可能な作業環境として扱う。

## 11. 後続順序

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

## 12. 未確定事項

- Canonical handednessの全体仕様確定（O3DE math runtimeのCross orientationおよび変換時winding反転は確認済み）。
- O3DE実メッシュAPIへtriangle winding/geometric normalを適用する統合経路（math probeでのwinding反転は確認済み）。
- 外部World GravityのBackend入力経路（実Runtime Backend統合時に確認）。
- stable SurfaceReference mapping。
- runtime/deformed mesh access。
- Validation Captureの最終出力経路。

> SansaCloth > docs > ja-JP > draft > verification > O3DEPC.RuntimeBackend
