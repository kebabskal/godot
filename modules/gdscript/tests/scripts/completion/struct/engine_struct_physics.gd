extends Node

func _ready():
	var ss := PhysicsServer3D.space_get_direct_state(RID())
	var r := ss.intersect_ray_struct(null)
	r.➡
