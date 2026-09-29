# Reference Surface Solver v1 / 参照表面ソルバー v1

> SansaCloth > docs > ja-JP > draft > specification > Reference Surface Solver

## 1. 目的

本書は、SansaCloth の意味論を検証する PC Reference 用 `ReferenceSurfaceSolver / 参照表面ソルバー` の v1 設計を定義する。

本Solverは Core 仕様そのものではない。Unity、O3DE、Godot 等のBackendは、同一の意味論と許容誤差内の結果を満たす限り、異なるアルゴリズムを使用してよい。

v1 は Stateful Quasi-static / 状態保持型準静的 Solver とし、時間積分を行う Dynamic Simulation ではない。

## 2. Stage

通常の外部APIは次を想定する。

```text
ReferenceSurfaceSolver.solve(input) -> SurfaceResponseResult
```

検証時のみ各Stageを観測できるAPIを許可する。

```text
Initial
  -> Support Resolution / 支持判定
  -> Bridge / 架橋
  -> Gravity Response / 重力応答
  -> Conformity Response / 表面追従応答
  -> Collision Response / 衝突応答
  -> Final
```

v1 は 1-pass とする。反復Solver化は SR-001～005 の測定結果から必要性を判断する。

## 3. Input / Output

主入力:
- Body Surface / 身体表面
- Cloth Placement / 布配置
- Surface Reference / 表面参照
- Anchor / 固定点
- Environment Gravity / 環境重力
- Conformity / 表面追従性
- ReferenceSolverSettings

主出力:
- Position / 位置 [m]
- Normal / 法線
- SurfaceReference / 表面参照
- SurfaceOffset / 表面相対変位 [m]
- Contact / 接触
- SeparationDistance / 表面離隔距離 [m]
- Stretch / 伸び
- Compression / 圧縮
- ResidualDemand / 残留要求
- BodyResponse / 身体応答

初期v1では未実装の出力があってよいが、未実装値を実値として偽装しない。

## 3.1 Integrated API Boundary / 統合API境界

通常利用者は個別Stageを直接連結せず、`ReferenceSurfaceSolver.solve(input)` を使用する。

`solve_with_debug(input)` は検証専用とし、次のStage snapshotを返す。

- Initial
- Support
- Bridge
- GravityResponse
- ConformityResponse
- CollisionResponse
- Final

`SurfaceResponseResult` は最終的な共通結果を表し、BakeやValidationは個別Stage実装ではなくこの結果/将来の共通IRへ依存する。

### Surface Query / 表面問い合わせ

Conformity と Collision に必要な `SeparationDistance`、`Desired Surface Position`、`Surface Normal` は、各Stageで位置が変化した後にBody Surfaceに対して再評価しなければならない。

したがって統合Solverは、初期入力で固定されたSeparation配列を全Stageで使い回してはならない。

概念境界:

```text
Bridge positions
    -> SurfaceQuery
    -> GravityResponse
    -> SurfaceQuery
    -> ConformityResponse
    -> SurfaceQuery
    -> CollisionResponse
    -> SurfaceQuery
    -> SurfaceResponseResult
```

SurfaceQueryの具体的なtrait/API、analytic fixture adapter、mesh/runtime実装は次の実装ステップで確定する。

この境界が確定するまで、個別Stage関数を機械的に連結した `solve()` を「統合Solver完成」と扱わない。

## 4. Support Resolution / 支持判定

- Anchor は強い Support とする。
- Contact は自動的に全自由度を固定する Support ではない。
- Separated な点は原則として直接Supportではない。
- Support はSolver内部の派生情報であり、永続Cloth Stateとはしない。
- Pairing / SurfaceReference は Separation があっても維持できる。

## 5. Bridge / 架橋

Bridge はSupport間の連続した基準形状を生成する。

v1:
- Support位置を保存する。
- Body Surfaceへ自動追従しない。
- Concave Surfaceへ自動吸着しない。
- 初期実装では線形補間等の単純な決定的手法を許容する。

## 6. Gravity Response / 重力応答

Environment Gravity は World Space のベクトル [m/s²] として入力する。Solverは `-Y` を重力方向としてハードコードしてはならない。

Gravity=0 の場合、Gravity Response はIdentityでなければならない。

v1 は時間積分を行わないため、重力加速度を直接変位へ変換しない。Reference v1では決定的な準静的幾何応答を使用する。

概念式:

```text
DeltaP = normalize(Gravity)
       * CharacteristicLength
       * QuasiStaticGravityScale
       * SupportWeight
```

この式は Core 仕様ではなく Reference v1 の近似である。

Anchor の Gravity displacement は0とする。Gravity StageはCollision補正やConformity処理を行わない。

## 7. Conformity Response / 表面追従応答

Conformity は「Body Surfaceの局所形状へClothがどの程度追従しようとするか」を表す。

```text
Conformity = 0   : 積極的な追従なし
Conformity = 1   : 最大限追従を試みる
```

ただし以下を保証する。

```text
Conformity = 1
!= Adhesion / 接着
!= Contact強制
!= SeparationDistance = 0 強制
```

Body Surfaceへの単純な完全Lerpは禁止する。

Reference v1では概念的に:

```text
FollowEligibility
  = DistanceWeight
  * SupportInfluence
  * OrientationWeight

EffectiveConformity
  = Conformity * FollowEligibility
```

を用いる。

`ConformityReach / 表面追従範囲` 外のUnsupported CPをConformityだけでBodyへ引き寄せてはならない。

Body Surfaceの細かな形状を無条件にコピーせず、`ConformityFilterRadius / 表面追従フィルター半径` を用いた局所的なDesired Cloth SurfaceをReference v1で構成できる。

Conformity StageはAnchor、Gravity入力、Slipを変更せず、最終Collision解決も行わない。

## 8. Collision Response / 衝突応答

Collision Response は最終的な不正侵入を解消するStageである。

- Gravity/Conformity Stageで一時的な侵入が存在してもよい。
- Rigid Referenceでは最終 penetration <= CollisionTolerance を要求する。
- 将来のSoftness導入時はRigid Collision AssertionとSoft Body Responseを分離する。

## 9. ReferenceSolverSettings

Reference v1 固有の近似設定:

| Formal identifier | 日本語表示名 | Unit | Status |
|---|---|---:|---|
| QuasiStaticGravityScale | 準静的重力スケール | dimensionless | TBD |
| ConformityReach | 表面追従範囲 | m | TBD |
| ConformityFilterRadius | 表面追従フィルター半径 | m | TBD |
| CollisionTolerance | 衝突許容誤差 | m | Reference v1 implemented; value TBD |

これらは現時点で SansaCloth Core Property ではない。実測前に根拠なく数値を固定しない。

## 10. Invariants / 不変条件

- INV-01 Deterministic / 決定的
- INV-02 Finite / 有限値
- INV-03 Unit Consistency / 単位整合性
- INV-04 Coordinate Independence / 座標独立性
- INV-05 Anchor Preservation / 固定点維持
- INV-06 No Conformity Adhesion / 表面追従による不正吸着なし
- INV-07 Collision Safety / 衝突安全性
- INV-08 Continuity / 連続性
- INV-09 Resolution Convergence / 解像度収束
- INV-10 Surface Generality / 表面一般性

Core/Solverに Heel、Breast 等の解剖学固有分岐を導入しない。

## 11. 未確定事項

- SupportInfluenceの厳密式
- Bridgeの正式アルゴリズム
- QuasiStaticGravityScale
- ConformityReach
- ConformityFilterRadius
- CollisionTolerance等の数値
- SurfaceQuery trait/APIとBody Surface adapter
- Gravityの物理的MagnitudeをSurfaceDensity等へ接続するモデル
- Stretchability/Tightness/BendingResponseをConformityへ統合する方法
- 反復Solverの必要性と収束条件

> SansaCloth > docs > ja-JP > draft > specification > Reference Surface Solver
