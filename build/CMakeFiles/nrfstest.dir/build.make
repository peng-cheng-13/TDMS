# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /BIGDATA/nsccgz_pcheng_1/src/octopus

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /BIGDATA/nsccgz_pcheng_1/src/octopus/build

# Include any dependencies generated for this target.
include CMakeFiles/nrfstest.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/nrfstest.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/nrfstest.dir/flags.make

CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o: CMakeFiles/nrfstest.dir/flags.make
CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o: ../src/test/nrfstest.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /BIGDATA/nsccgz_pcheng_1/src/octopus/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o"
	/BIGDATA/nsccgz_pcheng_1/install/mpich/bin/mpicxx   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o -c /BIGDATA/nsccgz_pcheng_1/src/octopus/src/test/nrfstest.cpp

CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.i"
	/BIGDATA/nsccgz_pcheng_1/install/mpich/bin/mpicxx  $(CXX_DEFINES) $(CXX_FLAGS) -E /BIGDATA/nsccgz_pcheng_1/src/octopus/src/test/nrfstest.cpp > CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.i

CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.s"
	/BIGDATA/nsccgz_pcheng_1/install/mpich/bin/mpicxx  $(CXX_DEFINES) $(CXX_FLAGS) -S /BIGDATA/nsccgz_pcheng_1/src/octopus/src/test/nrfstest.cpp -o CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.s

CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.requires:
.PHONY : CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.requires

CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.provides: CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.requires
	$(MAKE) -f CMakeFiles/nrfstest.dir/build.make CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.provides.build
.PHONY : CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.provides

CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.provides.build: CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o

# Object files for target nrfstest
nrfstest_OBJECTS = \
"CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o"

# External object files for target nrfstest
nrfstest_EXTERNAL_OBJECTS =

nrfstest: CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o
nrfstest: CMakeFiles/nrfstest.dir/build.make
nrfstest: /usr/lib64/libcrypto.so
nrfstest: libnrfsc.so
nrfstest: /usr/lib64/libcrypto.so
nrfstest: CMakeFiles/nrfstest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable nrfstest"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/nrfstest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/nrfstest.dir/build: nrfstest
.PHONY : CMakeFiles/nrfstest.dir/build

CMakeFiles/nrfstest.dir/requires: CMakeFiles/nrfstest.dir/src/test/nrfstest.cpp.o.requires
.PHONY : CMakeFiles/nrfstest.dir/requires

CMakeFiles/nrfstest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/nrfstest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/nrfstest.dir/clean

CMakeFiles/nrfstest.dir/depend:
	cd /BIGDATA/nsccgz_pcheng_1/src/octopus/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /BIGDATA/nsccgz_pcheng_1/src/octopus /BIGDATA/nsccgz_pcheng_1/src/octopus /BIGDATA/nsccgz_pcheng_1/src/octopus/build /BIGDATA/nsccgz_pcheng_1/src/octopus/build /BIGDATA/nsccgz_pcheng_1/src/octopus/build/CMakeFiles/nrfstest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/nrfstest.dir/depend

