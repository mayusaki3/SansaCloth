# SansaCloth Godot Validation (bootstrap)

This validation scaffold includes **verified GSR-007K / GSR-007L analytic fixture numerical parity**. Editor, interactive scene and real mesh integration remain unverified.

## Setup

1. Create a Godot 4 project (or use a disposable existing one).
2. Copy the `addons/sansacloth_validation` directory into the Godot project's `res://addons/`.
3. In **Project > Project Settings > Plugins**, enable **SansaCloth Validation**.
4. In **Project > Tools**, select **SansaCloth: Setup Validation Scene**.
5. Open `res://validation/sansacloth_validation.tscn` and use **Run Current Scene (F6)**.
6. Use Up/Down to move the cloth placeholder, Enter to reset it.

The setup is non-destructive: it does not overwrite an existing scene. Generated scenes stay available after running. The scene uses Godot's native Y-up coordinates and metres.

## Validation status

- EditorPlugin load: **UNVERIFIED** (requires Godot Editor execution).
- Scene generation: **UNVERIFIED**.
- Interactive scene: **UNVERIFIED**.
- SansaCloth solver execution: **analytic fixture computation verified for GSR-007K / GSR-007L; production engine integration not verified**.
- Reference fixture parity: **PASS for GSR-007K and GSR-007L analytic fixtures (Godot 4.7.2, Windows headless)**.
- Real mesh integration: **NOT IMPLEMENTED**.

Do not treat the placeholder geometry, input movement, or a successful scene launch as evidence that the SansaCloth solver passes.

## Next implementation gate

GSR-007K / GSR-007L analytic fixture numerical parity is verified. Next, validate Godot EditorPlugin loading, scene generation and interactive operation, then proceed to real mesh integration. Keep generated runtime output separate from tracked reference fixtures.

## GSR-007L execution

Verified on 2026-10-24 using Godot 4.7.2 stable.

- Cases: 36
- Points: 324
- Contacts: 61
- Maximum position error: 7.50856e-9 m
- Tolerance: 1e-6
- Mismatches: 0
- Exit code: 0

Run from a disposable Godot project containing
the repository's addons/sansacloth_validation.

Test script:
`res://addons/sansacloth_validation/tests/gsr_007l_integrated_parity_test.gd`

Reference snapshot:
`res://addons/sansacloth_validation/reference-results/GSR-007L-integrated.json`

The test independently computes the integrated
solver pipeline and compares its results with
the Rust reference snapshot.

This result does not establish EditorPlugin,
interactive scene, real mesh, or cross-platform
validation.
