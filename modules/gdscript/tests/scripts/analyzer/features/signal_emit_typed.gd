signal changed(items: Array[String])

func test():
	# The handler's parameter is `Array[String]` from the signal, and the nested
	# lambda's is `String` from that, so both short forms keep their types. `emit`
	# builds a real `Array[String]` from the literal, which is what the handler
	# accepts at runtime.
	changed.connect(a => print(a.filter(b => b.begins_with("k"))))
	changed.emit(["kebab", "pizza"])
