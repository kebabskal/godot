# Engine-declared layouts are usable like script structs: as types, constructors and values.
func describe(r: PhysicsRayResult3D) -> String:
	return "%s %s %s" % [r.hit, r.position, r.face_index]

func test():
	var r := PhysicsRayResult3D()
	print(describe(r))
	r.position = Vector3(1, 2, 3)
	r.hit = true
	print(describe(r))

	var r2: PhysicsRayResult3D = PhysicsRayResult3D(true, Vector3(4, 5, 6), Vector3.UP)
	print(r2.normal, " ", r2.collider == null, " ", r2.rid)
	print(PhysicsRayResult2D().normal)
	print(typeof(r2) == TYPE_STRUCT, " ", r2 == PhysicsRayResult3D(true, Vector3(4, 5, 6), Vector3.UP))
