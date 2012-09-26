CC=gcc
CFLAGS=-Wall -static
LIBS=-lpthread
BIN=/usr/bin
TARGET=baudrate

$(TARGET):
	$(CC) $(TARGET).c -o $(TARGET) $(CFLAGS) $(LIBS)

install:
	cp $(TARGET) $(BIN)/$(TARGET)

remove:
	rm -f $(BIN)/$(TARGET)

clean:
	rm -f $(TARGET)
