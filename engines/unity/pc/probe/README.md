# Unity PC Coordinate Probe

Runtime Backend Feasibility CheckpointのBF-001 Unit Mapping / BF-002 Coordinate Mappingを確認する最小Probe。

これはSansaCloth SolverのUnity移植ではない。

## 対象

- UBF-001～004
- CBF-001～008
- Unity PC

## 実行

1. Unity PC用の空Projectを作成する。
2. `SansaClothCoordinateProbe.cs` をProjectの `Assets/` 以下へコピーする。
3. 空GameObjectへ `SansaClothCoordinateProbe` を追加する。
4. Componentのcontext menuから `Run SansaCloth Coordinate Probe` を実行する。
5. Consoleの `SANSA_BF|` で始まる行を保存する。

Scene assetやPrefabをrepositoryへ追加する必要はない。

## 出力

`SANSA_BF|<Probe Key>|<Value>`

Vectorは `x,y,z`、scalarは単一値。小数点はInvariant Cultureで出力する。

## 判定

Reference側の既知basis/rotation/windingと比較する。

特に以下を観測する:
- Unity world unitとcanonical mの1:1運用が可能か
- +X/+Y/+Z basis
- +Z周りの+90/-90 degree rotation
- V00,V01,V11 windingのgeometric normal
- basis cross products
- Body rotationとWorld Gravityの独立性

handednessはProbe結果を確認する前に確定しない。

## 非対象

- SurfaceQuery
- Mesh deformation
- SR-001～005
- LOD
- Performance
- Bake
