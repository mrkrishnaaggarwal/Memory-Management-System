all: clean example

example: example.c mems.h
	gcc -o example example.c -lm

clean:
	rm -rf example