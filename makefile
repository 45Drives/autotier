TARGET = autotier
LIBS =  -lfuse3 -lpthread -lboost_system -lboost_filesystem -lboost_serialization -lstdc++ -lrocksdb
CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -I/usr/include/fuse3 -D_FILE_OFFSET_BITS=64 

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard src/*.cpp))
HEADERS = $(wildcard src/*.hpp)

TEST_OBJECTS = tests/view_db.o src/file.o src/tier.o src/alert.o

ifeq ($(PREFIX),)
	PREFIX := /opt/45drives/autotier
endif

.PHONY: default all clean clean-build clean-target install uninstall debug

default: CFLAGS := -O2 $(CFLAGS)
default: $(TARGET)
all: default

debug: CFLAGS := -g $(CFLAGS)
debug: $(TARGET)

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

install: all inst-man-pages inst-config inst-completion
	mkdir -p $(DESTDIR)$(PREFIX)
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)
	ln -sf $(PREFIX)/$(TARGET) $(DESTDIR)/usr/bin/$(TARGET)

uninstall: rm-man-pages rm-completion
	-rm -f $(DESTDIR)$(PREFIX)/$(TARGET)
	-rm -f $(DESTDIR)/usr/bin/$(TARGET)

tests: $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) -Wall $(LIBS) -o test_db

inst-man-pages:
	mkdir -p $(DESTDIR)/usr/share/man/man8
	gzip -k doc/man/autotier.8
	mv doc/man/autotier.8.gz $(DESTDIR)/usr/share/man/man8/

rm-man-pages:
	-rm -f $(DESTDIR)/usr/share/man/man8/autotier.8.gz

inst-config:
	mkdir -p $(DESTDIR)/etc
	cp -n doc/autotier.conf.template $(DESTDIR)/etc/autotier.conf

inst-completion:
	mkdir -p $(DESTDIR)/usr/share/bash-completion/completions
	cp doc/completion/autotier.bash-completion $(DESTDIR)/usr/share/bash-completion/completions/autotier

rm-completion:
	-rm -f $(DESTDIR)/usr/share/bash-completion/completions/autotier
