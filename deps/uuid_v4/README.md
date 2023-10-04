# uuid_v4

This is a fast C++ header-only library to generate, serialize, print and parse UUIDs version 4 variant 1 as specified in [RFC-4122].
It heavily relies on SIMD operations (instruction sets **SSE4.1**/**AVX**/**AVX2**), c\++11 <random> PRNG library and some c\++17 features.

This library generates UUIDs with pseudo-random numbers, seeded by true (hardware) random. It is *not* a cryptographically secure way of generating UUIDs.

While this lib is optimized to be fast with SIMD operations, it is possible to run it on any architecture with portable implementations of SIMD instructions like [simd-everywhere](https://github.com/simd-everywhere/simde)

## Update Notes

The namespace changed from `UUID` to `UUIDv4` to avoid a conflict with a windows.h dependency.

## Usage

### Cmake

```
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="/usr/local" ..
cmake --install .
```

Then use

```
find_package(uuid_v4)
target_link_libraries(MyProject uuid_v4::uuid_v4)
```

### Manually

Include `"uuid_v4.h"` and `"endianness.h"`.

## Documentation

To start generating UUIDs you need to create an object `UUIDv4::UUIDGenerator<random_generator>` where random_generator is a c\++11 Random number engine (see [random]).
It is highly recommended to use the default engine `std::mt19937_64` as it has a SIMD implementation (at least in libstdc++) and provides better randomness.

```c++
#include "uuid_v4"
UUIDv4::UUIDGenerator<std::mt19937_64> uuidGenerator;
UUIDv4::UUID uuid = uuidGenerator.getUUID();
```

Serializing an UUID to a byte string (16 bytes)
```c++
std::string bytes = uuid.bytes();
or
std::string bytes1;
uuid.bytes(bytes1);
or
char bytes2[16];
uuid.bytes(bytes2);
```

Pretty-printing an UUID (36 bytes)
```c++
std::string s = uuid.str();
or
std::string s1;
uuid.str(s1);
or
char s2[36];
uuid.bytes(s2);
```

Loading an UUID from a byte string (16 bytes)
```c++
UUIDv4::UUID uuid(bytes);
```

Parsing an UUID from a pretty string (36 bytes)
```c++
UUIDv4::UUID uuid = UUIDv4::UUID::fromStrFactory(string);
or
UUIDv4::UUID uuid;
uuid.fromStr(string);
```

Comparing UUIDs
```c++
UUIDv4::UUIDGenerator<std::mt19937_64> uuidGenerator;
UUIDv4::UUID uuid1 = uuidGenerator.getUUID();
UUIDv4::UUID uuid2 = uuidGenerator.getUUID();
if (uuid1 == uuid2) {
  std::cout << "1 in 10^36 chances of this printing" << std::endl
}
```

stream operations
```c++
std::cout << uuid << std::endl;
std::cin >> uuid;
```
## Benchmarks

Comparing generation time
+ Basic approach generating directly a string [basic]
+ libuuid [libuuid] uses /dev/urandom (cryptographically secure)
+ Boost UUID [boost] uses /dev/urandom (cryptographically secure)
+ Boost UUID with mt19937_64
+ UUID_v4 (this project)

|Benchmark         |        Time   |        CPU |Iterations
|------------------|---------------|------------|-----------
|Basic             |    16098 ns   |   16021 ns |     42807
|Libuuid           |   298655 ns   |  293749 ns |      2405
|BoostUUID         |    48476 ns   |  48357 ns  |    14689
|BoostUUIDmt19937  |     2673 ns   |    2665 ns |    262395
|UUID_v4           |     1117 ns   |    1114 ns |    618670


Timings of UUIDs operations, there is a scale factor on x100.
i.e UUIDGeneration takes 11.34ns to build one uuid.

Benchmark              |         Time     |      CPU |Iterations
-----------------------|-----------------|-----------|-----------
UUIDGeneration         |      1134 ns    |   1117 ns |    618589
UUIDSerializeAlloc     |      3197 ns    |   3182 ns |    214742
UUIDSerializeByRef     |       211 ns    |    211 ns |   3312380
UUIDSerializeCharArray |        64 ns    |     64 ns |  10747617
UUIDPretty             |      3424 ns    |   3415 ns |    206672
UUIDPrettyByRef        |       211 ns    |    209 ns |   3319069
UUIDPrettyCharArray    |        88 ns    |     88 ns |   7916795
UUIDLoad               |        64 ns    |     63 ns |  10837304
UUIDParse              |       320 ns    |    316 ns |   2206306
UUIDParseInPlace       |       317 ns    |    313 ns |   2222561
UUIDEqual              |        50 ns    |     49 ns |  12978765
UUIDCompare            |        65 ns    |     65 ns |  10672186


## Building

This project uses CMake to build tests and benchmarks.
If you do not have googletest and googlebenchmark installed globally
```
git clone --recurse-submodules https://github.com/crashoz/uuid_v4.git
```

If you want to run the benchmark against the other libraries you need to install them (`libuuid` and `boost`)

otherwise
```
git clone https://github.com/crashoz/uuid_v4.git
```

Then build
```
mkdir build
cd build
cmake -Dtest=ON -Dbenchmark=ON ..
cmake --build .
./tests/uuid_v4_test
./benchmarks/uuid_v4_benchmark
```

[RFC-4122]: https://tools.ietf.org/html/rfc4122
[random]: https://en.cppreference.com/w/cpp/header/random
[basic]: https://gist.github.com/fernandomv3/46a6d7656f50ee8d39dc
[libuuid]: https://linux.die.net/man/3/libuuid
[boost]: https://www.boost.org/doc/libs/1_68_0/libs/uuid/doc/index.html
