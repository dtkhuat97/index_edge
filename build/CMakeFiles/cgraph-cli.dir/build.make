# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

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
CMAKE_SOURCE_DIR = /home/dtkhuat/index_edge

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/dtkhuat/index_edge/build

# Include any dependencies generated for this target.
include CMakeFiles/cgraph-cli.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/cgraph-cli.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/cgraph-cli.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cgraph-cli.dir/flags.make

CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o: CMakeFiles/cgraph-cli.dir/flags.make
CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o: ../cmd/cgraph.c
CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o: CMakeFiles/cgraph-cli.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/dtkhuat/index_edge/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o -MF CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o.d -o CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o -c /home/dtkhuat/index_edge/cmd/cgraph.c

CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.i"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/dtkhuat/index_edge/cmd/cgraph.c > CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.i

CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.s"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/dtkhuat/index_edge/cmd/cgraph.c -o CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.s

# Object files for target cgraph-cli
cgraph__cli_OBJECTS = \
"CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o"

# External object files for target cgraph-cli
cgraph__cli_EXTERNAL_OBJECTS =

cgraph-cli: CMakeFiles/cgraph-cli.dir/cmd/cgraph.c.o
cgraph-cli: CMakeFiles/cgraph-cli.dir/build.make
cgraph-cli: libcgraph.so.1.0.0
cgraph-cli: /home/linuxbrew/.linuxbrew/lib/libserd-0.so
cgraph-cli: CMakeFiles/cgraph-cli.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/dtkhuat/index_edge/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable cgraph-cli"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cgraph-cli.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cgraph-cli.dir/build: cgraph-cli
.PHONY : CMakeFiles/cgraph-cli.dir/build

CMakeFiles/cgraph-cli.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cgraph-cli.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cgraph-cli.dir/clean

CMakeFiles/cgraph-cli.dir/depend:
	cd /home/dtkhuat/index_edge/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/dtkhuat/index_edge /home/dtkhuat/index_edge /home/dtkhuat/index_edge/build /home/dtkhuat/index_edge/build /home/dtkhuat/index_edge/build/CMakeFiles/cgraph-cli.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cgraph-cli.dir/depend

