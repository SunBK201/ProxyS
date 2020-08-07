CC = gcc 
LD = gcc

SRCS = $(wildcard *.c)

OBJS = $(patsubst %c, %o, $(SRCS))

TARGET = server

.PHONY:all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $^ -I /usr/include/libxml2/ -l xml2

%.o:%.c
	$(CC) -c $^

clean:
	rm -f $(OBJS) $(TARGET)
