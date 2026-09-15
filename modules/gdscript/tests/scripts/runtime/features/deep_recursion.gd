# Guards the VM's stack frame size. Every script call recurses into `GDScriptFunction::call()`, so its
# frame bounds the reachable call depth. If that function grows past what MSVC optimizes, the frame
# balloons and this depth crashes the process long before `MAX_CALL_DEPTH`.
func recurse(n: int) -> int:
	if n == 0:
		return 0
	return recurse(n - 1) + 1

func test():
	print(recurse(1000))
