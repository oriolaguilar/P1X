all: server

server: myserver.c
	gcc -o myserver myserver.c 
