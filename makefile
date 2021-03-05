TARGET = dist/from_source/autotier
LIBS =  -lfuse3 -lpthread -lboost_system -lboost_filesystem -lboost_serialization -lstdc++ -lrocksdb
CC = g++
CFLAGS = -Wall -Wextra -Isrc/incl -I/usr/include/fuse3 -D_FILE_OFFSET_BITS=64

SOURCE_FILES := $(shell find src/impl -name *.cpp)
OBJECT_FILES := $(patsubst src/impl/%.cpp, build/%.o, $(SOURCE_FILES))

ifeq ($(PREFIX),)
	PREFIX := /opt/45drives/autotier
endif

.PHONY: default all clean clean-build clean-target install uninstall debug

default: CFLAGS := -std=c++17 -O2 $(CFLAGS)
default: LIBS := -ltbb $(LIBS)
default: $(TARGET)
all: default

debug: CFLAGS := -std=c++17 -g -DLOG_METHODS $(CFLAGS)
debug: LIBS := -ltbb $(LIBS)
debug: $(TARGET)

no-par-sort: CFLAGS := -std=c++11 $(CFLAGS)
no-par-sort: $(TARGET)

.PRECIOUS: $(TARGET) $(OBJECTS)

$(OBJECT_FILES): build/%.o : src/impl/%.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(patsubst build/%.o, src/impl/%.cpp, $@) -o $@

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

tests: view-db

view-db:
	mkdir -p dist/tests
	$(CC) $(CFLAGS) -DBAREBONES_METADATA tests/src/view_db.cpp src/impl/metadata.cpp -Wall $(LIBS) -o dist/tests/view_db


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
