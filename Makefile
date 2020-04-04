all: myserver.c
	gcc myserver.c -o myserver -ansi -pedantic -Wall -std=c99 
clean:
	$(RM) myserver