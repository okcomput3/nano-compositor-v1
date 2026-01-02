

CC = gcc

WLROOTS_CFLAGS = $(shell pkg-config --cflags wlroots)
WLROOTS_LIBS = $(shell pkg-config --libs wlroots)

CFLAGS = -Wall -Wextra -fPIC -g -D_GNU_SOURCE $(WLROOTS_CFLAGS)
LIBS = -lgbm -lEGL -lGL -ldrm $(WLROOTS_LIBS)

SOURCES = dmabuf_share.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = libdmabuf_share.so

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -shared -o $@ $(OBJECTS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean