CC = gcc
CFLAGS = -Wall -g

TARGET = bank

OBJS = bank.o string_parser.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -lpthread

part1.o: part1.c account.h string_parser.h
	$(CC) $(CFLAGS) -c bank.c

string_parser.o: string_parser.c string_parser.h
	$(CC) $(CFLAGS) -c string_parser.c

clean:
	rm -f $(TARGET) $(OBJS)