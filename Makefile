CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -O2
LDFLAGS  := -pthread

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build

SRCS := $(SRC_DIR)/main.cpp \
        $(SRC_DIR)/RedisServer.cpp \
        $(SRC_DIR)/RESPParser.cpp \
        $(SRC_DIR)/KeyValueStore.cpp \
        $(SRC_DIR)/CommandHandler.cpp \
        $(SRC_DIR)/Snapshot.cpp

OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET := redis-server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
