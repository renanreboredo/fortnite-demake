CC = g++
CFLAGS = -std=c++11 -g -pthread -lncurses
OUTFILE = bin/fortnite
OBJS = bin/main.o
SRCS = main.cpp

$(OUTFILE): $(OBJS)
	$(CC) $(CFLAGS) -o $(OUTFILE) $(OBJS)
$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -c $(SRCS) -o $(OBJS)

clean:
	rm -rf *.o bin/*

run:
	./bin/fortnite

qrun:
	make && make run
