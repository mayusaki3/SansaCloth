# Runtime Simulation LOD / Runtime シミュレーション LOD

> Draft specification for SansaCloth Runtime Backends.

## 1. Purpose / 目的

複数 Avatar が同時に SansaCloth を利用する環境で、SurfaceResponse の意味を維持しながら Runtime 演算負荷を制御する。

Simulation LOD / シミュレーション LOD は **Runtime Backend の実行品質制御**であり、Core の Cloth/Material 特性や Reference Solver の正解基準を変更しない。

## 2. Separation of Responsibilities / 責務分離

```text
SansaCloth Core
    |
    v
SurfaceResponse semantics
    |
    +--> Reference Solver
    |       Full-quality correctness baseline
    |
    +--> Runtime Backend
            SimulationBudget
            SimulationQualityProfile
            Simulation LOD selection
            Update Frequency
            Backend-specific approximation
```

- Reference Solver は Simulation LOD を適用しない。
- Core の `Conformity / 表面追従性`、`Tightness / 締め付け` 等の意味は LOD により変更しない。
- Runtime Backend は同じ意味論を、品質段階に応じて近似してよい。
- Bake Only では Runtime SurfaceResponse を停止してよい。

## 3. SimulationQualityProfile / シミュレーション品質プロファイル

初期候補を以下とする。名称と境界値は Runtime 検証後に確定する。

| Profile | 主用途 | SurfaceResponse | Control Point | Update Frequency |
|---|---|---|---|---|
| Full | Local Avatar / 至近距離 | Full | Full | High |
| Reduced | 近～中距離 | Reduced approximation | Reduced | Reduced |
| LowFrequency | 遠距離 | Low-frequency approximation | Reduced | Low |
| BakeLight | さらに低優先度 | Bake + minimal runtime correction | Minimal | Very Low |
| BakeOnly | 超遠距離・非重要対象 | Runtime停止 | None | None |

固定のフレーム間隔は現段階では規定しない。Backend、端末性能、描画負荷、Avatar 数に応じて決定する。

## 4. SimulationBudget / シミュレーション予算

Runtime は Scene 全体に SimulationBudget を持てるものとする。

個々の Avatar の品質を独立に最大化するのではなく、Scene 全体の Budget 内で品質を割り当てる。

Budget の単位は現段階では未確定とする。候補:

- Control Point updates per frame
- SurfaceResponse work units
- GPU/CPU time budget
- Backend-specific cost estimate

特定 Backend の時間単位を Core 共通仕様にはしない。

## 5. SimulationPriority / シミュレーション優先度

LOD 選択は Distance / 距離だけで決定しない。

初期候補入力:

- ScreenSize / 画面占有率
- Distance / 距離
- Visibility / 可視状態
- LocalAvatar / ローカル Avatar
- Interaction / インタラクション状態
- Backend capability / Backend 能力
- Current SimulationBudget pressure / 現在の予算逼迫度

概念的には次のような Priority 関数を許容する。

```text
Priority = f(
    ScreenSize,
    Distance,
    Visibility,
    LocalAvatar,
    Interaction,
    BudgetPressure
)
```

具体的な重み、閾値、距離は Backend/Target 検証前には固定しない。

## 6. LOD Transition / LOD 遷移

品質切替による Cloth の位置ジャンプを避ける。

Runtime Backend は以下を考慮する。

- Hysteresis / ヒステリシス
- Transition Blend / 遷移ブレンド
- Update-rate transition / 更新頻度遷移
- State continuity / Cloth State 継続性

遷移時間などの具体値は実測後に決定する。

LOD が変化しても Pairing、SurfaceReference、Cloth State の論理的意味を失ってはならない。

## 7. Multi-Avatar Behavior / 複数 Avatar 時の動作

高負荷時には、低 Priority Avatar から段階的に品質を下げる。

例:

```text
Full
  -> Reduced
  -> LowFrequency
  -> BakeLight
  -> BakeOnly
```

Local Avatar や Interaction 中の Avatar を常に Full にすることは現段階では保証しない。端末が Budget を満たせない場合に備え、すべての Profile に degradation path を持てる設計とする。

## 8. Mobile and XR Relationship / Mobile・XR との関係

Simulation LOD は PC の多数 Avatar 対策だけではなく、Android、iOS、standalone XR 等の性能差にも共通利用する。

Target Profile は利用可能な最大品質を制限できる。

例:

```text
High-end PC:
    Full -> Reduced -> LowFrequency -> BakeLight -> BakeOnly

Mobile / standalone XR:
    Reduced -> LowFrequency -> BakeLight -> BakeOnly
```

上記は概念例であり、Target ごとの正式な上限は Backend/Target Validation 後に決定する。

## 9. Validation Requirements / 検証要求

Runtime Backend 検証では最低限、以下を測定対象とする。

1. Avatar 数増加時の演算コスト
2. Profile ごとの SurfaceResponse 誤差
3. Update Frequency 低下時の視覚差
4. LOD 遷移時の位置ジャンプ
5. Budget 超過時の degradation
6. Local/remote Avatar 混在時の品質配分
7. 画面外 Avatar の処理削減
8. Backend/Target ごとの差

Reference Solver の結果を Full-quality baseline として比較する。

## 10. Unresolved / 未確定事項

- SimulationBudget の共通表現
- Priority 算出式
- Profile ごとの Control Point 解像度
- Update Frequency
- Backend ごとの近似手法
- LOD 遷移時間とヒステリシス
- Visibility 判定方式
- Interaction priority の定義
- Local Avatar の最低保証品質
- Target ごとの最大品質
- Performance threshold
