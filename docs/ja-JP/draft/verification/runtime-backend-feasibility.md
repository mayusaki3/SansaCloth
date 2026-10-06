# Runtime Backend Feasibility Checkpoint / Runtime Backend 実現可能性チェックポイント

> Draft verification plan for SansaCloth Runtime Backends.

## 1. 目的

Reference Surface Solverで確認したSurfaceResponse semanticsを、実Runtime Backendへ無理なく写像できるかを、本格的なBackend実装へ進む前に確認する。

本CheckpointはBackendの性能比較、最終Backend選定、製品品質判定ではない。

## 2. 前提

- Reference SolverはCore仕様そのものではない。
- Runtime BackendはReferenceと同一アルゴリズムを使用する必要はない。
- Backend固有近似を許容するが、Core/SurfaceResponseの意味論を変更してはならない。
- 内部Canonical Lengthはm。
- GravityはWorld Space vectorとして外部から与え、Body orientationから暗黙生成しない。
- SurfaceReferenceの論理的意味はRuntime triangle IDや一時的なmesh indexに依存させない。
- Simulation LODは本Checkpointでは適用しない。まずFull相当の単一品質で境界を確認する。

## 3. Probe Target

最初のProbe Targetは **Unity PC** とする。

理由:
- repositoryに `engines/unity/pc` の受け皿が既に存在する。
- PCを先に使うことでMobile/Quest固有制約を分離できる。
- Unityを最終優先Backendと決定するものではない。

Unity PCで境界が成立した後、同じCheckpointをO3DE PCへ適用する。GodotはBackend受け皿とTarget方針を定義してから対象化する。

## 4. Checkpoint Boundary

Backend側に最低限必要な論理境界:

```text
Engine Body/Cloth Data
        |
        v
Backend Mapping
  - coordinate/unit conversion
  - SurfaceReference mapping
  - Anchor/Support input
  - World Gravity input
        |
        v
Backend Surface Query / Surface Response
        |
        v
Common-semantic Result
  - Position
  - SurfaceReference
  - Surface Normal
  - Separation
  - Support
        |
        v
Engine Cloth Output
```

ReferenceのStage内部構造をBackend APIとして公開することは要求しない。

## 5. Feasibility Gates

### BF-001 Unit Mapping / 単位写像

- Engine側LengthをCanonical mへ一意に変換できる。
- round-tripで意味を失わない。
- UI表示単位はBackend境界へ混入させない。

### BF-002 Coordinate Mapping / 座標写像

- Position、Direction、Normal、Gravityを明示的に変換できる。
- Body rotationとWorld Gravityを独立に扱える。
- handedness差がある場合はBackend Mappingで吸収し、Core semanticsを変更しない。

### BF-003 SurfaceReference Mapping / 表面参照写像

- SurfaceReferenceをEngineの一時的Triangle IDだけに依存せず保持できる。
- Body Transform後も同じ論理表面を参照できる。
- Runtime mesh更新時のstable mapping方針を定義可能である。

### BF-004 SurfaceQuery Feasibility / 表面問い合わせ実現性

最低限、指定SurfaceReferenceとCurrent Positionから以下を得られる見込みがあること:
- Surface Position
- Surface Normal
- signed Separation

完全なSurfaceFrame tangent U/Vは本Checkpointの必須条件にしない。Basic Validation v1のSurfaceOffset NOT MEASUREDと整合させる。

### BF-005 Input Semantics / 入力意味論

- AnchorとContactを同一視しない。
- Contactした全CPをpinしない。
- World Gravityを明示入力できる。
- Conformity等のCore propertyをBackend固有名称へ変換しても意味を保持できる。

### BF-006 Output Semantics / 出力意味論

最低限、検証用に以下を共通意味へ戻せること:
- Final Position
- SurfaceReference
- Surface Normal
- SeparationDistance
- Support

Backend固有の内部データ形式を共通形式そのものにする必要はない。

### BF-007 Basic Fixture Mapping / 基本Fixture写像

SR-001～005の以下をProbe Targetへ配置可能であること:
- Flat
- Convex-Up
- Convex-Side
- Concave-Shallow
- Concave-Deep
- CF-FLAT-001 21 x 7 CP
- AF-BOTH-EDGES-001
- AF-EDGE-001

30 Basic RunsをBackend上で直ちに数値一致させることはCheckpoint通過条件ではない。まず同じ入力意味論を構成・観測できることを確認する。

### BF-008 Validation Capture / 検証取得

Backend実行結果から、少なくともBasic ValidationのFinal CP MeasurementとAggregate Measurementを取得できる経路を持てること。

Reference専用Stage snapshotの再現は必須ではない。

## 6. 判定

各Gateを以下で記録する:
- PASS: 実装経路を確認できた。
- BLOCKED: Backend/API制約により現行semanticsを維持できない。
- OPEN: 設計または実機確認が不足している。
- N/A: Targetに適用しない。

Checkpoint通過条件:
- BF-001～008にBLOCKEDがない。
- OPENが残る場合、後続実装で解消する所有箇所と検証方法が明確である。
- Core semantics変更が必要になった場合はBackend側で迂回せず、Core/Reference設計へフィードバックする。

## 7. Checkpointで実装しないもの

- Simulation LOD
- Performance threshold
- Mobile/Quest最適化
- Bake
- Shader最適化
- Dynamic Cloth v2
- Production serialization
- Full SurfaceOffset
- Backend間性能比較

## 8. Probe順序

1. Unity PC: BF-001～008の境界確認
2. Reference Basicとの最小fixture交換形式を決定
3. Unity PCでSR-001 Flatの単一Caseをend-to-end probe
4. 問題がなければSR-002～005へ拡張
5. O3DE PCへ同じGateを適用
6. 共通化できる部分だけをBackend-neutral contractへ昇格

Unity固有設計を先に共通抽象化しない。2つ目のBackendで共通性が確認されてから抽象化する。

## 9. Unity PC BF-001 / BF-002 Probe Definition

### 9.1 Unity側確認事項

Unity公式仕様で以下を確認した。

- Unityはleft-handed coordinate systemを使用する。
- Unity world axisは +X=right、+Y=up、+Z=forward。
- Unity Physicsはdefaultでworld spaceの1 unit = 1 meterを前提とする。
- World SpaceとSelf/Local Spaceは明示的に区別できる。

Reference:
- Unity Manual, Rotation and orientation: https://docs.unity3d.com/ja/current/Manual/QuaternionAndEulerRotationsInUnity.html
- Unity Manual, Transform: https://docs.unity3d.com/ja/current/Manual/class-Transform.html
- Unity Scripting API, Space.World: https://docs.unity3d.com/cn/6000.0/ScriptReference/Space.World.html

### 9.2 BF-001 Unit Mapping

Unity PC Probe Contract:
- 1 Unity world unit = 1 canonical meterとして扱う。
- Backend boundaryでPosition/Lengthに追加scaleを掛けない。
- imported assetのScale Factor / Convert Units / Transform scaleは入力データ準備側の責務とし、SansaCloth canonical unitを変更しない。
- non-unit Transform scaleを許容する場合のSurface Query挙動は後続Probeで別途確認する。

Probe:
- UBF-001: Unity (1,0,0) -> canonical (1m,0,0)
- UBF-002: canonical 0.20m cloth width -> Unity 0.20 world unit
- UBF-003: round-trip Position/Lengthが許容誤差内で一致
- UBF-004: Gravity magnitudeをlength scale変換の対象にしない。Unity world gravityをcanonical m/s^2として明示取得する。Reference Basicの既定値 -9.80665 m/s^2 とUnity ProjectのPhysics.gravity既定値そのものの一致は要求せず、Backend入力として明示的に受け渡せることを確認する。

BF-001 status: **PASS (runtime confirmed, 2026-10-01)**。

Unity実測:
- Unity +X = (1,0,0)
- cloth width 0.20m = 0.2 Unity world unit
- Unity Physics.gravity = (0,-9.81,0)
- Reference Basic gravity = (0,-9.80665,0)

Gravity既定値の差はmapping scaleではなく入力値の差として扱えるため、unit contractを阻害しない。

### 9.3 BF-002 Coordinate Mapping

SansaClothで既定済みのaxis meaning:
- +X = right
- +Y = up
- +Z = forward

Unityのaxis directionは上記と一致するため、Position/Directionのcomponent permutationやaxis sign flipは現時点では不要な候補である。

ただしSansaCloth canonical handednessは未確定である。したがってaxis direction一致だけを根拠にrotation mappingをidentityと確定しない。

Probe:
- CBF-001: basis Position (+X,+Y,+Z) mapping
- CBF-002: basis Direction (+X,+Y,+Z) mapping
- CBF-003: Surface Normal mapping
- CBF-004: World GravityはBody rotation後もWorld Spaceで同方向
- CBF-005: +90/-90 degree rotationの意味をReferenceとUnityで比較
- CBF-006: triangle windingから得るgeometric normalの向きを比較
- CBF-007: cross-product orientationを比較
- CBF-008: Body+Gravityを同一rigid transformしたcoordinate-rotation equivalence

CBF-005～007の結果からBackend rotation conversionを明示する。canonical handednessそのものは別途Core設計事項として扱う。

BF-002 status: **PASS (runtime confirmed, 2026-10-01)**。

Unity/Reference実測比較:
- Basis X/Y/Z: 一致
- Surface Normal +Y: 一致
- +90 degree Z applied to +X: 約(0,+1,0)で一致
- -90 degree Z applied to +X: 約(0,-1,0)で一致
- V00,V01,V11 winding normal: +Yで一致
- Cross(X,Y)=+Z / Cross(Y,Z)=+X / Cross(Z,X)=+Y: 一致
- Body +90 degree Z rotation applied to +X: 約(0,+1,0)で一致
- World Gravity: Body rotationによって暗黙回転しない

Unity floatの約5.96e-8級の丸め差は方向の不一致ではない。

Unity PC Backendでは、現ReferenceとのPosition / Direction / Normal / rigid Rotation mappingにcomponent permutationまたはaxis sign flipを追加しない。

このPASSはSansaCloth canonical handednessをUnityのhandednessへ固定する決定ではない。現ReferenceとUnity PC Backend境界がidentity mappingで成立することの確認である。

### 9.4 Probe実装境界

最初のUnity probeはSansaCloth solverをUnityへ移植しない。

Unity側で以下だけを生成・観測できる最小Harnessとする:
- known Position/Direction/Normal
- known Transform rotation
- World Gravity
- known triangle winding
- round-trip measurement output

結果はReference側の既知vector/rotation結果と比較できるテキストまたは機械可読値として取得する。

初期Probe実装:
- Unity: `engines/unity/pc/probe/SansaClothCoordinateProbe.cs`
- Unity手順: `engines/unity/pc/probe/README.md`
- Reference: `reference/crates/sansacloth-reference/examples/backend_coordinate_probe.rs`
- Unity出力prefix: `SANSA_BF|`
- Reference出力prefix: `SANSA_REF|`

Reference Probe実行:

```powershell
cd reference
cargo run -p sansacloth-reference --example backend_coordinate_probe
```

比較規約:
- UBF-001/002は1 Unity world unit = 1 canonical mのmapping contractを確認する。
- CBF-001/003はbasis/normalの成分対応を比較する。
- CBF-004はGravityの数値既定値ではなく、world-space vectorとして明示的に扱えることを確認する。
- CBF-005～007はReference実装とUnityのrotation/winding/cross-product結果を比較し、変換要否を決める。
- CBF-008はBody rotationがWorld Gravityを暗黙に回転させないことを確認する。
- Reference Probeのglam挙動をcanonical handednessの定義として扱わない。これは現Reference実装との比較基準である。

これにより、SurfaceQueryやSR-001の実装前にcoordinate/unit contractの誤りを分離する。

## 10. Unity PC BF-003 / BF-004 Probe Definition

### 10.1 BF-003 SurfaceReference Mapping

初期Unity Probeでは、Reference Fixtureと同じ0.20m x 0.10m flat meshを使用し、単一domainを次のように扱う。

- DomainId = 1
- U/V = mesh UV0
- UV範囲 = [0,1] x [0,1]
- fixed diagonal = V00 -> V11
- triangle winding = (V00,V01,V11), (V00,V11,V10)

UnityのMesh APIはUV channelとtriangle vertex indexを取得できる。UVはvertex indexと対応するため、単一domain・重複なしUVのProbeではSurfaceReferenceからsurface triangle/barycentric positionを解決できる。

ただしProductionではUV seam、UV overlap、複数domain、topology更新があり得る。したがって:
- Texture UVそのものを永続SurfaceReference IDとは定義しない。
- Runtime triangle indexをSurfaceReferenceへ保存しない。
- `domain_id + (u,v)` が論理参照であり、Backendがruntime meshへ解決する。
- Production stable mapping方式は本Probeの結果だけでは確定しない。

Probe:
- SRF-001: DomainId=1, UV=(0.25,0.75)を解決できる。
- SRF-002: identity placementで同じSurfaceReferenceを再解決できる。
- SRF-003: Body rigid transform後もDomainId/UVを変更せず同じ論理点を解決できる。
- SRF-004: resolved runtime triangle indexは観測してよいがSurfaceReference semanticsへ含めない。

BF-003 status: **PASS for rigid single-domain probe (runtime confirmed, 2026-10-01)**。

実測:
- DomainId=1を維持
- UV=(0.25,0.75)を維持
- identity / rigid transformの双方でresolved triangle=0
- Body rigid transform後もDomainId/UVを変更せず同じ論理表面を解決

ProductionのUV seam / overlap / multiple domain / topology updateに対するstable mappingは未確定であり、本PASSには含めない。

### 10.2 BF-004 SurfaceQuery Feasibility

初期ProbeのSurfaceQueryは、SurfaceReferenceをUV triangleへ解決し、barycentric interpolationでSurface Positionを得る。geometric Surface NormalとCurrent Positionからsigned Separationを計算する。

Probe:
- SQF-001: identity flat fixtureでSurface Positionを取得。
- SQF-002: outward Surface Normalを取得。
- SQF-003: Surface + Normal * 0.01m のCurrent PositionでSeparation = +0.01m。
- SQF-004: rigid transform後も同じSurfaceReferenceでSurface Position/Normalを再評価。
- SQF-005: rigid transform後もNormal方向+0.01mでSeparation = +0.01m。

Unity 6にはMesh UV/triangle/read-only mesh data取得APIがあり、SkinnedMeshRendererにはdeformed mesh snapshot取得APIも存在する。このためstatic fixtureを超える実装経路は存在するが、CPU BakeMeshをProduction Runtime方式として採用することは本Checkpointでは決定しない。

BF-004 status: **PASS for static rigid fixture (runtime confirmed, 2026-10-01)**。

Reference / Unity実測比較:
- Identity Surface Position: (-0.05,0,0.025)で一致
- Identity Surface Normal: (0,1,0)で一致
- Identity Separation: +0.01mで一致
- Transformed Surface Position: (0.30,0.25,-0.075)で一致
- Transformed Surface Normal: 約(+1,0,0)で一致
- Transformed Separation: 約+0.01mで一致

Unity floatによる微小差は許容される。deformed/skinned mesh SurfaceQueryは後続Probe対象であり、このPASSには含めない。

### 10.3 Probe実装

- Unity: `engines/unity/pc/probe/SansaClothSurfaceQueryProbe.cs`
- Reference baseline: `reference/crates/sansacloth-reference/examples/backend_coordinate_probe.rs`

Unity Probeはvalidation専用であり、Production SurfaceQuery Backendではない。

## 11. BF-001～004 Runtime Result Summary

| Gate | Unity PC result | Scope |
|---|---|---|
| BF-001 Unit Mapping | PASS | 1 world unit = 1 canonical m |
| BF-002 Coordinate Mapping | PASS | Current Reference <-> Unity PC rigid mapping |
| BF-003 SurfaceReference Mapping | PASS | Rigid single-domain fixture |
| BF-004 SurfaceQuery Feasibility | PASS | Static rigid fixture |

次段階ではBF-005 Input SemanticsとBF-006 Output Semanticsを、SR-001 Flatの単一Case end-to-end probeとしてまとめて確認する。

BF-003/004のProduction拡張課題を解決するためだけにCheckpointを停止しない。Skinned/deformed mesh、stable mapping lifecycleは後続Backend実装課題として保持する。

## 12. Unity PC BF-005 / BF-006 SR-001 End-to-End Probe

### 12.1 Case

Probe Case ID: `UNITY-SR-001-C0-G0`

Reference Basic SR-001 Flatの6 casesのうち、最小境界確認として以下を使用する。

- Body Surface: SF-FLAT-001
- Cloth: CF-FLAT-001
- Control Points: 21 x 7 = 147
- Placement: PL-CONTACT-001
- Anchor: AF-BOTH-EDGES-001
- Gravity: Off = (0,0,0)
- Conformity: 0
- CollisionTolerance: 0m
- DomainId: 1
- Expected deformation: none

このCaseではSurfaceResponse algorithmの一致ではなく、入力意味論と最終出力意味論のend-to-end mappingを確認する。

### 12.2 BF-005 Input Semantics

Probe:
- ISF-001: 147 CPを構成できる。
- ISF-002: U=0およびU=1の14 CPだけがAnchor。
- ISF-003: Flat contact状態の全147 CPをContactとして表現できる。
- ISF-004: Contact=trueでもAnchor=falseの133 CPをpin/supportへ昇格しない。
- ISF-005: Gravity=(0,0,0)をWorld Space入力として保持する。
- ISF-006: Conformity=0を入力として保持する。
- ISF-007: SurfaceReference DomainId=1とnormalized U/Vを各CPへ保持する。

Expected:
- AnchorCount = 14
- ContactInputCount = 147
- DirectSupportCount = 14
- ContactOnlyUnsupportedCount = 133

BF-005 status: **PASS (runtime confirmed, 2026-10-01)**。

Unity実測:
- CPCount = 147
- AnchorCount = 14
- ContactInputCount = 147
- DirectSupportCount = 14
- ContactOnlyUnsupportedCount = 133
- World Gravity input = (0,0,0)
- Conformity input = 0

Contact=trueの133 non-anchor CPがUnsupportedのままであり、ContactをAnchor/Supportへ暗黙昇格しない意味論を確認した。

### 12.3 BF-006 Output Semantics

各CPについて検証用共通結果を次の意味で出力する。

- StableControlPointId
- Final Position [m]
- SurfaceReference
- Surface Normal
- SeparationDistance [m]
- Support

Contact出力はBasic v1 measurement conventionに従い、`SeparationDistance <= CollisionTolerance` からValidation側で導出できるため、Backend固有Contact stateを必須出力にしない。

Probe:
- OSF-001: Final Positionを147 CP取得。
- OSF-002: SurfaceReferenceを147 CP保持。
- OSF-003: Surface Normalを147 CP取得。
- OSF-004: SeparationDistanceを147 CP取得。
- OSF-005: Supportを147 CP取得。
- OSF-006: Final PositionはInitial Positionと一致。
- OSF-007: Normal=(0,+1,0)。
- OSF-008: SeparationDistance=0m。
- OSF-009: SupportCount=14。
- OSF-010: derived ContactCount=147。

BF-006 status: **PASS (runtime confirmed, 2026-10-01)**。

Unity実測:
- Final Position deviation max = 0m
- Normal deviation max = 0
- Separation absolute max = 0m
- SupportCount = 14
- derived ContactCount = 147
- representative CP id = 0 / 73 / 146
- representative SurfaceReference = DomainId 1 + normalized UV
- center CP id=73 is Contact相当かつUnsupported

SR-001 C=0/G=0の境界でFinal Position / SurfaceReference / Surface Normal / Separation / Supportを共通意味へ戻せることを確認した。

### 12.4 Probe Result Format

Console出力は個別147 CPを常時展開せず、まずaggregateを出力する。

```text
SANSA_BF|SR-001.CP_COUNT|147
SANSA_BF|SR-001.ANCHOR_COUNT|14
SANSA_BF|SR-001.CONTACT_INPUT_COUNT|147
SANSA_BF|SR-001.DIRECT_SUPPORT_COUNT|14
SANSA_BF|SR-001.CONTACT_ONLY_UNSUPPORTED_COUNT|133
SANSA_BF|SR-001.FINAL_POSITION_DEVIATION_MAX_M|0
SANSA_BF|SR-001.NORMAL_DEVIATION_MAX|0
SANSA_BF|SR-001.SEPARATION_ABS_MAX_M|0
SANSA_BF|SR-001.DERIVED_CONTACT_COUNT|147
```

加えて代表CPとしてcorner / center / opposite cornerの3点を出力し、SurfaceReferenceとPositionを確認する。

## 13. Unity PC BF-007 Basic Fixture Mapping Probe

### 13.1 Fixture Definition

Unity側でReference fixtureと同じanalytic definitionを構成する。

Common:
- width = 0.20m
- depth = 0.10m
- feature width = 0.10m
- U samples = 21
- V samples = 7
- CP/vertex count = 147
- fixed diagonal = V00 -> V11
- triangle count = 20 x 6 x 2 = 240

Fixtures:
- SR-001 Flat: height 0
- SR-002 Convex-Up: raised cosine height +0.03m
- SR-003 Convex-Side: SR-002 shapeをZ axis -90 degree rigid rotation
- SR-004 Concave-Shallow: raised cosine depth -0.02m
- SR-005 Concave-Deep: raised cosine depth -0.05m

Anchor:
- SR-001/002/004/005: AF-BOTH-EDGES-001 => 14 anchors
- SR-003: AF-EDGE-001 => 7 anchors

### 13.2 Probe

- FMF-001: 5 fixtureすべて147 vertices / 240 triangles。
- FMF-002: Flat center=(0,0,0)。
- FMF-003: Convex-Up center=(0,+0.03,0)。
- FMF-004: Convex-Side center=(+0.03,0,0) within float tolerance。
- FMF-005: Concave-Shallow center=(0,-0.02,0)。
- FMF-006: Concave-Deep center=(0,-0.05,0)。
- FMF-007: non-side fixtureのcenter normal=(0,+1,0)。
- FMF-008: Convex-Side center normal=(+1,0,0) within float tolerance。
- FMF-009: fixed windingのfirst triangle outward normalはnon-sideで+Y、sideで+X。
- FMF-010: AnchorCountはSR-003のみ7、他は14。

BF-007 status: **PASS (runtime confirmed, 2026-10-01)**。

Unity実測:
- 全5 fixtures: VertexCount=147 / TriangleCount=240 / CPCount=147
- SR-001 Flat: center=(0,0,0), AnchorCount=14, center/first triangle normal=+Y
- SR-002 Convex-Up: local max=+0.03m, center=(0,+0.03,0), AnchorCount=14, center/first triangle normal=+Y
- SR-003 Convex-Side: local max=+0.03m, center≈(+0.03,0,0), AnchorCount=7, center/first triangle normal≈+X
- SR-004 Concave-Shallow: local min=-0.02m, center=(0,-0.02,0), AnchorCount=14, center/first triangle normal=+Y
- SR-005 Concave-Deep: local min=-0.05m, center=(0,-0.05,0), AnchorCount=14, center/first triangle normal=+Y

SR-003の約1e-9～1e-8級の残差はUnity float/quaternion計算による丸め差であり、fixture mappingの不一致ではない。

### 13.3 Implementation

- Unity: `engines/unity/pc/probe/SansaClothBasicFixtureProbe.cs`

このProbeはFixture Mappingのみを検証し、30 Basic RunsのSurfaceResponse数値一致を要求しない。

## 14. Unity PC BF-008 Validation Capture Probe

### 14.1 Capture Scope

CheckpointではProduction serializationを定義しない。Validation専用の一時captureとして、SR-001 C=0/G=0から以下を取得する。

Metadata:
- EventId
- TimestampUtc
- TestId
- Backend
- CPCount

Final CP Measurement:
- StableControlPointId
- Final Position [m]
- SurfaceReference DomainId/U/V
- Surface Normal
- SeparationDistance [m]
- Support
- derived Contact

Aggregate Measurement:
- ContactCount
- SupportCount
- MeanSeparation
- MaxSeparation
- MaxPenetration
- MaxPositionDeviation
- RMSPositionDeviation

Stage MeasurementはReference専用debug情報のため必須としない。

### 14.2 Capture Transport

ProbeはJSON Lines validation artifactをUnity `Application.temporaryCachePath` 以下へ書き出し、保存先をConsoleへ出力する。

これはBackend-neutral serialization formatの決定ではない。Checkpointで「共通意味の測定結果をBackend実行から外部取得できる」ことだけを確認する。

Expected:
- header 1 record
- final CP 147 records
- aggregate 1 record
- total 149 JSONL records
- file pathをConsoleから取得可能
- aggregate: ContactCount=147, SupportCount=14, separation/deviation metrics=0

BF-008 status: **PASS (runtime confirmed, 2026-10-01)**。

Unity実測:
- EventId = 7de6bb81-db2b-4eaf-bf1d-8efacb220d02
- TestId = UNITY-SR-001-C0-G0
- JSONL RecordCount = 149
- Header = 1
- Final CP Measurement = 147
- Aggregate Measurement = 1
- StableControlPointId = 0..146, unique and contiguous
- SurfaceReference DomainId = 1 for all CP
- Support = Anchor 14 / Unsupported 133
- derived ContactCount = 147
- Surface Normal = (0,+1,0) for all CP
- SeparationDistance = 0m for all CP
- Aggregate ContactCount = 147
- Aggregate SupportCount = 14
- MeanSeparation / MaxSeparation / MaxPenetration = 0m
- MaxPositionDeviation / RMSPositionDeviation = 0m

UVから期待Positionを再構成した場合の実測最大差は約1.03e-8mで、Unity float精度による丸め範囲である。

Validation実行結果からFinal CP MeasurementとAggregate Measurementを外部JSONL artifactとして取得できることを確認した。

### 14.3 Implementation

- Unity: `engines/unity/pc/probe/SansaClothValidationCaptureProbe.cs`

## 15. Unity PC Runtime Backend Feasibility Checkpoint Result

2026-10-01のUnity PC実測結果:

| Gate | Result | Confirmed scope |
|---|---|---|
| BF-001 Unit Mapping | PASS | canonical m <-> Unity world unit |
| BF-002 Coordinate Mapping | PASS | Position / Direction / Normal / rigid Rotation |
| BF-003 SurfaceReference Mapping | PASS | rigid single-domain fixture |
| BF-004 SurfaceQuery Feasibility | PASS | static rigid fixture |
| BF-005 Input Semantics | PASS | SR-001 C=0/G=0 semantic boundary |
| BF-006 Output Semantics | PASS | Final common-semantic result |
| BF-007 Basic Fixture Mapping | PASS | SR-001..005 analytic fixtures and anchors |
| BF-008 Validation Capture | PASS | Final CP + Aggregate JSONL capture |

判定: **Unity PC Runtime Backend Feasibility Checkpoint PASS**。

BF-001～008にBLOCKEDまたはOPENは残らない。

このPASSが意味するもの:
- 現Reference semanticsをUnity PC Backendへ写像できる実装経路を確認した。
- Unity固有の座標/単位都合でCore semanticsを変更する必要は現時点で認められない。
- SR-001～005の入力fixtureをUnity側へ構成できる。
- Backend結果をBasic Validationの共通測定意味へ戻す経路を確認した。

このPASSが意味しないもの:
- Unity Production Backend完成
- Reference SolverとUnity Backendの30 Runs数値一致
- Skinned/deformed mesh stable SurfaceReference完成
- Runtime性能保証
- Simulation LOD完成
- Mobile/Quest対応
- Bake対応

Checkpoint後のRust quality gate (2026-10-01):
- `cargo fmt --check`: PASS
- `cargo clippy --workspace --all-targets -- -D warnings`: PASS
- `cargo test --workspace`: PASS (Core 7 / Fixture 13 / Reference 57 = 77 tests)
- `git status`: clean

次段階はProbe順序に従い、Reference BasicとBackend間の最小fixture exchange formatを定義してから、Unity PC SR-001 Flatの実solver end-to-end実装へ進む。

Fixture exchange設計:
- `docs/ja-JP/draft/verification/runtime-backend-fixture-exchange.md`

O3DE PC検証:
- `docs/ja-JP/draft/verification/runtime-backend-o3de-pc.md`
- Unity 30-case SurfaceResponse比較完了後の第2 Backendとして、BF-001/BF-002のUnit/Coordinate Probeから開始する。

## 16. 未確定事項

- Unity側の具体的なmesh/deformation API
- Unityでのstable SurfaceReference保持方式
- handedness/axis conversionの正式定義
- Runtime mesh更新時のSurfaceReference lifecycle
- Backend Validation fixture exchange format
- Backend結果とReference結果の許容誤差
- O3DE側の具体的なmesh/deformation API
- Godot Backendのrepository layoutと初期Target

> SansaCloth > docs > ja-JP > draft > verification > RuntimeBackend.Feasibility
