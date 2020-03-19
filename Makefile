LDFLAGS=	# -lnsl -lsocket

all: server

server: myserver.c
	gcc -o myserver myserver.c $(LDFLAGS)

clean:
	-rm -fr talk_client talk_server