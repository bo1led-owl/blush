OUTDIR = build
TARGET = csh

MAIN_FLAGS = -std=c11
WARNINGS_FLAGS = -Wall -Wextra -Wpedantic -Wduplicated-branches -Wduplicated-cond -Wcast-qual -Wconversion -Wsign-conversion -Wlogical-op
SANITIZER_FLAGS = -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak -fsanitize=undefined -fsanitize-address-use-after-scope
DEBUG_FLAGS = $(MAIN_FLAGS) $(WARNINGS_FLAGS) $(SANITIZER_FLAGS) -g -O0
RELEASE_FLAGS = $(MAIN_FLAGS) $(WARNINGS_FLAGS) -s -O3

SOURCES = $(wildcard ./src/*.c)

run_debug: debug
	@$(OUTDIR)/debug/$(TARGET)

run_release: release
	@$(OUTDIR)/release/$(TARGET)

debug: $(OUTDIR)/debug/$(TARGET)
release: $(OUTDIR)/release/$(TARGET)

$(OUTDIR)/debug/$(TARGET): $(OUTDIR)/debug $(SOURCES)
	@$(CC) $(DEBUG_FLAGS) $(SOURCES) -o $@

$(OUTDIR)/release/$(TARGET): $(OUTDIR)/release $(SOURCES)
	@$(CC) $(RELEASE_FLAGS) $(SOURCES) -o $@

$(OUTDIR):
	@mkdir $@

$(OUTDIR)/debug: $(OUTDIR)
	@mkdir $@

$(OUTDIR)/release: $(OUTDIR)
	@mkdir $@

clean:
	@rm -r $(OUTDIR)

.PHONY: run_debug run_release clean
