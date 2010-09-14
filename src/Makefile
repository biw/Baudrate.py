CC=gcc
CFLAGS=-Wall -static
BIN=/usr/bin
TARGET=baudrate

$(TARGET):
	$(CC) $(TARGET).c -o $(TARGET) $(CFLAGS)

install:
	cp $(TARGET) $(BIN)/$(TARGET)

remove:
	rm -f $(BIN)/$(TARGET)

clean:
	rm -f $(TARGET)
