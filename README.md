# osal
C++11 OS Abstraction Layer

### Install test dependencies
```sh
conan install . -if <build dir>
```

### Build
```sh
# Configure with tests
cmake -B <build dir> -S .

# Configure without tests
cmake -B <build dir> -S . -DOSAL_TEST=NO

# Build
cmake --build <build dir>
```
