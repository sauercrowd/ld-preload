preload.so: preload.c
	gcc -shared -fpic preload.c -o preload.so -ldl
