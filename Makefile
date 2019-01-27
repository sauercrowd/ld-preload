preload.so: preload.c
	gcc -g -shared -fpic preload.c -o preload.so -ldl
