CC=gcc
CFLAGS=-g -Wall -Werror
LIBS=-lpthread
BIN=/usr/local/bin
TARGET=baudrate

$(TARGET):
	$(CC) -DMINICOM_BIN_PATH='"$(shell which minicom)"' $(TARGET).c -o $(TARGET) $(CFLAGS) $(LIBS)

install:
	cp $(TARGET) $(BIN)/$(TARGET)

remove:
	rm -f $(BIN)/$(TARGET)

clean:
	rm -f $(TARGET)
