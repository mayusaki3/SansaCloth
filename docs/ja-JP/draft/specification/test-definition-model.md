# Test Definition Model / テスト定義モデル

> SansaCloth > docs > ja-JP > draft > specification > Test Definition Model

## 1. 目的

Reference、Bake、Unity、O3DE、Godot等で同一の検証意図を再利用できる、エンジン非依存の論理Test Definitionモデルを定義する。

Serialization形式やRust構造体は本書では固定しない。

## 2. 実行モデル

```text
TestDefinition
      +
ExecutionProfile
      ↓
ValidationRun
      ↓
ValidationResult
```

TestDefinitionは「何を検証するか」、ExecutionProfileは「どのTargetでどう実行するか」を表す。

## 3. TestDefinition

論理要素:

```text
TestDefinition
├─ Identity
├─ Requirements
├─ Environment
├─ Body
├─ Cloth
├─ Properties
├─ InitialState
├─ Operations
├─ Cases
├─ Assertions
└─ Measurements
```

Identityは少なくともTestID、Name、Version、Category、Purposeを持つ。

Test Version変更時は過去Resultと識別可能でなければならない。

## 4. Requirements / 必要機能

RequiredCapabilitiesを機械判定可能にする。

未対応Capabilityに依存するTestは、実装不良を意味するFAILではなくUNSUPPORTEDとして扱える。

## 5. Environment / 環境

内部Canonical Length Unitはm。

座標規約:
- +X Right / 右
- +Y Up / 上
- +Z Forward / 前

GravityはWorld/Environment vector [m/s²] として明示する。Solverで-Yをハードコードしない。

## 6. StateとDerived Result

InitialStateにはPlacement、SurfaceReference、Anchor、Pairing等を指定できる。

Contact、Separation等、Solverが導出すべき値を入力の正解値として固定しない。

## 7. Operations / 操作

初期BasicではSolveのみ。

将来:
- SetPose
- MovePlacement
- ExternalPull
- Solve
- Reset
等のStateful sequenceへ拡張できる。

## 8. ExecutionProfile

例:

```text
ProfileID
Target
SolverResolution
DeterministicMode
DebugStageOutput
PerformanceMeasurement
VisualCapture
ToleranceProfile
```

Solver resolution、Backend、Capture有無等をTestDefinitionから分離する。

Visual Capture I/OはSolver Performance測定から除外する。

## 9. ValidationResult

```text
ValidationResult
├─ TestIdentity
├─ ExecutionIdentity
├─ InputIdentity
├─ CapabilityResult
├─ AssertionResults
├─ Measurements
├─ SurfaceResult
├─ StageResults
├─ Performance
├─ Diagnostics
└─ Artifacts
```

Control Point result候補:
- Position [m]
- Normal
- SurfaceReference
- SurfaceOffset [m]
- Contact
- SeparationDistance [m]

## 10. Status

Execution Status:
- PASS
- FAIL
- UNSUPPORTED
- SKIPPED
- ERROR

Diagnostic Severity:
- INFO
- WARNING
- ERROR

WARNINGをExecution Statusとして使用しない。

Visual、Performance等は必要に応じて別軸で表す。例:
- Functional: PASS
- Visual: NOT_REVIEWED / REVIEWED
- Performance: MEASURED

## 11. Assertions / Measurements / Visual Validation

### Automated Assertions
機械的なPASS/FAIL条件。

### Numerical Measurements
値を記録するが、十分な根拠がない段階では閾値を設定しない。

### Visual Validation
幾何形状、吸着、不連続、Surface品質等を人間が確認する。

測定可能だからという理由だけで恣意的なFAIL閾値を設定しない。

## 12. Comparison

Baseline/Candidate比較候補:
- Position Mean/RMS/Max Error
- Normal Error
- Separation Error
- Contact Difference
- Performance Difference

Reference implementationとEngine Backendは内部アルゴリズム一致を要求せず、意味論と結果の互換性を検証する。

## 13. TestDefinitionと製品データ

TestDefinitionはSansaCloth製品Authoringデータそのものではない。

将来の `.sansacloth` formatとValidation Test Definition serializationは分離して設計する。

## 14. 未確定事項

- Serialization形式
- Rust API/型
- ToleranceProfile構造
- InputIdentity hash/version方式
- Artifact schema
- Visual status詳細
- Backend間比較Tolerance

> SansaCloth > docs > ja-JP > draft > specification > Test Definition Model
