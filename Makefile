CC      ?= gcc
CFLAGS  ?= -std=gnu11 -Wall -Wextra -Wconversion -Werror -O2 -g -pthread -Iinclude
LDFLAGS ?= -pthread
BUILD   := build

BASE := estado util filtro trama archivo dibujo juego menu dino pong prueba
HW   := $(BASE) spi adc botones pantalla hw principal

all: $(BUILD)/arcade

$(BUILD)/arcade: $(addprefix src/,$(addsuffix .c,$(HW))) | $(BUILD)
	@ver=$$(pkg-config --modversion libgpiod 2>/dev/null || true); \
	echo "$$ver" | grep -q '^1\.' || { echo "hace falta libgpiod 1.x (bookworm trae 1.6.3), hay [$$ver]"; exit 1; }
	$(CC) $(CFLAGS) -o $@ $(addprefix src/,$(addsuffix .c,$(HW))) $(LDFLAGS) $(shell pkg-config --libs libgpiod)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
