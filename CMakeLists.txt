cmake_minimum_required(VERSION 3.18)

project(libtab VERSION "0.0.1")
# set(CMAKE_BUILD_TYPE Debug)

# Define CMAKE_INSTALL_xxx: LIBDIR, INCLUDEDIR
include(GNUInstallDirs)

# Currently, Scikit-build does not support FindPython, so we convert the
# provided hints ourselves.
if(SKBUILD)
   set(Python_EXECUTABLE "${PYTHON_EXECUTABLE}")
   set(Python_INCLUDE_DIR "${PYTHON_INCLUDE_DIR}")
   set(Python_LIBRARY "${PYTHON_LIBRARY}")
endif()

set(Python_FIND_IMPLEMENTATIONS CPython PyPy)
find_package(Python REQUIRED COMPONENTS Interpreter Development)

# Set C++ standard before finding pybind11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get pybind11 CMake dir
execute_process(
  COMMAND
    "${Python_EXECUTABLE}" -c
    "import pybind11; print(pybind11.get_cmake_dir())"
  OUTPUT_VARIABLE _tmp_cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ECHO STDOUT ECHO_OUTPUT_VARIABLE
  ECHO_ERROR_VARIABLE)
list(APPEND CMAKE_PREFIX_PATH "${_tmp_cmake_dir}")

find_package(Eigen3 REQUIRED)
message(STATUS "Found Eigen ${EIGEN3_INCLUDE_DIRS}")
include_directories(${EIGEN3_INCLUDE_DIRS})

# Now we can find pybind11
find_package(pybind11 CONFIG REQUIRED)

add_subdirectory(cpp)

pybind11_add_module(_libtabcpp MODULE cpp/wrapper.cpp)

set_target_properties(_libtabcpp PROPERTIES SKIP_BUILD_RPATH FALSE)
set_target_properties(_libtabcpp PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
set_target_properties(_libtabcpp PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

if (APPLE)
  set_target_properties(_libtabcpp PROPERTIES INSTALL_RPATH "@loader_path/lib")
else()
  set_target_properties(_libtabcpp PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
endif()

target_link_libraries(_libtabcpp PRIVATE pybind11::module tab)

file(GLOB header_files "cpp/*.h")
set_target_properties(tab PROPERTIES PUBLIC_HEADER "${header_files}")

install(TARGETS _libtabcpp DESTINATION .)
install(TARGETS tab
		LIBRARY DESTINATION lib
		PUBLIC_HEADER DESTINATION include)
