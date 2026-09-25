CC ?= cc
CFLAGS ?= -O2
CPPFLAGS ?=
LDLIBS ?= -lgpiolib
PREFIX ?= /usr/local
BUILD_DIR ?= build

TARGET := $(BUILD_DIR)/pinwatch

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): pinwatch.c
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -Werror $< $(LDLIBS) -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/pinwatch

test:
	python3 -m unittest discover -s tests -v

clean:
	rm -rf $(BUILD_DIR)
