# shush::stack
A stack implementation with increased security provided by layers of protection: storing hash of the entire stack buffer, having protection values around borders of allocated space and filling uninitialized data with `poison` values. All of this uses nice logger and dump systems.

## Build
```shell
mkdir build && cd build
cmake .. # "-UBUILD_TESTS -DBUILD_TESTS=ON" to build tests
make
```

## How to use THE LIBRARY
Download the repository and place it into your project directory. Don't forget to `git submodule update <submodule>` all necessary submodules. Change the target name of one of shush-formats in submodules so that you can actually link them (or use another method of compiling, bit this particular seems easier). In your project's CMakeLists.txt file, insert the following lines:
```cmake
...
add_subdirectory(shush-logs)
add_subdirectory(shush-dump)
...
target_link_libraries(${PROJECT_NAME} shush-logs)
target_link_libraries(${PROJECT_NAME} shush-dump)
...
```

## How to use THE EXECUTABLE
```shell
cd build
./PROJECT_NAME
```

## Build documentation
```shell
doxygen
```
Documentation will be generated in the directory named `docs`.