# SansaCloth

SansaCloth is an **SDF-based anti-clipping cloth shader foundation** for **Unity** and **O3DE (Atom)**.  
Prevents cloth–body interpenetration at render time and supports **PC and Android** with a **mobile-lite (vertex-only) mode**.

> **VRChat note**
> - **Worlds (PC/Quest)**: custom shaders OK → use SansaCloth (mobile-lite on Quest)
> - **Avatars (Quest)**: custom shaders NG → use **offline baking** (body auto-hide, clearance/thickness bake, tightness mask) + official Mobile shaders.

## Planned Layout

/core
/engines
/unity/{pc,android,tools}/
/o3de/{pc,android}/
/samples/{worlds-mobile,avatars-quest}/

> **VRChat note**
> - **Worlds (PC/Quest)**: custom shaders OK → SansaCloth（Questはモバイルライト版）
> - **Avatars (Quest)**: custom shaders NG → **オフライン焼き**で回避（ボディ自動ハイド、クリアランス厚み焼き、Tightnessマスク）＋公式 Mobile シェーダ使用
