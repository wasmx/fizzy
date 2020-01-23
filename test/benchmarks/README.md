# Fizzy Benchmarking Suite

## Inputs files

For each wasm file there may exist an `.inputs` file matching the `.wasm` file
name.

The inputs file is needed to benchmark execution and multiple execution cases may be
specified in the file.

Execution case is specified by 6 lines:

1. The case name. Must not be empty.
2. The exported wasm function name to be executed.
3. The function arguments as space-separated list of integers. May be empty.
4. The hex-encoded bytes of the initial memory. May be empty.
5. The expected result as an integer. Empty line means no result is expected.
6. The hex-encoded bytes of the expected memory after execution. 
   If empty, result memory will not be checked.

Additional empty lines are allowed before each case.

### Example

```
case_1
testFunction
2 3 4
000000ff
1
ff000000ff000000

case_2
memset
0 0xfe 3


fefefe
```
