CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS = -lm

SRCS    = src/cJSON.c src/avl_tree.c src/avl_pq.c src/b_tree.c \
          src/graph.c src/dijkstra.c src/astar.c src/bellman_ford.c \
          src/prim.c src/engine.c src/main.c

ifeq ($(OS),Windows_NT)
    TARGET  = ../bin/freight_engine.exe
    LDFLAGS =
else
    TARGET  = ../bin/freight_engine
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p ../bin
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f ../bin/freight_engine ../bin/freight_engine.exe