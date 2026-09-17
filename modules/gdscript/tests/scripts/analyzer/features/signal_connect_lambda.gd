signal hit(damage: int, who: String)
signal cleared()

func test():
	# The lambda takes its parameter types from the signal, so the short form stays readable.
	hit.connect((d, w) => print(w, " took ", d * 2))
	cleared.connect(() => print("cleared"))
	hit.emit(5, "goblin")
	cleared.emit()
