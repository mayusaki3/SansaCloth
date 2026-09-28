# Validation Fixtures v1 / 検証Fixture v1

> SansaCloth > docs > ja-JP > draft > specification > Validation Fixtures

## 1. 目的

SR-001～005で使用する解析的Body Surface、Cloth Fixture、Surface Atlas、Sampling、Placement、Anchorを定義する。

Fixture v1は検証専用であり、製品Avatarや人体形状の標準ではない。

内部長さ単位は meter (m)。

## 2. Fixture Coordinate

Fixture v1のみ次を使用する。

- +X = Right / 右
- +Y = Up / 上
- +Z = Forward / 前
- Surface extent: X-Z
- Feature height/depth: Y
- Base outward normal: +Y

これはFixture規約でありCore Solverの軸仮定ではない。

共通外形:
- Width X = 0.20 m
- Depth Z = 0.10 m
- U,V = [0,1]

```text
x = (u - 0.5) * 0.20
z = (v - 0.5) * 0.10
```

## 3. Raised Cosine / 持ち上げコサイン

Feature中心 x=0、Feature Width W、Height/Depth H:

```text
f(x) = H/2 * (1 + cos(2*pi*x/W))  for |x| <= W/2
       0                           otherwise
```

Derivative:

```text
df/dx = -(H*pi/W) * sin(2*pi*x/W)
```

中心でH、境界で0、境界一次微分0、左右対称で、平面へC1連続する。

## 4. Body Fixtures

| FixtureID | 日本語 | Surface |
|---|---|---|
| SF-FLAT-001 | 平面 | P=(x,0,z) |
| SF-CONVEX-001 | 凸面 | P=(x,+f,z), W=0.10m, H=0.03m |
| SF-CONCAVE-SHALLOW-001 | 浅い凹面 | P=(x,-f,z), W=0.10m, H=0.02m |
| SF-CONCAVE-DEEP-001 | 深い凹面 | P=(x,-f,z), W=0.10m, H=0.05m |

SR-003はSF-CONVEX-001を再利用し、Body Transformのみ変更する。

## 5. Analytic Surface Atlas

SR-001～005ではAutomatic Atlas Builderを使用しない。Fixture generatorがGround Truth Atlasを直接生成する。

Authoring Surface Reference:

```text
SurfaceDomainID
SurfaceCoordinate
  U
  V
```

同じFixtureは再生成しても同じStable Domain identityを得られる設計とする。実際のID表現はTBD。

UはFixture X、VはFixture Zへ正規化対応する。ただしこれはFixture v1固有でありCore全体のU/V規則ではない。

Texture UVとSansaCloth Surface Coordinateを同一視しない。

## 6. Mesh Sampling

Regular U/V gridを使用する。

| Profile | Samples |
|---|---:|
| Low | 11 x 5 |
| Medium | 41 x 11 |
| High | 161 x 41 |

各quadは固定対角線で2 triangleへ分割し、全解像度で同じ規則を使用する。

quadの頂点を次のように定義する。

```text
V01 ---- V11
 |        |
 |        |
V00 ---- V10

V00 = (u_i,     v_j)
V10 = (u_{i+1}, v_j)
V01 = (u_i,     v_{j+1})
V11 = (u_{i+1}, v_{j+1})
```

固定対角線は `V00 -> V11` とする。

Base Outward Normal = +Y と整合するWindingは次の通り。

```text
Triangle A = (V00, V01, V11)
Triangle B = (V00, V11, V10)
```

平面Fixtureでは、右手系cross product `cross(P1-P0, P2-P0)` により両TriangleのGeometric Normalが+Yとなる。凸面・凹面でも同じ頂点順序を使用する。

全Triangleについて `dot(TriangleNormal, AnalyticNormal) > 0` を検証する。

Analytic Normal、Mesh Geometric Normal、Render Vertex Normalを区別する。

## 7. CF-FLAT-001 / 平面布Fixture

- Width = 0.20 m
- Depth = 0.10 m
- U,V = [0,1]
- Bodyとは独立したCloth Surface Domainを持つ。
- Fixture形状とPlacementを分離する。

Baseline Solver Control Points:
- Medium = 21 x 7

Resolution Suite:
- Low = 11 x 5
- Medium = 21 x 7
- High = 41 x 11

Control PointはTriangle IDではなくCloth Surface Coordinateで意味付けする。

## 8. Placement Profiles / 配置Profile

### PL-CONTACT-001 / 接触配置
Initial Separation = 0 m。

### PL-SEPARATED-001 / 離隔配置
Initial Separation = +0.01 m。

### PL-PENETRATING-001 / 侵入配置
Initial Offset = -0.01 m。

±0.01mはValidation Fixture値であり製品規格ではない。

Convex/Concave BasicではClothをBody Surfaceへ事前追従させず、Fixture Base Planeを基準とした平面配置を使用する。

## 9. Anchor Profiles / 固定点Profile

### AF-BOTH-EDGES-001
U=0およびU=1のEdge上Control PointをAnchorとする。

### AF-EDGE-001
指定した片側EdgeのみをAnchorとする。

AnchorはCloth Domain + SurfaceCoordinateで意味付けし、Solver/Mesh resolution変更に依存しない。

## 10. Initial Body Mapping

SR Fixtureでは初期単純化として:

```text
Cloth(U,V) -> Body(U,V)
```

の1:1 mappingを使用する。

これはFixture固有であり、一般AvatarではSurface Atlas/Binding/Connectivityを使用する。

## 11. Shape / Atlas Tests

Shape:
- SHAPE-001 center height/depth
- SHAPE-002 feature ends y=0
- SHAPE-003 feature ends dy/dx=0
- SHAPE-004 symmetry
- SHAPE-005 outside feature y=0
- SHAPE-006 finite positions
- SHAPE-007 finite normals
- SHAPE-008 normal length=1
- SHAPE-009 outward winding
- SHAPE-010 position continuity
- SHAPE-011 first derivative continuity
- SHAPE-012 deterministic mesh generation

Analytic Atlas:
- AAT-001 center mapping
- AAT-002 UV boundary
- AAT-003 flat y=0
- AAT-004 convex center 0.03m
- AAT-005 shallow center -0.02m
- AAT-006 deep center -0.05m
- AAT-007 analytic normal finite/normalized
- AAT-008 same UV across resolutions
- AAT-009 binding convergence
- AAT-010 authoring reference stable across resolution
- AAT-011 Runtime Triangle ID may change
- AAT-012 Body Transform preserves UV meaning
- AAT-013 anchors resolution-independent
- AAT-014 deterministic atlas regeneration

## 12. 未確定事項

- Stable SurfaceDomainIDの具体的表現
- Runtime binding tolerance
- Mesh/analytic convergence threshold

> SansaCloth > docs > ja-JP > draft > specification > Validation Fixtures
