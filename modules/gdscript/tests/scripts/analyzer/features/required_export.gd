# `@required` marks an export with PROPERTY_USAGE_REQUIRED, in either order with `@export`.
@export @required var label: String
@export @required var names: Array[String]
@required @export var data: Resource
@export_node_path("Node3D") @required var target: NodePath
@export var optional: Resource

func test():
	for property in get_property_list():
		if property.usage & PROPERTY_USAGE_SCRIPT_VARIABLE:
			print(property.name, " ", (property.usage & PROPERTY_USAGE_REQUIRED) != 0)
