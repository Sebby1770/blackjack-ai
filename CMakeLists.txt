cmake_minimum_required(VERSION 3.16)
project(blackjack_ai LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT MSVC)
    add_compile_options(-Wall -Wextra)
endif()

# ---- core library ----------------------------------------------------------
add_library(bjcore
    src/Card.cpp
    src/Hand.cpp
    src/Deck.cpp
    src/Environment.cpp
    src/TabularAgent.cpp
    src/QLearningAgent.cpp
    src/DoubleQLearningAgent.cpp
    src/SarsaAgent.cpp
    src/ExpectedSarsaAgent.cpp
    src/MonteCarloAgent.cpp
    src/BasicStrategyAgent.cpp
    src/CountingAgent.cpp
    src/Chart.cpp
    src/Stats.cpp
    src/Persistence.cpp
    src/Game.cpp
    src/Analyzer.cpp
)
target_include_directories(bjcore PUBLIC include)
target_compile_features(bjcore PUBLIC cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(bjcore PUBLIC Threads::Threads)

# ---- optional SQLite persistence -------------------------------------------
# If SQLite3 is available the program writes run stats to blackjack.db in
# addition to the always-on CSV. If not, it builds fine without it.
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY NAMES sqlite3)
if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
    message(STATUS "SQLite found (${SQLITE3_LIBRARY}) -- SQL persistence enabled.")
    target_compile_definitions(bjcore PUBLIC USE_SQLITE)
    target_include_directories(bjcore PUBLIC ${SQLITE3_INCLUDE_DIR})
    target_link_libraries(bjcore PUBLIC ${SQLITE3_LIBRARY})
else()
    message(STATUS "SQLite not found -- building with CSV-only persistence.")
endif()

# ---- executable ------------------------------------------------------------
add_executable(blackjack src/main.cpp)
target_link_libraries(blackjack PRIVATE bjcore)

# ---- tests -----------------------------------------------------------------
enable_testing()
add_executable(bjtests tests/tests.cpp)
target_link_libraries(bjtests PRIVATE bjcore)
add_test(NAME unit COMMAND bjtests)
