
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude
CPPFLAGS =
LDFLAGS =

TARGET = pkgman

obj-y :=
subdir-y += src

V_RELEASE := 0
V_SUBVER  := 1
V_PATCH   := 0
V_EXTRA   := -rc0

VERSION := $(V_RELEASE).$(V_SUBVER).$(V_PATCH)$(V_EXTRA)

ROOT := $(CURDIR)
BUILD_HELPER := $(ROOT)/scripts/Makefile.build

export CC CFLAGS CPPFLAGS LDFLAGS

.PHONY: all clean source version help

all: source
	@set -eu; \
	objects=""; \
	for dir in $(subdir-y); do \
		result="$$($(MAKE) --no-print-directory -s \
			-f "$(BUILD_HELPER)" \
			ROOT="$(ROOT)" SRCDIR="$$dir" print-objs)"; \
		objects="$$objects $$result"; \
	done; \
	if [ -z "$$objects" ]; then \
		echo "Error: no object files to link"; \
		exit 1; \
	fi; \
	$(CC) $(LDFLAGS) -o "$(TARGET)" $$objects; \
	echo "  LINK    $(TARGET)"

source:
	@set -e; \
	for dir in $(subdir-y); do \
		$(MAKE) --no-print-directory \
			-f "$(BUILD_HELPER)" \
			ROOT="$(ROOT)" SRCDIR="$$dir" source; \
	done

version:
	@echo "$(VERSION)"

help:
	@echo "====================================="
	@echo " pkgman $(VERSION)"
	@echo "====================================="
	@echo "all:     Build pkgman"
	@echo "clean:   Clean build artifacts"
	@echo "help:    Display this message"
	@echo "version: Print pkgman version"
	@echo "====================================="

clean:
	rm -f "$(TARGET)"
	@set -e; \
	for dir in $(subdir-y); do \
		$(MAKE) --no-print-directory \
			-f "$(BUILD_HELPER)" \
			ROOT="$(ROOT)" SRCDIR="$$dir" clean; \
	done
