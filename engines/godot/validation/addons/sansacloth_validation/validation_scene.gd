extends Node3D

# Editor/visual smoke test only; no SansaCloth solver is connected.
@export var movement_speed_m_per_sec: float = 0.5
@export var initial_cloth_offset_m: float = 0.0

@onready var cloth: MeshInstance3D = $ClothPlaceholder
var _start_position: Vector3

func _ready() -> void:
    _start_position = cloth.position
    cloth.position.y += initial_cloth_offset_m
    print("SANSA_GODOT|SCENE|READY|SOLVER_NOT_CONNECTED")

func _process(delta: float) -> void:
    var movement := Input.get_axis("ui_down", "ui_up")
    cloth.position.y += movement * movement_speed_m_per_sec * delta
    if Input.is_action_just_pressed("ui_accept"):
        cloth.position = _start_position + Vector3.UP * initial_cloth_offset_m
