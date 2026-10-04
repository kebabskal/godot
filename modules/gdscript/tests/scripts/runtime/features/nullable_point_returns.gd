# Engine methods that return a point or null are typed as nullable points, so strict code can
# use them without casts.

func hit_plane(origin: Vector3) -> Vector3?:
	return Plane.PLANE_XZ.intersects_ray(origin, Vector3.DOWN)

func test():
	var plane := Plane.PLANE_XZ
	var result := plane.intersects_ray(Vector3(1, 5, 2), Vector3.DOWN) # Vector3?
	if result != null:
		var point: Vector3 = result # Narrowed: no cast.
		print(point)
	print(plane.intersects_ray(Vector3(1, 5, 2), Vector3.UP)) # Away from the plane: null.

	# `if var` on a nullable point tests for null, not truthiness: a hit at the origin is a hit.
	if var hit := plane.intersects_ray(Vector3(0, 5, 0), Vector3.DOWN):
		print("hit at ", hit)
	else:
		print("missed")
	if var hit := plane.intersects_ray(Vector3(0, 5, 0), Vector3.UP):
		print("unexpected ", hit)
	else:
		print("missed")
	# A non-nullable value keeps testing its truthiness, as before.
	if var n := 0:
		print("unexpected ", n)
	else:
		print("zero is false")

	var box := AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2))
	print(box.intersects_segment(Vector3(0, 5, 0), Vector3(0, -5, 0)))
	print(Geometry2D.segment_intersects_segment(Vector2(0, 0), Vector2(2, 2), Vector2(0, 2), Vector2(2, 0)))
	print(Geometry3D.ray_intersects_triangle(Vector3(0.2, 1, 0.2), Vector3.DOWN, Vector3.ZERO, Vector3(1, 0, 0), Vector3(0, 0, 1)))
	print(hit_plane(Vector3(3, 1, 3)))
