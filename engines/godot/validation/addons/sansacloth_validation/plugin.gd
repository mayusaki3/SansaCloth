@tool
extends EditorPlugin

const SCENE_PATH = "res://validation/sansacloth_validation.tscn"

func _enter_tree():
    add_tool_menu_item("SansaCloth: Setup Validation Scene", _setup_scene)

func _exit_tree():
    remove_tool_menu_item("SansaCloth: Setup Validation Scene")

func _setup_scene():
    print("SANSA_GODOT|SETUP|START")
