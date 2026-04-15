cmake_minimum_required(VERSION 3.16)
project(customer_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Standalone Asio (no Boost required) ──────────────────────────────────────
# Crow can use either Boost.Asio or standalone Asio.
# We use standalone to avoid the get_io_service() breakage in Boost >= 1.70.
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-28-0
)
FetchContent_MakeAvailable(asio)

# ── Crow (header-only HTTP framework) ────────────────────────────────────────
# v1.1.0 is stable with standalone Asio on Ubuntu 22.04.
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.1.0
)
FetchContent_MakeAvailable(Crow)

# ── nlohmann/json (header-only) ──────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── libpqxx (system package: apt install libpqxx-dev) ────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# ── Target ────────────────────────────────────────────────────────────────────
add_executable(customer_service src/main.cpp)

target_include_directories(customer_service PRIVATE
    ${PQXX_INCLUDE_DIRS}
    ${asio_SOURCE_DIR}/asio/include   # standalone Asio headers
)

target_link_libraries(customer_service PRIVATE
    Crow::Crow
    nlohmann_json::nlohmann_json
    ${PQXX_LIBRARIES}
    pthread
)

# Tell Crow to use standalone Asio, not Boost
target_compile_definitions(customer_service PRIVATE
    CROW_USE_BOOST=0
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_FUNCTIONAL_
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
