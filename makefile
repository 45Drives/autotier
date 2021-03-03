TARGET = dist/from_source/autotier
LIBS =  -lfuse3 -lpthread -lboost_system -lboost_filesystem -lboost_serialization -lstdc++ -lrocksdb
CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -I/usr/include/fuse3 -D_FILE_OFFSET_BITS=64 

SOURCE_FILES := $(wildcard src/*.cpp)
OBJECT_FILES := $(patsubst src/%.cpp, build/%.o, $(SOURCE_FILES))

TEST_OBJECTS = tests/view_db.o src/file.o src/tier.o src/alert.o

ifeq ($(PREFIX),)
	PREFIX := /opt/45drives/autotier
endif

.PHONY: default all clean clean-build clean-target install uninstall debug

default: CFLAGS := -O2 $(CFLAGS)
default: $(TARGET)
all: default

debug: CFLAGS := -g -DLOG_METHODS $(CFLAGS)
debug: $(TARGET)

.PRECIOUS: $(TARGET) $(OBJECTS)

$(OBJECT_FILES): build/%.o : src/%.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(patsubst build/%.o, src/%.cpp, $@) -o $@

$(TARGET): $(OBJECT_FILES)
	mkdir -p dist/from_source
	$(CC) $(OBJECT_FILES) -Wall $(LIBS) -o $@

clean: clean-build clean-target

clean-target:
	-rm -rf dist/from_source

clean-build:
	-rm -rf build

install: all inst-man-pages inst-config inst-completion
	mkdir -p $(DESTDIR)$(PREFIX)
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)
	ln -sf $(PREFIX)/$(notdir $(TARGET)) $(DESTDIR)/usr/bin/$(notdir $(TARGET))

uninstall: rm-man-pages rm-completion
	-rm -f $(DESTDIR)$(PREFIX)/$(notdir $(TARGET))
	-rm -f $(DESTDIR)/usr/bin/$(notdir $(TARGET))

tests: $(TEST_OBJECTS)
	mkdir -p dist/tests
	$(CC) $(TEST_OBJECTS) -Wall $(LIBS) -o dist/tests/test_db

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
