func total[T: int | float | Vector2](items: Array[T]) -> T:
	var sum: T = items[0]
	for i in range(1, items.size()):
		sum = sum + items[i]
	return sum

func lerped[T: Vector2 | Vector3](a: T, b: T, weight: float) -> T:
	# Mixing `T` with a `float` still lands on `T`, because it does for every member.
	return a + (b - a) * weight

func same[T: int | float | Vector2](a: T, b: T) -> bool:
	return a == b

func test():
	var ints: Array[int] = [1, 2, 3]
	var floats: Array[float] = [1.5, 2.5]
	var vectors: Array[Vector2] = [Vector2(1, 1), Vector2(2, 3)]

	print(total(ints))
	print(total(floats))
	print(total(vectors))
	print(lerped(Vector2.ZERO, Vector2(10, 0), 0.5))
	print(same(1, 1), " ", same(Vector2.ONE, Vector2.ZERO))
