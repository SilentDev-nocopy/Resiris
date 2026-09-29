cmake_minimum_required(VERSION 3.16)
project(resiris_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_library(resiris
  src/value.cpp
  src/tokenizer.cpp
  src/parser.cpp
  src/interpreter.cpp
  src/rsmath.cpp
  src/rsbase.cpp
)
target_include_directories(resiris PUBLIC include)
target_compile_options(resiris PRIVATE -Wall -Wextra)

add_executable(resiris-cli src/cli.cpp)
target_link_libraries(resiris-cli PRIVATE resiris)
target_compile_options(resiris-cli PRIVATE -Wall -Wextra)
set_target_properties(resiris-cli PROPERTIES OUTPUT_NAME resiris)

# Installs the `resiris` command, the same entry point name the Python package
# provides via [project.scripts]:
#
#   cmake --install build --prefix ~/.local
#
# The C++ port resolves its modules internally, so it runs from any directory
# with no PYTHONPATH or virtualenv setup.
include(GNUInstallDirs)
install(TARGETS resiris-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

enable_testing()

add_executable(resiris-tests
  tests/test_main.cpp
  tests/test_tokenizer.cpp
  tests/test_parser.cpp
  tests/test_declarations.cpp
  tests/test_expressions.cpp
  tests/test_control_flow.cpp
  tests/test_modules.cpp
  tests/test_rsbase.cpp
)
target_link_libraries(resiris-tests PRIVATE resiris)
target_include_directories(resiris-tests PRIVATE third_party)
add_test(NAME resiris-tests COMMAND resiris-tests)