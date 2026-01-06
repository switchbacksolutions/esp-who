# ESP-WHO Root CMakeLists.txt
# Note: This is a component library. To build examples, navigate to the example directories
# (e.g., examples/human_face_detect/) and run idf.py commands from there.

cmake_minimum_required(VERSION 3.5)

# This root CMakeLists.txt is provided for compatibility with tools that expect
# a CMakeLists.txt at the project root. For actual builds, use the example directories.

# If you want to build a specific example, you can set it here:
# set(EXAMPLE_DIR "examples/human_face_detect")

# For ESP-IDF projects, you typically build from example directories using:
# cd examples/human_face_detect
# idf.py build

# Basic CMake setup to prevent errors
project(esp-who)

message(STATUS "ESP-WHO Component Library")
message(STATUS "To build examples, navigate to examples/ directory and run idf.py commands")
message(STATUS "Example: cd examples/human_face_detect && idf.py build")

