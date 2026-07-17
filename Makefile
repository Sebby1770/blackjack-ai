# Convenience Makefile for people without CMake.
#   make            build ./blackjack and ./bjtests
#   make test       build and run the unit tests
#   make run        build and launch the interactive menu
#   make clean      remove build artefacts
#
# SQLite is auto-detected: if <sqlite3.h> and -lsqlite3 are available the
# program is built with SQL persistence, otherwise it falls back to CSV only.

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude -pthread
LDLIBS   ?= -pthread

# Probe for SQLite by trying to compile+link a tiny program.
SQLITE_OK := $(shell printf '\#include <sqlite3.h>\nint main(){sqlite3_libversion();return 0;}' \
             | $(CXX) -xc++ -std=c++17 - -lsqlite3 -o /dev/null 2>/dev/null && echo yes)
ifeq ($(SQLITE_OK),yes)
    CXXFLAGS += -DUSE_SQLITE
    LDLIBS   += -lsqlite3
endif

LIB_SRCS := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIB_OBJS := $(LIB_SRCS:.cpp=.o)

.PHONY: all test run clean
all: blackjack bjtests

blackjack: $(LIB_OBJS) src/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

bjtests: $(LIB_OBJS) tests/tests.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: bjtests
	./bjtests

run: blackjack
	./blackjack

clean:
	rm -f src/*.o tests/*.o blackjack bjtests
