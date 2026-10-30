# ==============================================================================
# ModernQuantSuite - Universal C++20 Makefile
# ==============================================================================

CXX ?= clang++
UNAME_S := $(shell uname -s)

# Detect Include Paths Dynamically
CONDA_INC := $(if $(CONDA_PREFIX),$(CONDA_PREFIX)/include,)
HOMEBREW_ARM := /opt/homebrew/include
HOMEBREW_INTEL := /usr/local/include
LOCAL_CONDA := $(HOME)/anaconda3/include

# Candidate include locations for Eigen & Boost
POSSIBLE_INCLUDES := include \
                     $(CONDA_INC) \
                     $(CONDA_INC)/eigen3 \
                     $(HOMEBREW_ARM) \
                     $(HOMEBREW_ARM)/eigen3 \
                     $(HOMEBREW_INTEL) \
                     $(HOMEBREW_INTEL)/eigen3 \
                     $(LOCAL_CONDA) \
                     $(LOCAL_CONDA)/eigen3 \
                     /usr/include \
                     /usr/include/eigen3 \
                     /usr/local/include/eigen3

# Filter only existing directories
EXISTING_INCLUDES := $(foreach dir,$(POSSIBLE_INCLUDES),$(if $(wildcard $(dir)),-I$(dir)))

# macOS SDK detection
ifeq ($(UNAME_S),Darwin)
    SDK_PATH := $(shell xcrun --show-sdk-path 2>/dev/null)
    MAC_FLAGS := -isysroot $(SDK_PATH) -nostdinc++ -isystem $(SDK_PATH)/usr/include/c++/v1
else
    MAC_FLAGS :=
endif

CXXFLAGS := $(MAC_FLAGS) \
            -std=c++20 \
            -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION \
            -Wno-deprecated-declarations \
            -Wno-deprecated-builtins \
            -Wall -Wextra \
            $(EXISTING_INCLUDES)

LDFLAGS := -lcurl -pthread

SRCS := src/main.cpp
TARGET := bin/quant_suite
TARGET_ASAN := bin/quant_suite_asan

.PHONY: all release sanitize test clean help

all: release

release: $(SRCS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -O3 $(SRCS) $(LDFLAGS) -o $(TARGET)
	@echo "✔ Release build completed: $(TARGET)"

sanitize: $(SRCS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=address,undefined $(SRCS) $(LDFLAGS) -o $(TARGET_ASAN)
	@echo "✔ Sanitizer build completed: $(TARGET_ASAN)"

test: sanitize
	@echo "\n=== [1] Running AddressSanitizer & UndefinedBehaviorSanitizer ==="
	./$(TARGET_ASAN) --test
	@echo "\n=== [2] Running Performance Release Test ==="
	@$(MAKE) release
	./$(TARGET) --test

clean:
	rm -rf bin
	@echo "✔ Cleaned build artifacts."

help:
	@echo "ModernQuantSuite Build Commands:"
	@echo "  make          - Build optimized release binary (bin/quant_suite)"
	@echo "  make test     - Build & run full textbook verification test suite"
	@echo "  make sanitize - Build AddressSanitizer debug binary"
	@echo "  make clean    - Remove build artifacts"
