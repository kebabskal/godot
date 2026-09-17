func test():
	var nums: Array[int] = [1, 2]
	# `n` is an int here, so this is caught instead of failing at runtime.
	print(nums.map(n => n.to_upper()))
