all: Server Client

Server: utils.c server.c
	gcc -o $@ $^

Client: utils.c client.c
	gcc -o $@ $^

clean:
	-rm Server Client
	-rm *.o
