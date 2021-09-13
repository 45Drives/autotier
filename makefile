FS_TARGET = dist/from_source/autotierfs
CLI_TARGET = dist/from_source/autotier
FS_LIBS =  -lfuse3 -lpthread -lboost_system -lboost_filesystem -lboost_serialization -lrocksdb -l:lib45d.a
CLI_LIBS = -l:libboost_system.a -l:libboost_filesystem.a -lpthread -l:lib45d.a
CC = g++
CFLAGS = -g -Wall -Wextra -Isrc/incl -I/usr/include/fuse3 -D_FILE_OFFSET_BITS=64

FS_LIBS += $(EXTRA_LIBS)
CFLAGS += $(EXTRA_CFLAGS)

FS_SOURCE_FILES := $(shell find src/impl/autotierfs -name *.cpp)
FS_OBJECT_FILES := $(patsubst src/impl/%.cpp, build/%.o, $(FS_SOURCE_FILES))

CLI_SOURCE_FILES := $(shell find src/impl/autotier -name *.cpp)
CLI_OBJECT_FILES := $(patsubst src/impl/%.cpp, build/%.o, $(CLI_SOURCE_FILES))

SHARED_SOURCE_FILES := $(shell find src/impl/shared -name *.cpp)
SHARED_OBJECT_FILES := $(patsubst src/impl/%.cpp, build/%.o, $(SHARED_SOURCE_FILES))

ifeq ($(PREFIX),)
	PREFIX := /opt/45drives/autotier
	DEFAULT_PREFIX := 1
endif

.PHONY: default all clean clean-build clean-target install uninstall debug

default: CFLAGS := -std=c++17 -O2 $(CFLAGS)
default: FS_LIBS := -ltbb $(FS_LIBS)
default: $(FS_TARGET) $(CLI_TARGET)
all: default

debug: CFLAGS := -std=c++17 -g -DLOG_METHODS $(CFLAGS)
debug: FS_LIBS := -ltbb $(FS_LIBS)
debug: $(FS_TARGET) $(CLI_TARGET)

no-par-sort: CFLAGS := -std=c++11 -O2 $(CFLAGS)
no-par-sort: $(FS_TARGET) $(CLI_TARGET)

.PRECIOUS: $(TARGET) $(OBJECTS)

$(FS_OBJECT_FILES) $(CLI_OBJECT_FILES) $(SHARED_OBJECT_FILES): build/%.o : src/impl/%.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(patsubst build/%.o, src/impl/%.cpp, $@) -o $@

$(FS_TARGET): $(FS_OBJECT_FILES) $(SHARED_OBJECT_FILES)
	mkdir -p dist/from_source
	$(CC) $(FS_OBJECT_FILES) $(SHARED_OBJECT_FILES) -Wall $(FS_LIBS) -o $@

$(CLI_TARGET): $(CLI_OBJECT_FILES) $(SHARED_OBJECT_FILES)
	mkdir -p dist/from_source
	$(CC) $(CLI_OBJECT_FILES) $(SHARED_OBJECT_FILES) -Wall $(CLI_LIBS) -o $@

clean: clean-build clean-target

clean-target:
	-rm -rf dist/from_source

clean-build:
	-rm -rf build

clean-tests:
	-rm -rf dist/tests

install: all inst-man-pages inst-config inst-completion inst-scripts
	mkdir -p $(DESTDIR)$(PREFIX)
	install -m 755 $(FS_TARGET) $(DESTDIR)$(PREFIX)
	install -m 755 $(CLI_TARGET) $(DESTDIR)$(PREFIX)
ifeq ($(DEFAULT_PREFIX),1)
	mkdir -p $(DESTDIR)/usr/bin
	ln -sf $(PREFIX)/$(notdir $(FS_TARGET)) $(DESTDIR)/usr/bin/$(notdir $(FS_TARGET))
	ln -sf $(PREFIX)/$(notdir $(CLI_TARGET)) $(DESTDIR)/usr/bin/$(notdir $(CLI_TARGET))
endif
ifneq ($(PACKAGING),1)
	groupadd -f autotier
endif

uninstall: rm-man-pages rm-completion rm-scripts
	-rm -f $(DESTDIR)$(PREFIX)/$(notdir $(FS_TARGET))
	-rm -f $(DESTDIR)$(PREFIX)/$(notdir $(CLI_TARGET))
	-rm -f $(DESTDIR)/usr/bin/$(notdir $(FS_TARGET))
	-rm -f $(DESTDIR)/usr/bin/$(notdir $(CLI_TARGET))

tests: view-db

view-db:
	mkdir -p dist/tests
	$(CC) $(CFLAGS) -DBAREBONES_METADATA tests/src/view_db.cpp src/impl/autotierfs/metadata.cpp -Wall -lboost_serialization -lrocksdb -o dist/tests/view_db

inst-man-pages:
	mkdir -p $(DESTDIR)/usr/share/man/man8
	gzip -k doc/man/autotier.8
	mv doc/man/autotier.8.gz $(DESTDIR)/usr/share/man/man8/
	ln -snf autotier.8.gz  $(DESTDIR)/usr/share/man/man8/autotierfs.8.gz

rm-man-pages:
	-rm -f $(DESTDIR)/usr/share/man/man8/autotier.8.gz
	-rm -f $(DESTDIR)/usr/share/man/man8/autotierfs.8.gz

inst-config:
	mkdir -p $(DESTDIR)/etc
	cp -n doc/autotier.conf.template $(DESTDIR)/etc/autotier.conf

inst-completion:
	mkdir -p $(DESTDIR)/usr/share/bash-completion/completions
	cp doc/completion/autotier.bash-completion $(DESTDIR)/usr/share/bash-completion/completions/autotier
	cp doc/completion/autotierfs.bash-completion $(DESTDIR)/usr/share/bash-completion/completions/autotierfs

rm-completion:
	-rm -f $(DESTDIR)/usr/share/bash-completion/completions/autotier
	-rm -f $(DESTDIR)/usr/share/bash-completion/completions/autotierfs

inst-scripts:
	mkdir -p $(DESTDIR)$(PREFIX)
	install -m 755 script/autotier-init-dirs $(DESTDIR)$(PREFIX)
ifeq ($(DEFAULT_PREFIX),1)
	mkdir -p $(DESTDIR)/usr/bin
	ln -sf $(PREFIX)/autotier-init-dirs $(DESTDIR)/usr/bin/autotier-init-dirs
endif

rm-scripts:
	-rm -f $(DESTDIR)$(PREFIX)/autotier-init-dirs
	-rm -f $(DESTDIR)/usr/bin/autotier-init-dirs
