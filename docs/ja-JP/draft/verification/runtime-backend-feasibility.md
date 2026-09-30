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

BF-001 status: **PASS (design mapping)**。
Unity実行環境でUBF-001～004を確認後、runtime-confirmed PASSへ更新する。

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

CBF-005～007の結果からcanonical handednessとBackend rotation conversionを明示する。

BF-002 status: **OPEN**。
Position/Direction axis mappingはidentity候補。Rotation/handedness mappingはProbe完了まで未確定。

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

## 10. 未確定事項

- Unity側の具体的なmesh/deformation API
- Unityでのstable SurfaceReference保持方式
- handedness/axis conversionの正式定義
- Runtime mesh更新時のSurfaceReference lifecycle
- Backend Validation fixture exchange format
- Backend結果とReference結果の許容誤差
- O3DE側の具体的なmesh/deformation API
- Godot Backendのrepository layoutと初期Target

> SansaCloth > docs > ja-JP > draft > verification > RuntimeBackend.Feasibility
