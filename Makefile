
OBJS_DIR = .objs

# define all the student executables
EXE_CLIENT=client
EXE_SERVER=server
EXES_ALL=$(EXE_CLIENT) $(EXE_SERVER)

# list object file dependencies for each
OBJS_CLIENT=$(EXE_CLIENT).o chat_window.o user_hooks.o client.o utils.o
OBJS_SERVER=$(EXE_SERVER).o user_hooks.o utils.o

# set up compiler
CC = clang
INCLUDES = -I./includes
WARNINGS = -Wall -Wextra -Werror -Wno-error=unused-parameter -Wmissing-declarations -Wmissing-variable-declarations
CFLAGS_DEBUG   = -O0 $(WARNINGS) $(INCLUDES) -g -std=c99 -c -MMD -MP -DDEBUG -D_GNU_SOURCE -pthread
CFLAGS_RELEASE = -O2 $(WARNINGS) $(INCLUDES) -std=c99 -c -MMD -MP -D_GNU_SOURCE -pthread

# set up linker
LD = clang
LDFLAGS = -pthread -lncurses -lm

.PHONY: all
all: release

# build types
# run clean before building debug so that all of the release executables
# disappear
.PHONY: debug
.PHONY: release

release: $(EXES_ALL)
debug:   clean $(EXES_ALL:%=%-debug)

# include dependencies
-include $(OBJS_DIR)/*.d

$(OBJS_DIR):
	@mkdir -p $(OBJS_DIR)

# patterns to create objects
# keep the debug and release postfix for object files so that we can always
# separate them correctly
$(OBJS_DIR)/%-debug.o: %.c | $(OBJS_DIR)
	$(CC) $(CFLAGS_DEBUG) $< -o $@

$(OBJS_DIR)/%-release.o: %.c | $(OBJS_DIR)
	$(CC) $(CFLAGS_RELEASE) $< -o $@

# exes
# you will need a triple of exe and exe-debug and exe-tsan for each exe (other
# than provided exes)

$(EXE_CLIENT): $(OBJS_CLIENT:%.o=$(OBJS_DIR)/%-release.o)
	$(LD) $^ $(LDFLAGS) -o $@

$(EXE_CLIENT)-debug: $(OBJS_CLIENT:%.o=$(OBJS_DIR)/%-debug.o)
	$(LD) $^ $(LDFLAGS) -o $@

$(EXE_SERVER): $(OBJS_SERVER:%.o=$(OBJS_DIR)/%-release.o)
	$(LD) $^ $(LDFLAGS) -o $@

$(EXE_SERVER)-debug: $(OBJS_SERVER:%.o=$(OBJS_DIR)/%-debug.o)
	$(LD) $^ $(LDFLAGS) -o $@


.PHONY: clean
clean:
	-rm -rf .objs $(EXES_ALL) $(EXES_ALL:%=%-debug)
