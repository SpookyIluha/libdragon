INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
all: mkmaterial

mkmaterial: mkmaterial.c
	@echo "    [TOOL] mkmaterial"
	$(CC) $(CFLAGS) mkmaterial.c -o mkmaterial -lm

install: mkmaterial
	install -m 0755 mkmaterial $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkmaterial
