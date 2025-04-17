CC = gcc
CFLAGS = -w -g
OBJFILES = bitmap.o list.o hash.o hex_dump.o main.o
TARGET = testlib

all: $(TARGET)

$(TARGET) : $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES) $(TARGET)

.PHONY: all clean 
