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

	# The other query result layouts, and a typed array of an engine struct.
	var rest := PhysicsRestInfo3D()
	print(rest.hit, " ", rest.linear_velocity)
	print(PhysicsCastResult2D().safe_fraction, " ", PhysicsCastResult2D(0.5, 0.75).unsafe_fraction)
	var hits: Array[PhysicsShapeResult3D] = [PhysicsShapeResult3D(), PhysicsShapeResult3D(RID(), 7)]
	print(hits.size(), " ", hits[1].collider_id, " ", hits[0].collider == null)

	# Core layouts: `Time` returns structs and takes them as parameters.
	var dt := Time.get_datetime_from_unix_time(0)
	print(dt)
	print(Time.get_unix_time_from_datetime(DateTime(2000, 1, 1)))
	print(Time.get_datetime_string_from_datetime(DateTime(2000, 1, 1, 6, 12, 30), true))
	var parsed: DateTime = Time.get_datetime_from_datetime_string("2001-02-03T04:05:06")
	print(parsed.weekday, " ", parsed.second)
	print(Engine.get_version_info_struct().major >= 4, " ", typeof(OS.get_memory_info_struct().physical) == TYPE_INT)
