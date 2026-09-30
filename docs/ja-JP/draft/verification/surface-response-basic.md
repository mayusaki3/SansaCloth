# SurfaceResponse.Basic Verification / 表面応答基本検証

> SansaCloth > docs > ja-JP > draft > verification > SurfaceResponse.Basic

## 1. 目的

Reference Surface Solver v1の基本意味論をSR-001～005で検証する。

Basic Suiteは正しさを優先し、性能値や未確定な物理品質へ根拠のないPASS/FAIL閾値を設定しない。

## 2. 共通条件

- Cloth Fixture: CF-FLAT-001
- Cloth Size: 0.20 x 0.10 m
- Baseline Solver CP: 21 x 7
- Internal Unit: m
- Conformity: 0.0 / 0.5 / 1.0
- Gravity: Off / On
- Gravity On baseline environment: (0, -9.80665, 0) m/s²
- Gravity Off: zero vector
- Basic Validation暫定Execution Profile:
  - QuasiStaticGravityScale / 準静的重力スケール = 0.1
  - ConformityReach / 表面追従範囲 = 0.02 m
  - CollisionTolerance / 衝突許容誤差 = 0 m

上記3値は30 Basic Runsを再現可能にするためのReference Validation専用暫定値であり、Core Property、製品既定値、最終推奨値ではない。Basic測定結果に基づき変更してよい。

CF-FLAT-001の21 x 7 CPは、Reference v1のordered-strip Solverへ147点を1本として入力しない。V一定の21点stripを7本生成して個別に解き、Validation Resultで147 CPへ再結合する。これはReference v1のFixture Runner責務とする。

Gravity magnitudeをReference v1の変位へ直接変換しない。準静的近似はReferenceSolverSettingsで扱う。

## 3. Case Matrix

各SR Testは6 Cases:

| Case | Gravity | Conformity |
|---|---|---:|
| C01 | Off | 0.0 |
| C02 | Off | 0.5 |
| C03 | Off | 1.0 |
| C04 | On | 0.0 |
| C05 | On | 0.5 |
| C06 | On | 1.0 |

5 Tests x 6 Cases = 30 Validation Runs。

## 4. SR-001 Flat / 平面基準

- Body: SF-FLAT-001
- Body Transform: Identity
- Cloth: CF-FLAT-001
- Placement: PL-CONTACT-001
- Anchor: AF-BOTH-EDGES-001

目的:
- 不要変形がない
- Anchor維持
- Gravity OffでGravity変位なし
- Collision安全性
- 決定性

必須Assertion:
- Finite
- AnchorPreserved
- NoInvalidPenetration
- GravityZeroResponse (C01-C03)
- Deterministic
- NoUnnecessaryDeformation

NoUnnecessaryDeformation toleranceはTBD。

## 5. SR-002 Convex-Up / 上向き凸面

- Body: SF-CONVEX-001
- Transform: Identity
- Cloth: CF-FLAT-001
- Placement: Base Plane
- Anchor: AF-BOTH-EDGES-001

凸面へClothを事前追従させない。中央部の幾何学的競合をCollision Responseが処理し、Conformityは周辺形状への追従を担当する。

必須Assertion:
- Finite
- AnchorPreserved
- NoInvalidPenetration
- GravityZeroResponse
- Deterministic

Conformity差は当初Measurementとして記録する。

## 6. SR-003 Convex-Side / 横向き凸面

- Body Geometry: SF-CONVEX-001
- Body Transform: Side Orientation
- Cloth: CF-FLAT-001
- Placement: transformed Base Plane
- Anchor: AF-EDGE-001

SR-002と同じGeometryを使用し、Body/Placement/Surface FrameをTransformする。GravityはWorld Environmentに残す。

主検証:
- Support
- Gravity direction independence
- Conformity
- Contact/Separation
- No Conformity Adhesion

NoConformityAdhesionは初期段階ではDiagnostic + Visual Validationを併用し、判定式確定前は恣意的なOverall FAIL条件にしない。

## 7. SR-004 Concave-Shallow / 浅い凹面

- Body: SF-CONCAVE-SHALLOW-001
- Depth: 0.02 m
- Placement: Base Plane
- Anchor: AF-BOTH-EDGES-001

Bridge -> Gravity -> Conformityの差を観測する。

ConformityTrendとしてMean Separationを測定する。

期待傾向:
```text
D(1.0) <= D(0.5) <= D(0.0)
```

ただし初期v1ではMeasurementであり、厳密なHard Assertionではない。

## 8. SR-005 Concave-Deep / 深い凹面

- Body: SF-CONCAVE-DEEP-001
- Depth: 0.05 m
- Placement: Base Plane
- Anchor: AF-BOTH-EDGES-001

Conformity=1でも凹面底への完全Contactを要求しない。

優先条件:
- Finite
- Stable
- No invalid penetration
- No artificial adhesion

底まで届かないこと自体はFAILではない。

## 9. 共通Measurements

Per Control Point:
- Position [m]
- Normal
- SurfaceReference
- SurfaceOffset [m]
- Contact
- SeparationDistance [m]
- Support (Reference debug)

Aggregate:
- ContactCount
- SupportCount
- MeanSeparation
- MaxSeparation
- MaxPenetration
- MaxPositionDeviation
- RMSPositionDeviation

Reference Basic v1での集計定義:
- ContactCount: Final SeparationDistance <= CollisionTolerance のCP数
- SupportCount: Final Support != Unsupported のCP数
- MeanSeparation: Final SeparationDistanceの算術平均 [m]
- MaxSeparation: Final SeparationDistanceの最大値 [m]
- MaxPenetration: max(0, -Final SeparationDistance) の最大値 [m]
- MaxPositionDeviation: Initial PlacementからFinal Positionまでの距離の最大値 [m]
- RMSPositionDeviation: Initial PlacementからFinal Positionまでの距離のRMS [m]

ContactCountの上記定義はReference Basic v1の測定規約であり、将来の独立したContact Stateを置き換えるCore定義ではない。

Reference Stage:
- Initial
- Support
- Bridge
- GravityResponse
- ConformityResponse
- CollisionResponse
- Final

Performance:
- Stage times
- TotalSolverTime
- PeakMemory

Performanceは初期BasicではMEASUREDとし、PASS/FAIL thresholdを設けない。

## 10. Gravity Response Tests

- GRV-001 Gravity=0 -> displacement=0
- GRV-002 Anchor displacement=0
- GRV-003 BothEdges -> center region has largest response tendency
- GRV-004 OneEdge -> response increases away from anchor
- GRV-005 Gravity reversal -> response direction reverses
- GRV-006 Body-only rotation does not rotate World Gravity
- GRV-007 Body+Gravity rotation -> inverse transformed result equivalent
- GRV-008 Gravity stage does not perform collision correction
- GRV-009 Gravity stage does not perform conformity
- GRV-010 deterministic
- GRV-011 resolution does not diverge
- GRV-012 finite

## 11. Conformity Response Tests

- CONF-001 Conformity=0 -> stage displacement=0
- CONF-002 Anchor displacement=0
- CONF-003 unsupported CP outside Reach is not attracted
- CONF-004 eligible CP response increases with Conformity
- CONF-005 Conformity=1 does not attract outside Reach
- CONF-006 convex collision resolution does not depend only on Conformity
- CONF-007 shallow concavity can generate conformity response
- CONF-008 deep concavity does not force bottom contact
- CONF-009 side convex unsupported region does not adhere
- CONF-010 coordinate rotation equivalence
- CONF-011 does not modify Gravity input
- CONF-012 does not modify Anchor
- CONF-013 deterministic
- CONF-014 finite
- CONF-015 resolution does not diverge

## 12. Cloth / Placement / Anchor / Mapping Tests

Cloth:
- CLOTH-001 dimensions 0.20 x 0.10m
- CLOTH-002 U/V boundary
- CLOTH-003 baseline 21x7 CP
- CLOTH-004 deterministic CP identity
- CLOTH-005 position reconstruction from U/V

Placement:
- PLACE-001 contact offset 0
- PLACE-002 separated +0.01m
- PLACE-003 penetrating -0.01m
- PLACE-004 transform consistency

Anchor:
- ANCHOR-001 BothEdges U=0/1
- ANCHOR-002 OneEdge only selected side
- ANCHOR-003 resolution-independent meaning

Mapping:
- MAP-001 Cloth U/V -> Body U/V 1:1
- MAP-002 mapping preserved by Body Transform
- MAP-003 independent of Runtime Triangle ID

## 13. Rust実装前テスト一覧 / Implementation Gate

Rust実装開始前に、以下を番号付きTestとして実装対象に固定する。

### Core

- CORE-001 internal Lengthはm
- CORE-002 cm display/input -> m変換
- CORE-003 mm display/input -> m変換
- CORE-004 valid SurfaceReference
- CORE-005 barycentric normalization
- CORE-006 SurfaceFrame orthonormal
- CORE-007 SurfaceOffsetからPositionを再構成
- CORE-008 Transform後もSurfaceReferenceの意味を維持
- CORE-009 AnchorがCloth Control Pointを正しく参照
- CORE-010 Gravity vector transform

### Fixture

- FIX-001 flat plane
- FIX-002 convex maximum height
- FIX-003 convex feature width
- FIX-004 shallow concavity depth
- FIX-005 deep concavity depth
- FIX-006 same input -> deterministic mesh
- FIX-007 resolution変更でanalytic shape semantics維持
- FIX-008 CF-FLAT-001 dimensions
- FIX-009 AF-EDGE-001
- FIX-010 AF-BOTH-EDGES-001
- FIX-011 fixed diagonal = V00 -> V11
- FIX-012 triangle winding produces outward normal
- FIX-013 every triangle satisfies dot(geometric, analytic) > 0

### Reference Support

- REF-SUP-001 Anchor -> Support
- REF-SUP-002 Contactだけで全自由度を固定しない
- REF-SUP-003 unsupported pointを識別

### Reference Bridge

- REF-BRI-001 Support位置を維持
- REF-BRI-002 continuous bridge
- REF-BRI-003 Bridge単独ではConcave Surfaceへ吸着しない

### Reference Gravity

GRV-001～012をReference unit/integration testとして使用する。

### Reference Conformity

CONF-001～015をReference unit/integration testとして使用する。

### Reference Collision

- REF-COL-001 non-penetrating pointを不要に移動しない
- REF-COL-002 penetrating pointをoutwardへ補正
- REF-COL-003 final penetration <= configured tolerance

### Integration

- INT-SR-001 SR-001 six cases
- INT-SR-002 SR-002 six cases
- INT-SR-003 SR-003 six cases
- INT-SR-004 SR-004 six cases
- INT-SR-005 SR-005 six cases

これら5 Integration Test Definitionから30 Basic Runsを生成する。

### Bake

Reference Geometry Bake開始時に以下を必須化する。

- BAKE-001 CP resultをRender Meshへ転送
- BAKE-002 Anchor vertex維持
- BAKE-003 Flatで不要変形なし
- BAKE-004 Reference Positionへ近似
- BAKE-005 Normal再構築
- BAKE-006 Render resolution変更でSurface破綻なし
- INT-BAKE-001～005: SR-001～005相当のBake比較

Bake実装はReference Solverの内部Stageへ直接依存せず、共通SurfaceResponseResult/IRを入力とする。

## 14. Basic Measurement実測記録

2026-09-30のReference Basic 21 x 7 CP / 暫定Execution Profileで、30 Basic RunsおよびAggregate Measurementを実行した。

SR-004 Concave-ShallowのMeanSeparation実測:

| Gravity | Conformity 0.0 | Conformity 0.5 | Conformity 1.0 |
|---|---:|---:|---:|
| Off | 0.004374254 m | 0.003741016 m | 0.003107777 m |
| On | 0.001493235 m | 0.001027579 m | 0.000561923 m |

両Gravity条件で、観測対象としていた `D(1.0) <= D(0.5) <= D(0.0)` が成立した。ただし、現段階ではMeasurementでありHard Assertionへ昇格しない。

SR-005 Concave-DeepのConformity=1.0実測:

| Case | Gravity | ContactCount | MeanSeparation | MaxSeparation |
|---|---|---:|---:|---:|
| C03 | Off | 84 / 147 | 0.007621851 m | 0.050000000 m |
| C06 | On | 98 / 147 | 0.005263063 m | 0.040000000 m |

Conformity=1.0でも全CP Contactにはならず、深い凹面底への完全Contactを要求しない現行意味論と整合した。この結果だけを根拠にNoConformityAdhesionの一般Hard Assertion式は確定しない。

30 Basic Runs、Aggregate Measurement tests、既存Core/Fixture/Reference testsは機能テストとしてPASSした。品質ゲートで検出されたrustfmt差分および現行Clippyの `chunks_exact_to_as_chunks` 指摘は実装修正対象とし、測定結果そのものとは分離して扱う。

## 15. Collision専用Suiteとの分離

PL-PENETRATING-001は30 Basic Runsへ混ぜず、別の `SurfaceResponse.Collision` Suiteで使用する。

検証Suite構成候補:

```text
SurfaceResponse.Basic
SurfaceResponse.Collision
SurfaceResponse.Resolution
SurfaceResponse.Performance
```

## 16. Coverage

Core / Reference / Bakeの対象コードはUnit Test Coverage 100%を完了条件とする。到達不能分岐等を除外する場合は理由を文書化する。利用可能ならBranch Coverageも確認する。

## 17. 未確定事項

- AnchorTolerance
- CollisionTolerance
- NoUnnecessaryDeformation tolerance
- ConformityTrendをHard Assertionへ昇格する条件
- NoConformityAdhesionの完全自動判定式
- ReferenceSolverSettings各値
- Performance threshold
- Visual Validation判定基準

> SansaCloth > docs > ja-JP > draft > verification > SurfaceResponse.Basic
