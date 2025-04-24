CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread -g

PROGS = myshell

all: $(PROGS)

shellex: $(CC) $(CFLAGS)  myshell.c csapp.c

clean:
	rm -rf *~ $(PROGS)

