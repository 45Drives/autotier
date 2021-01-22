TARGET = autotier
LIBS =  -l:libfuse3.so -l:libpthread.so -l:libboost_system.a -l:libboost_filesystem.a -l:libstdc++.a -lsqlite3
CC = g++
CFLAGS = -g -std=gnu++11 -Wall -Wextra -I/usr/include/fuse3

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard src/*.cpp))
HEADERS = $(wildcard src/*.hpp)

ifeq ($(PREFIX),)
	PREFIX := /opt/45drives/autotier
endif

.PHONY: default all clean clean-build clean-target install uninstall

default: $(TARGET)
all: default

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean: clean-build clean-target

clean-target:
	-rm -f $(TARGET)

clean-build:
	-rm -f src/*.o

install: all
	install -m 755 autotier $(DESTDIR)$(PREFIX)/bin
	cp autotier.service /usr/lib/systemd/system/autotier.service
	systemctl daemon-reload

uninstall:
	-systemctl disable --now autotier
	-rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	-rm -f /usr/lib/systemd/system/autotier.service
	systemctl daemon-reload
