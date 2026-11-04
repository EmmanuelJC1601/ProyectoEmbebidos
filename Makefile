CC      ?= gcc
CFLAGS  ?= -std=gnu11 -Wall -Wextra -Wconversion -O2 -g -pthread -Iinclude
LDFLAGS ?= -pthread
BUILD   := build

# Bookworm trae libgpiod 1.6 (sin -DGPIOD_API_V2). Trixie trae 2.x.
GPIOD_VER := $(shell pkg-config --modversion libgpiod 2>/dev/null)
ifneq ($(filter 2.%,$(GPIOD_VER)),)
CFLAGS += -DGPIOD_API_V2=1
endif

COMMON  := util filter mcp3008 lcd1602 storage game game_diag
SRCS    := $(addprefix src/,$(addsuffix .c,$(COMMON) buttons main))
OBJS    := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
GPIOD_L ?= $(shell pkg-config --libs libgpiod 2>/dev/null || echo -lgpiod)

all: $(BUILD)/arcade

$(BUILD)/arcade: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(GPIOD_L)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Pruebas de logica pura: no necesitan Raspberry Pi ni libgpiod
TEST_SRCS := tests/test_logic.c $(addprefix src/,$(addsuffix .c,util filter mcp3008 storage game game_diag))
test: | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/test_logic $(TEST_SRCS)
	./$(BUILD)/test_logic

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
.PHONY: all test clean
