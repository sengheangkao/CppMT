cmake_minimum_required(VERSION 3.15...3.30)
project(MovieTicket)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── Permanent rpath: embed /usr/local/lib into the binary ───────────────────
set(CMAKE_INSTALL_RPATH "/usr/local/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# ── Find xlnt ────────────────────────────────────────────────────────────────
set(XLNT_INCLUDE_DIR /usr/local/include)
set(XLNT_LIB_DIR     /usr/local/lib)

# ── Source files ─────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/User.cpp
    src/Movie.cpp
    src/Ticket.cpp
    src/DataManager.cpp
    src/AuthManager.cpp
    src/MenuManager.cpp
)

add_executable(MovieTicket ${SOURCES})

target_include_directories(MovieTicket PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${XLNT_INCLUDE_DIR}
)

target_link_directories(MovieTicket PRIVATE ${XLNT_LIB_DIR})
find_package(Threads REQUIRED)
target_link_libraries(MovieTicket PRIVATE xlnt Threads::Threads)
