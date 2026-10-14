@tool
extends EditorPlugin

const SCENE_PATH := "res://validation/sansacloth_validation.tscn"
const SCENE_SCRIPT := preload("res://addons/sansacloth_validation/validation_scene.gd")

func _enter_tree() -> void:
    add_tool_menu_item("SansaCloth: Setup Validation Scene", _setup_scene)
    add_tool_menu_item("SansaCloth: Open Validation Scene", _open_scene)

func _exit_tree() -> void:
    remove_tool_menu_item("SansaCloth: Setup Validation Scene")
    remove_tool_menu_item("SansaCloth: Open Validation Scene")

func _setup_scene() -> void:
    if ResourceLoader.exists(SCENE_PATH):
        print("SANSA_GODOT|SETUP|EXISTS")
        _open_scene()
        return
    DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://validation"))
    var root := Node3D.new()
    root.name = "SansaClothValidation"
    root.set_script(SCENE_SCRIPT)
    var camera := Camera3D.new()
    camera.name = "Camera3D"
    camera.position = Vector3(0.0, 1.6, 3.5)
    camera.rotation_degrees = Vector3(-15.0, 0.0, 0.0)
    camera.current = true
    root.add_child(camera)
    camera.owner = root
    var light := DirectionalLight3D.new()
    light.name = "DirectionalLight3D"
    light.rotation_degrees = Vector3(-45.0, -30.0, 0.0)
    root.add_child(light)
    light.owner = root
    var body := MeshInstance3D.new()
    body.name = "AnalyticBodyPlaceholder"
    body.mesh = SphereMesh.new()
    body.position = Vector3(0.0, 0.7, 0.0)
    root.add_child(body)
    body.owner = root
    var cloth := MeshInstance3D.new()
    cloth.name = "ClothPlaceholder"
    var cloth_mesh := BoxMesh.new()
    cloth_mesh.size = Vector3(1.3, 0.08, 1.3)
    cloth.mesh = cloth_mesh
    cloth.position = Vector3(0.0, 0.7, 0.0)
    root.add_child(cloth)
    cloth.owner = root
    var packed := PackedScene.new()
    var pack_error := packed.pack(root)
    if pack_error != OK:
        push_error("SANSA_GODOT|SETUP|PACK_FAILED|%s" % pack_error)
        root.free()
        return
    var save_error := ResourceSaver.save(packed, SCENE_PATH)
    root.free()
    if save_error != OK:
        push_error("SANSA_GODOT|SETUP|SAVE_FAILED|%s" % save_error)
        return
    print("SANSA_GODOT|SETUP|PASS")
    _open_scene()

func _open_scene() -> void:
    if not ResourceLoader.exists(SCENE_PATH):
        push_warning("Run SansaCloth: Setup Validation Scene first.")
        return
    get_editor_interface().open_scene_from_path(SCENE_PATH)
