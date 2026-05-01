# project setup
PROJ = AmigaSplash
BUILD_DIR = .build
SRC_DIR = src
INCLUDE_DIR = include

VERSION_H = $(BUILD_DIR)/version.h
LOG_ITEMS = $(BUILD_DIR)/log_items.inc
LOG_DEFS = $(BUILD_DIR)/log_defs.inc

OUTPUT = $(BUILD_DIR)/$(PROJ)
#OUTPUT_ICON = $(OUTPUT).info
OUTPUT_MAP = $(OUTPUT).map
HOST_OUTPUT = $(shell wslpath -a -w $(OUTPUT))
HOST_ICON = $(shell wslpath -a -w $(OUTPUT_ICON))

# toolchain setup
TOOLCHAIN := m68k-amigaos-
CC := $(TOOLCHAIN)gcc
CXX := $(TOOLCHAIN)g++
STRIP := $(TOOLCHAIN)strip
CMD := cmd.exe
CRT := nix20

CFLAGS := \
	-m68010 \
	-mtune=68010 \
	-mcrt=$(CRT) \
	-funsigned-char \
	-Os \
	-fjump-tables \
	-fomit-frame-pointer \
	-foptimize-strlen \
	-ffunction-sections \
	-fdata-sections \
	-fshort-enums \
	-I$(BUILD_DIR) \
	-I$(INCLUDE_DIR) \


CXXFLAGS := \
	-I$(BUILD_DIR) \
	-I$(INCLUDE_DIR) \


CPPFLAGS := \
	-Wall \
	-Wextra \
	-Werror \
	-Wno-error=unused-function \
	-Wno-error=unused-parameter \
	-Wno-error=unused-variable \
	-Wno-error=unused-but-set-variable \
	-Wno-missing-field-initializers \
	-Wno-strict-aliasing \
	-Wno-pointer-sign \
	-Wno-ignored-qualifiers \
	-Wno-switch \
	-pipe \


LDFLAGS := \
	-mcrt=$(CRT) \
	-Wl,--gc-sections \
	-Wl,-Map=$(OUTPUT_MAP) \


ifeq ($(NLOG), 1)
CPPFLAGS := $(CPPFLAGS) -DNLOG=1
endif

ifeq ($(NDEBUG), 1)
CPPFLAGS := $(CPPFLAGS) -DNDEBUG=1
endif

# List source files
SRCS_S  =$(wildcard $(SRC_DIR)/*.s)
SRCS_C  =$(wildcard $(SRC_DIR)/*.c)
SRCS_CXX=$(wildcard $(SRC_DIR)/*.cpp)
SRCS_H  =$(wildcard $(INCLUDE_DIR)/*.h)

# List of object files generated from source files
OBJS = \
	$(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
	$(patsubst $(SRC_DIR)/%.s, $(BUILD_DIR)/%.o, $(SRCS_S)) \
	$(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS_CXX)) \


.PHONY: all generate_version generate_log_items generate_log_defs

all: $(OUTPUT)

dump:
	@echo CC: $(CC)
	@echo OBJS: $(OBJS)
	@echo SRCS_C: $(SRCS_C)
	@echo SRCS_CXX: $(SRCS_CXX)
	@echo SRCS_S: $(SRCS_S)

generate_version: $(BUILD_DIR)
	@echo -n "#define GIT_VERSION " > $(VERSION_H).tmp
	@git describe --tags --always --dirty 2> /dev/null >> $(VERSION_H).tmp || echo "unknown" >> $(VERSION_H).tmp
	@cmp -s $(VERSION_H) $(VERSION_H).tmp || (mv $(VERSION_H).tmp $(VERSION_H) && echo "Generated '$(VERSION_H)'")
	@rm -f $(VERSION_H).tmp

$(VERSION_H): generate_version

generate_log_items: $(BUILD_DIR)
	@for i in `rgrep -h LOG_FACILITY $(SRCS_C) | tr -d ' \r' | sed -nE 's/LOG_FACILITY\((.+),(.+)\)\;/\1/p'`; do echo "LOG_FACILITY_ITEM($$i)" >> "$(LOG_ITEMS).tmp"; done
	@cmp -s $(LOG_ITEMS) $(LOG_ITEMS).tmp || (mv $(LOG_ITEMS).tmp $(LOG_ITEMS) && echo "Generated '$(LOG_ITEMS)'")
	@rm -f $(LOG_ITEMS).tmp

$(LOG_ITEMS): generate_log_items

generate_log_defs: $(BUILD_DIR)
	@for i in `rgrep -h LOG_FACILITY $(SRCS_C) | tr -d ' \r' | sed -nE 's/LOG_FACILITY\((.+),(.+)\)\;/\1/p'`; do echo "LOG_FACILITY_DEF($$i)" >> "$(LOG_DEFS).tmp"; done
	@cmp -s $(LOG_DEFS) $(LOG_DEFS).tmp || (mv $(LOG_DEFS).tmp $(LOG_DEFS) && echo "Generated '$(LOG_DEFS)'")
	@rm -f $(LOG_DEFS).tmp

$(LOG_DEFS): generate_log_defs

# Generated object files
$(BUILD_DIR):
	@mkdir -pv $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s $(VERSION_H) $(LOG_DEFS) $(LOG_ITEMS) $(SRCS_H) Makefile
	@echo "Compiling '$<'"
	@$(CC) $(CPPFLAGS) -DBASE_FILE_NAME=$< -x assembler-with-cpp $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(VERSION_H) $(LOG_DEFS) $(LOG_ITEMS) $(SRCS_H) Makefile
	@echo "Compiling '$<'"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -DBASE_FILE_NAME=$< -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(VERSION_H) $(LOG_DEFS) $(LOG_ITEMS) $(SRCS_H) Makefile
	@echo "Compiling '$<'"
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DBASE_FILE_NAME=$< -c -o $@ $<

$(OUTPUT_ICON):
	cp assets/icon.info $(OUTPUT_ICON)

# Generate executable
$(OUTPUT): $(OBJS) $(OUTPUT_ICON)
	@echo "Linking '$@'"
	@$(CC) $(CPPFLAGS) $(LDFLAGS) $(OBJS) -o $@
ifeq ($(DOSTRIP), 1)
	@echo "Stripping '$@'"
	@$(STRIP) $@
endif
	@echo -n "Output file size: "
	@du -bh "$@" | cut -f1

clean:
	@rm -fv $(OBJS) $(VERSION_H) $(OUTPUT) $(OUTPUT_ICON) $(OUTPUT_MAP) $(LOG_DEFS) $(LOG_ITEMS)

deploy: $(OUTPUT)
	@echo "Copying stripped '$(OUTPUT)' to floppy..."
	@$(STRIP) $(OUTPUT)
	@$(CMD) /C copy '$(HOST_OUTPUT)' A:\\
	@$(CMD) /C copy '$(HOST_ICON)' A:\\
