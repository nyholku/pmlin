TARGET_EXEC = pmlin-demo

BUILD_DIR = ./build
PMLIN_DIR = $(abspath ..)
SRC_DIRS = ./src $(PMLIN_DIR)/master/src $(PMLIN_DIR)/slave/src 
SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d) $(PMLIN_DIR)/includes/ $(PMLIN_DIR)/includes/devices
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS += -target macos-x86_64
CPPFLAGS ?= $(INC_FLAGS) -MMD -MP
LDFLAGS += -target macos-x86_64

$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p

all: pmlin-demo
