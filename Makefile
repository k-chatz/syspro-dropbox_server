OBJS	= main.o list.o options.o connection.o handler.o client.o
SOURCE	= main.c list.c options.c connection.c handler.c client.c
HEADER	= list.h client.h connection.h handler.h options.h
OUT	= dropbox_server
CC	 = gcc
FLAGS	 = -c -Wall

all: $(OBJS)
	$(CC) $(OBJS) -o $(OUT)

main.o: main.c
	$(CC) $(FLAGS) main.c

list.o: list.c
	$(CC) $(FLAGS) list.c

options.o: options.c
	$(CC) $(FLAGS) options.c

connection.o: connection.c
	$(CC) $(FLAGS) connection.c

handler.o: handler.c
	$(CC) $(FLAGS) handler.c

client.o: client.c
	$(CC) $(FLAGS) client.c

clean:
	rm -f $(OBJS) $(OUT)
