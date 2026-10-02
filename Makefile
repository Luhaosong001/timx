CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -O2 -Iinclude
BUILD   = build

.PHONY: test clean

test: $(BUILD)/test_timx
	./$(BUILD)/test_timx

$(BUILD)/test_timx: tests/test_timx.c src/timx.c include/timx/timx.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) tests/test_timx.c src/timx.c -o $@

clean:
	rm -rf $(BUILD)
