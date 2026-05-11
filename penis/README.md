# Penis

Ever wanted to code like the game penis? Penis makes this possible. It's quite simple too:
- Start with `penis` (0) or `penis!` (1)
- For each new bit, add an exclamation mark after `penis`. For instance:<br>
  `1000101` would be:
  `penis! penis!! penis!!! penis!!!! penis!!!!!! penis!!!!!!! penis!!!!!!!!!`<br>
  `test.penis` is an example with comments

That's it. The file extension is `.penis`.

## Compiler

> [!NOTE]
> To build the compiler use:
> ```sh
> gcc -O3 -march=native -flto compiler.c -o peniscc
> ```

The compiler is straightforward.
```sh
# Compile
peniscc code.penis out.txt
# or (mark explicity as compile so input can be any extension, in case you hate .penis for some reason)
peniscc -c code.txt out.bin

# Encode
peniscc in.txt out.penis
# or (mark explicitly as encode so input can be a .penis input)
peniscc in.penis out.penis

# Estimate output size
peniscc -s in.bin
# or fast estimation -- enc = 5n + 1.5 * (n(n + 1) / 2), (enc is output size, n is bit count of original file)
peniscc -s --fast in.bin
```

The compile step is aided by AVX2, which is automatically disabled if your cpu does not support it. If you'd like to not use it (for whatever reason), just add the `--no-hints` flag.

## Examples

The examples are as follows:
- [hello](./examples/hello.penis) — `penis` source file, generated from [hello](./examples/hello) (Unix executable created from [hello.s](./examples/hello.s) with `nasm -f bin hello.asm -o hello`) with `peniscc examples/hello examples/hello.penis`.
  - Running `peniscc examples/hello.penis example/hello2 \ chmod +x exaples/hello2 \ ./examples/hello2` does execute and is the same binary as [hello](./examples/hello).
- [hi](./examples/hi.penis) — `penis` source file, outputs [hi.txt](./examples/hi.txt) with `peniscc examples/hi.penis example/hi.txt`