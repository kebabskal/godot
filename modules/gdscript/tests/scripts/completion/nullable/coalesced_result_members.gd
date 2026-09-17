extends Node

class Item:
	var item_name := "sword"

func use(item: Item?):
	(item?.item_name ?? "none").to_up➡
