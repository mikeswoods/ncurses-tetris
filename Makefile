CC=gcc
CFLAGS=-Wall
LDFLAGS=-lncurses
TARGET=tetris

main:	$(TARGET)

$(TARGET):	$(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)

clean:
	rm -f $(TARGET)
