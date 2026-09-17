extends Node

class Item:
	var item_name := "sword"
	func describe() -> String:
		return item_name

func use(item: Item?):
	item?.des➡
