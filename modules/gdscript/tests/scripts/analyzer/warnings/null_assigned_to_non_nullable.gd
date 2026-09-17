# Only `null` itself and a type written with `?` count as "may be null" here. A plain
# object type makes no promise to break outside strict mode, so ordinary code stays quiet.

class Item:
	var name: String = "sword"

func takes_item(item: Item) -> String:
	return item.name

func takes_nullable(item: Item?) -> String:
	return item?.name ?? "(none)"

func returns_item() -> Item:
	return null # NULL_ASSIGNED_TO_NON_NULLABLE

func returns_nullable() -> Item?:
	return null

func test():
	var plain: Item = Item.new()
	var maybe: Item? = Item.new()

	var from_null: Item = null # NULL_ASSIGNED_TO_NON_NULLABLE
	var from_nullable: Item = maybe # NULL_ASSIGNED_TO_NON_NULLABLE
	var kept: Item? = maybe
	var widened: Item? = plain

	plain = maybe # NULL_ASSIGNED_TO_NON_NULLABLE

	# The declared type is what counts, so this is still flagged even though the value
	# right here happens not to be null.
	print(takes_item(maybe)) # NULL_ASSIGNED_TO_NON_NULLABLE
	print(takes_item(plain))
	print(takes_nullable(maybe))
	print(takes_nullable(plain))

	# Narrowing clears it: this is the supported way to pass a `T?` on.
	if maybe != null:
		print(takes_item(maybe))

	# Two UNSAFE_NULLABLE_ACCESS here: `kept` and `widened` were declared `Item?`.
	print(from_null, " ", from_nullable.name, " ", kept.name, " ", widened.name)
	print(returns_item(), " ", returns_nullable())

	maybe = null
	plain = null # NULL_ASSIGNED_TO_NON_NULLABLE
	print(maybe, " ", plain)
