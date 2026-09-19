enum Dir:
	LEFT
	RIGHT

	func opposite() -> Dir:
		return RIGHT if self == LEFT else LEFT
