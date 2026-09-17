# Narrowing is what makes `T?` usable: a test that can only pass for a non-null value
# makes the value non-null for as long as that result holds. Everything below is
# warning-free except where marked, and the warnings are the point of the test.

class Item:
	var name := "sword"
	var hp := 3
	func describe() -> String:
		return name

func unchecked(item: Item?) -> String:
	return item.name # UNSAFE_NULLABLE_ACCESS: nothing ruled out null.

func unchecked_call(item: Item?) -> String:
	return item.describe() # UNSAFE_NULLABLE_ACCESS

func checked(item: Item?) -> String:
	if item != null:
		return item.name
	return "(none)"

func checked_reversed(item: Item?) -> String:
	if null != item:
		return item.name
	return "(none)"

func guard_clause(item: Item?) -> String:
	if item == null:
		return "(none)"
	return item.name

func negated_guard(item: Item?) -> String:
	if not item:
		return "(none)"
	return item.name

func truthy(item: Item?) -> String:
	if item:
		return item.name
	return "(none)"

func else_branch(item: Item?) -> String:
	if item == null:
		return "(none)"
	else:
		return item.name

func chained_and(item: Item?) -> String:
	if item != null and item.hp > 0:
		return item.name
	return "(none)"

func after_or(item: Item?) -> String:
	if item == null or item.hp == 0:
		return "(none)"
	return item.name

func reassigned(item: Item?, other: Item?) -> String:
	if item != null:
		item = other
		return item.name # UNSAFE_NULLABLE_ACCESS: the new value proves nothing.
	return "(none)"

func only_in_branch(item: Item?) -> String:
	if item != null:
		pass
	return item.name # UNSAFE_NULLABLE_ACCESS: the branch ended, and it did not return.

func guarded_navigation(item: Item?) -> String:
	return item?.name ?? "(none)"

func test():
	print(unchecked(Item.new()))
	print(unchecked_call(Item.new()))
	print(checked(null))
	print(checked_reversed(null))
	print(guard_clause(null))
	print(negated_guard(null))
	print(truthy(null))
	print(else_branch(null))
	print(chained_and(null))
	print(after_or(null))
	print(reassigned(Item.new(), Item.new()))
	print(only_in_branch(Item.new()))
	print(guarded_navigation(null))
