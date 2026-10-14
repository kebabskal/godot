func by_name(_a: String, _b: String) -> void:
	pass

func test():
	var names: Array[String] = ["b", "a"]
	# A named function is checked the same way as a lambda.
	names.sort_custom(by_name)
