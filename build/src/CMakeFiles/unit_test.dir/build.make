# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = "/home/dayday/桌面/db2025-main (2)/rmdb"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/home/dayday/桌面/db2025-main (2)/rmdb/build"

# Include any dependencies generated for this target.
include src/CMakeFiles/unit_test.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/CMakeFiles/unit_test.dir/compiler_depend.make

# Include the progress variables for this target.
include src/CMakeFiles/unit_test.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/unit_test.dir/flags.make

src/CMakeFiles/unit_test.dir/unit_test.cpp.o: src/CMakeFiles/unit_test.dir/flags.make
src/CMakeFiles/unit_test.dir/unit_test.cpp.o: /home/dayday/桌面/db2025-main\ (2)/rmdb/src/unit_test.cpp
src/CMakeFiles/unit_test.dir/unit_test.cpp.o: src/CMakeFiles/unit_test.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir="/home/dayday/桌面/db2025-main (2)/rmdb/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/CMakeFiles/unit_test.dir/unit_test.cpp.o"
	cd "/home/dayday/桌面/db2025-main (2)/rmdb/build/src" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/CMakeFiles/unit_test.dir/unit_test.cpp.o -MF CMakeFiles/unit_test.dir/unit_test.cpp.o.d -o CMakeFiles/unit_test.dir/unit_test.cpp.o -c "/home/dayday/桌面/db2025-main (2)/rmdb/src/unit_test.cpp"

src/CMakeFiles/unit_test.dir/unit_test.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/unit_test.dir/unit_test.cpp.i"
	cd "/home/dayday/桌面/db2025-main (2)/rmdb/build/src" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E "/home/dayday/桌面/db2025-main (2)/rmdb/src/unit_test.cpp" > CMakeFiles/unit_test.dir/unit_test.cpp.i

src/CMakeFiles/unit_test.dir/unit_test.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/unit_test.dir/unit_test.cpp.s"
	cd "/home/dayday/桌面/db2025-main (2)/rmdb/build/src" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S "/home/dayday/桌面/db2025-main (2)/rmdb/src/unit_test.cpp" -o CMakeFiles/unit_test.dir/unit_test.cpp.s

# Object files for target unit_test
unit_test_OBJECTS = \
"CMakeFiles/unit_test.dir/unit_test.cpp.o"

# External object files for target unit_test
unit_test_EXTERNAL_OBJECTS =

bin/unit_test: src/CMakeFiles/unit_test.dir/unit_test.cpp.o
bin/unit_test: src/CMakeFiles/unit_test.dir/build.make
bin/unit_test: lib/libstorage.a
bin/unit_test: lib/liblru_replacer.a
bin/unit_test: lib/librecord.a
bin/unit_test: lib/libgtest_main.a
bin/unit_test: lib/libsystem.a
bin/unit_test: lib/libtransaction.a
bin/unit_test: lib/librecovery.a
bin/unit_test: lib/librecord.a
bin/unit_test: lib/libsystem.a
bin/unit_test: lib/libtransaction.a
bin/unit_test: lib/librecovery.a
bin/unit_test: lib/libindex.a
bin/unit_test: lib/libstorage.a
bin/unit_test: lib/libgtest.a
bin/unit_test: src/CMakeFiles/unit_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir="/home/dayday/桌面/db2025-main (2)/rmdb/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable ../bin/unit_test"
	cd "/home/dayday/桌面/db2025-main (2)/rmdb/build/src" && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/unit_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/unit_test.dir/build: bin/unit_test
.PHONY : src/CMakeFiles/unit_test.dir/build

src/CMakeFiles/unit_test.dir/clean:
	cd "/home/dayday/桌面/db2025-main (2)/rmdb/build/src" && $(CMAKE_COMMAND) -P CMakeFiles/unit_test.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/unit_test.dir/clean

src/CMakeFiles/unit_test.dir/depend:
	cd "/home/dayday/桌面/db2025-main (2)/rmdb/build" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/home/dayday/桌面/db2025-main (2)/rmdb" "/home/dayday/桌面/db2025-main (2)/rmdb/src" "/home/dayday/桌面/db2025-main (2)/rmdb/build" "/home/dayday/桌面/db2025-main (2)/rmdb/build/src" "/home/dayday/桌面/db2025-main (2)/rmdb/build/src/CMakeFiles/unit_test.dir/DependInfo.cmake" "--color=$(COLOR)"
.PHONY : src/CMakeFiles/unit_test.dir/depend

