CFLAGS = -Wall -ggdb3 -std=gnu99
LDFLAGS = $(shell pkg-config --cflags gstreamer-1.0)
LDLIBS = $(shell pkg-config --libs gstreamer-1.0)

all: nightconv

install:
	install -m755 nightconv /usr/local/bin/

clean:
	rm -f nightconv
