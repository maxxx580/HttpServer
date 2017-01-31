CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread

web_server: server.c
	${CC} -o web_server server.c util.o ${LDFLAGS} -g

test_server: web_server
	./web_server 9000 /home/maxxx580/Desktop/csci4061/csci4061-project4/testing 10 10 50 10

clean:
	rm web_server
