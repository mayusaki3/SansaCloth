# SansaCloth Godot Validation (bootstrap)

This is a **Godot 4 editor/scene smoke-test scaffold**, not a solver validation result.

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
- SansaCloth solver execution: **NOT IMPLEMENTED**.
- Reference fixture parity: **NOT IMPLEMENTED**.
- Real mesh integration: **NOT IMPLEMENTED**.

Do not treat the placeholder geometry, input movement, or a successful scene launch as evidence that the SansaCloth solver passes.

## Next implementation gate

Inspect the Rust reference fixture and the existing Unity solver probe, then document Godot solver test cases and expected numerical tolerances **before** implementing the Godot solver adapter. Keep generated test output separate from tracked source files.
