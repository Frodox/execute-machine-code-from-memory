TARGET  = mem_test
CFLAGS  = -Wall -Wextra --std=gnu99 -fpie -Wno-format
LDFLAGS = -lrt -lselinux
CC = gcc
SOURCES=$(wildcard *.c )
OBJECTS=$(SOURCES:.c=.o)

.PHONY : all clean

all : $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) mem-test-exec.c -o $(TARGET) $(LDFLAGS)

#all:
#	$(CC) $(CFLAGS) test-exec-mem-2.c -o $(TARGET) $(LDFLAGS)


.c.o :
	$(CC) $(CFLAGS)  -ffreestanding -c $< -o $@


install:
	cp ./$(TARGET) /usr/bin/$(TARGET)
	execstack -s /usr/bin/$(TARGET)
	chmod +x /usr/bin/$(TARGET)
	chcon --reference=/usr/bin/w /usr/bin/$(TARGET)

clean:
	rm -rf *.o ./$(TARGET)
