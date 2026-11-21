CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iengine/include
LDFLAGS = -lm

SRC_DIR = engine/src
BIN_DIR = bin
TARGET  = $(BIN_DIR)/freight_engine

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/controller.c \
	$(SRC_DIR)/graph.c \
	$(SRC_DIR)/avl_tree.c \
	$(SRC_DIR)/b_tree.c \
	$(SRC_DIR)/astar.c \
	$(SRC_DIR)/dijkstra.c \
	$(SRC_DIR)/bellman_ford.c \
	$(SRC_DIR)/prims.c \
	$(SRC_DIR)/dp_planner.c \
	$(SRC_DIR)/knapsack.c

all: $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
