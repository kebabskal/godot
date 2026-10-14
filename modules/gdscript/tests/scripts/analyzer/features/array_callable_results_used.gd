# Built-in container methods that use what the callable returns accept anything that returns
# something; only a callable that returns nothing is rejected. `map()` uses the result as a value,
# so a lambda that returns nothing is fine there (it maps to `null`).
func is_short(s: String) -> bool:
	return s.length() < 3

func test():
	var names: Array[String] = ["bb", "a", "cccc"]
	var plain := ["bb", "a", "cccc"]
	print(names.any(n => n.length() > 3))
	print(names.all(func(n) -> bool: return n.length() > 0))
	print(names.filter(is_short))
	print(plain.filter(n => n.length() < 3))
	print(plain.any(func(n): return n == "a"))
	var sorted := names.duplicate()
	sorted.sort_custom((a, b) => a < b)
	print(sorted)
	print(names.map(n => print(n)))
