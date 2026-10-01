# Runtime Backend Fixture Exchange / Runtime Backend Fixture交換

> Draft validation interchange for SansaCloth Runtime Backend bring-up.

## 1. 目的

Reference Basic Validationの同一Fixture/Case入力をRuntime Backendへ渡し、Backend側で手作業による再定義をせずに同じ検証入力を構成できる最小交換形式を定義する。

本形式はValidation専用であり、Product serialization、Authoring project format、Runtime asset format、Core API、Backend-neutral contractの確定版ではない。Unity PCとReference間で最初に使用し、O3DE PCで同じ情報境界が成立することを確認した後にのみ共通contract化を検討する。

## 2. 設計原則

### 2.1 意味を交換し、Referenceアルゴリズムを交換しない

交換対象:
- Body Surface geometry
- logical SurfaceReference domain
- Cloth Control Point
- strip/order topology
- Anchor
- Contact input
- World Gravity
- Conformity
- CollisionTolerance

交換しないもの:
- Reference Solver stage snapshot
- GravitySupportLayout
- quasi_static_gravity_scale
- conformity_reach_m
- characteristic_length_m
- Reference内部Support Weight / Stage中間位置
- Runtime triangle ID
- Unity component/instance ID
- Backend固有solver state

`GravitySupportLayout`等を交換形式へ入れるとReference-v1のアルゴリズムをBackend contractへ固定するため、含めない。

### 2.2 Fixtureはresolved dataとして交換する

`Flat` / `Convex` / `Concave` やraised-cosine式をBackendへ解釈させない。Reference側でValidation Fixtureを次へ解決してから交換する。

- Body Surface vertex position / normal / normalized U/V
- triangle topology
- Cloth CP position
- SurfaceReference
- Anchor / Contact

SR-003 Convex-Sideのrigid transformも交換前に適用する。これによりFixture生成式とBackend SurfaceQuery/Solverの検証を分離する。

### 2.3 Canonical unit / coordinate

- Length: metre
- Gravity: m/s^2
- +X: right
- +Y: up
- +Z: forward

交換形式自体はUnity unitへ変換しない。Backend Mappingが必要な場合は読み込み境界で変換する。

## 3. File Format

初期形式はUTF-8 JSON、1 file = 1 validation caseとする。Rust/Unity双方で検査しやすく、diff可能であり、Production serialization性能を本検証へ持ち込まないためである。

Format ID: `sansacloth.validation.fixture-exchange/0`

version 0はValidation用Draftであり互換性保証を行わない。

## 4. Logical Structure

```json
{
  "format": "sansacloth.validation.fixture-exchange/0",
  "case_id": "SR-001-C0-G0",
  "coordinate": {
    "length_unit": "m",
    "gravity_unit": "m/s^2",
    "x": "right",
    "y": "up",
    "z": "forward"
  },
  "body_surface": {
    "domain_id": 1,
    "vertices": [
      {
        "position_m": [-0.1, 0.0, -0.05],
        "normal": [0.0, 1.0, 0.0],
        "uv": [0.0, 0.0]
      }
    ],
    "triangles": [[0, 21, 22]]
  },
  "cloth": {
    "control_points": [
      {
        "stable_id": 0,
        "strip_id": 0,
        "strip_order": 0,
        "position_m": [-0.1, 0.0, -0.05],
        "surface_reference": {"domain_id": 1, "u": 0.0, "v": 0.0},
        "anchor": true,
        "contact": true
      }
    ]
  },
  "inputs": {
    "world_gravity_m_per_s2": [0.0, 0.0, 0.0],
    "conformity": 0.0,
    "collision_tolerance_m": 0.0
  }
}
```

上記は構造例で1 vertex / 1 CPだけを示す。実SR-001は147 vertices / 240 triangles / 147 CPを持つ。

## 5. Body Surface

Basic Validation v1では1 domainを使用する。

- `domain_id = 1`
- SurfaceReference = `domain_id + normalized (u,v)`
- runtime triangle indexをSurfaceReferenceとして保存しない。

各vertex:
- `position_m: [x,y,z]`
- `normal: [x,y,z]`
- `uv: [u,v]`

NormalはReference analytic fixtureのoutward normalをresolved dataとして出力し、Backendがanalytic formulaを再実装する必要をなくす。

Triangle:
- zero-based vertex indices
- Reference fixtureと同じoutward winding
- fixed diagonal V00 -> V11

Basic 21 x 7 samplingは147 vertices / 240 triangles。Triangle indexはmesh topology用でありlogical SurfaceReference IDではない。

## 6. Cloth

### 6.1 Control Point identity

各CPは`stable_id`を持つ。Basicでは0..146、unique、row-majorで `stable_id = v_index * 21 + u_index`。

### 6.2 Strip topology

Reference v1 Solverはalready-ordered stripを処理するため、アルゴリズム名ではなく入力topologyとして以下を交換する。

- `strip_id`: BasicではV row、0..6
- `strip_order`: row内U order、0..20

BackendはReferenceの`GravitySupportLayout`を受け取らず、同じcloth orderingを再構成する。

### 6.3 SurfaceReference

各CPは`domain_id`とnormalized `u/v`を持つ。Runtime triangle IDは含めない。

### 6.4 Anchor / Contact

`anchor`と`contact`は別々のboolとして交換する。Contact=trueをSupport/Pinへ暗黙昇格してはならない。

Basic Fixture生成時のContactはReference SurfaceQueryで初期位置のsigned Separationを評価して解決した値を出力する。Backend側で入力Contactを再推定して置換しない。

## 7. Case Inputs

交換する:
- `world_gravity_m_per_s2`
- `conformity`
- `collision_tolerance_m`

交換しないReference-v1 setting:
- `quasi_static_gravity_scale`
- `conformity_reach_m`
- `characteristic_length_m`
- `GravitySupportLayout`

Reference実行ではReference側Validation profileから与える。Runtime Backend固有設定も交換Fixtureの意味論とは分離する。

## 8. Basic Validation Mapping

| Scenario | Body resolved shape | Anchor | Transform |
|---|---|---|---|
| SR-001 Flat | flat | Both U edges | baked identity |
| SR-002 Convex-Up | convex +0.03m | Both U edges | baked identity |
| SR-003 Convex-Side | convex +0.03m | U=0 edge | baked Z -90 degree |
| SR-004 Concave-Shallow | concave -0.02m | Both U edges | baked identity |
| SR-005 Concave-Deep | concave -0.05m | Both U edges | baked identity |

各ScenarioはGravity Off/On × C=0/0.5/1の6 cases、合計30 files/casesを生成可能な構造とする。ただし最初の実装対象は`SR-001-C0-G0` 1 caseのみ。

Gravity On = (0,-9.80665,0)m/s^2。

## 9. Validation / Error Rules

Importerは少なくとも以下を拒否する:
- unknown `format`
- non-finite number
- invalid vector component count
- UV outside [0,1]
- duplicate `stable_id`
- duplicate `strip_id + strip_order`
- SurfaceReferenceが存在しないdomainを参照
- triangle vertex index out of range
- zero/invalid normal
- empty Body Surface / Cloth CP
- conformity outside [0,1]
- negative collision tolerance

配列順そのものをCP identityとして使用しない。

## 10. Test Specification Before Implementation

Exporter/Importer実装前に以下をテストIDとして固定する。

- FXE-001 Format: supported format idを受理し、unknown versionを拒否する。
- FXE-002 Unit: SR-001 width=0.20m / depth=0.10mを保持する。
- FXE-003 Body Mesh: SR-001が147 vertices / 240 trianglesを持つ。
- FXE-004 SurfaceReference: DomainId=1とnormalized U/Vを保持し、runtime triangle IDを必要としない。
- FXE-005 CP Identity: stable_id 0..146がunique/contiguousである。
- FXE-006 Strip Topology: 7 strips x 21 ordered CPを復元できる。
- FXE-007 Anchor/Contact: SR-001 C0/G0でAnchor=14、Contact input=147を保持する。
- FXE-008 SR-003: baked Convex-Side geometryとAnchor=7を保持する。
- FXE-009 Case Inputs: World Gravity / Conformity / CollisionToleranceをround-tripする。
- FXE-010 Rust Round-trip: export -> importでvalidation semantic inputが一致する。
- FXE-011 Unity Import: SR-001 imported fixtureがBF-007 geometry/countと一致する。
- FXE-012 Unity SR-001 Boundary: imported SR-001 C0/G0がBF-005/006の147/14/147 semantic countsを再現する。
- FXE-013 Invalid Data: malformed domain/UV/index/id/normal/scalarを拒否する。

FXE-001～010をRust側実装の最低テスト、FXE-011～013のUnity該当項目をUnity integration validationとする。

## 11. Implementation Order

1. 本仕様を確定。
2. Rust側にValidation-only exchange model + JSON exporter/importerを追加。
3. FXE-001～010を実装しRust quality gateを通す。
4. `SR-001-C0-G0` JSONをReferenceから生成。
5. Unity PC importer probeを追加。
6. FXE-011/012/013をUnityで確認。
7. imported SR-001を使用して実Solver end-to-endへ進む。
8. SR-002～005 / 30 casesへ拡張する。

## 12. 未確定事項

- JSON parser/library選定は実装時にRust workspace依存関係を確認して決める。
- Validation exchange v0を将来Product serializationへ流用することは決定しない。
- Body Surfaceのmultiple domain / seam / overlapはBasic v1の対象外。
- Skinned/deformed mesh stable SurfaceReference lifecycleは本形式では解決しない。
- O3DE検証前にBackend-neutral contractへ昇格しない。


## 13. Implementation Status

2026-10-01:

- Rust validation exchange model: implemented in `sansacloth-fixture::exchange`.
- JSON serialization/deserialization: implemented with Serde / serde_json.
- Basic resolved fixture generator: SR-001～005 supported.
- Basic case selector: Gravity Off/On × Conformity 0/0.5/1 supported.
- SR-003 rigid transform: baked into exported Body/Cloth coordinates.
- FXE-001～010: Rust tests implemented.
- FXE-013 invalid semantic data rejection: Rust test implemented in advance of Unity importer validation.
- SR-001-C0-G0 exporter example: implemented.
- Cargo.lock update and local Rust quality gate: **OPEN**.
- Generated SR-001-C0-G0 JSON verification: **OPEN**.
- Unity FXE-011/012/013: **OPEN**.

Rust exporter:

```powershell
cd reference
cargo run -p sansacloth-fixture --example export_fixture_exchange
```

Default output:

`reference/validation/fixture-exchange/SR-001-C0-G0.json`

Expected exporter summary:

```text
SANSA_FXE|CASE_ID|SR-001-C0-G0
SANSA_FXE|BODY_VERTEX_COUNT|147
SANSA_FXE|BODY_TRIANGLE_COUNT|240
SANSA_FXE|CP_COUNT|147
SANSA_FXE|ANCHOR_COUNT|14
SANSA_FXE|CONTACT_INPUT_COUNT|147
SANSA_FXE|OUTPUT|validation\fixture-exchange\SR-001-C0-G0.json
```

OPEN項目はローカルCargo実行結果を確認してからPASSへ変更する。依存追加に伴う`Cargo.lock`はCargoで生成し、手編集しない。
