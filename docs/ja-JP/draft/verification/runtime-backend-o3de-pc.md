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

Body rotationとWorld Gravityを独立に設定・観測し、Body rotationがGravity vectorを暗黙回転させない境界を確認する。

Physics既定Gravity値そのものをReference既定値と一致させることは要求しない。

## 6. Phase判定

O3DE BF-001 Unit Mapping:
- OBF-001 PASSでPASS候補。

O3DE BF-002 Coordinate Mapping:
- OBF-002～006をすべてruntime確認してからPASSとする。
- Python Binding不足による未観測項目はC++ Probeへ移し、推測でPASSにしない。

初期状態:
- OBF-001: IMPLEMENTED / RUNTIME OPEN
- OBF-002: IMPLEMENTED / RUNTIME OPEN
- OBF-003: IMPLEMENTED / RUNTIME OPEN
- OBF-004: IMPLEMENTED / RUNTIME OPEN
- OBF-005: OPEN
- OBF-006: OPEN
- BF-001: OPEN
- BF-002: OPEN

## 7. Probe配布方針

Unity検証と同様、O3DE検証Project自体をSansaCloth repositoryへ含めることは要求しない。

Canonical source:
- engines/o3de/pc/probe/

Deploy target:
- <O3DE project>/Editor/Scripts/SansaCloth/

初期ProbeはEditor Python Bindingsを利用する。検証ProjectではPython Editor Bindings Gemを有効化する。

Probe sourceとdeploy scriptはSansaCloth repositoryをsource of truthとし、検証Project側は再生成可能な作業環境として扱う。

## 8. 後続順序

1. OBF-001～004 Python Probe runtime確認。
2. 必要ならC++ Probeを追加しOBF-004/005を確認。
3. OBF-006 World Gravityを確認。
4. O3DE BF-001/BF-002を判定。
5. BF-003 SurfaceReference Mappingへ進む。
6. BF-004 SurfaceQuery。
7. BF-005/006 Input/Output Semantics。
8. BF-007 Basic Fixture Mapping。
9. BF-008 Validation Capture。
10. Fixture Exchange 30-case import/SurfaceResponse比較へ進む。
11. Unity/O3DE双方で成立した境界だけをBackend-neutral contract候補として整理する。

## 9. 未確定事項

- O3DE runtime handednessの実測結果。
- canonical rotationからO3DE rotationへのaxis/sign変換。
- Python BindingでCross productを直接観測できるか。
- O3DE mesh runtime APIでのtriangle winding/geometric normal取得経路。
- Physics scene Gravityの取得/設定API。
- stable SurfaceReference mapping。
- runtime/deformed mesh access。
- Validation Captureの最終出力経路。

> SansaCloth > docs > ja-JP > draft > verification > O3DEPC.RuntimeBackend
