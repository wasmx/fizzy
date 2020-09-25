# Fizzy TestFloat

The `fizzy-testfloat` is a CLI tool that cooperates with the [Berkeley TestFloat] floating-point
implementation test suite and tools. 

The TestFloat's `testfloat_gen` generates operand values and expected result for selected
floating-point instruction. Then this generated input is processed by `fizzy-testfloat` to check if
Fizzy's implementation of the matching WebAssembly instruction produces the same result.

For more information check `fizzy-testfloat --help` and `testfloat_gen --help`.

## Examples

1. Test `f32.add` with default rounding mode
   ```shell script
   testfloat_gen f32_add | fizzy-testfloat f32_add
   ```
2. Test `f32.add` with rounding down (not WebAssembly compliant)
   ```shell script
   testfloat_gen -rmin f32_add | fizzy-testfloat -rmin f32_add
   ```
3. Test `f32.trunc`
   ```shell script
   testfloat_gen -rminMag f32_roundToInt | fizzy-testfloat f32_trunc
   ```


[Berkeley TestFloat]: http://www.jhauser.us/arithmetic/TestFloat.html
