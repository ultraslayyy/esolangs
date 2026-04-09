# Penis

[Esolangs page](https://esolangs.org/wiki/Penis)

Ever wanted to code like the game penis? Penis makes this possible. It's quite simple too:
- Start with `penis` (0) or `penis!` (1)
- For each new bit, add an exclamation mark after `penis`. For instance:<br>
  `1000101` would be:
  `penis! penis!! penis!!! penis!!!! penis!!!!!! penis!!!!!!! penis!!!!!!!!!`<br>
  `test.penis` is an example with comments

That's it. The file extensions are `.penis`, `.peen`, or `.peenar`.

## Compiler

> [!NOTE]
> To build the compiler just use
> ```sh
> gcc compiler.c -o peniscc
> ```

The compiler is straightforward.
```sh
# Compile
peniscc code.penis out.txt
# or (mark explicity as compile so input can be any extension)
peniscc -c code.txt out.bin

# Encode
peniscc in.txt out.penis
# or (mark explicitly as encode so input can be a .penis input)
peniscc -e in.penis out.penis

# Estimate output size
peniscc -s in.bin
```

## Examples

The examples are as follows:
- [hello](./examples/hello.penis) — `penis` source file, generated from [hello](./examples/hello) (Unix executable created from [hello.s](./examples/hello.s) with `nasm -f bin hello.asm -o hello`) with `peniscc examples/hello examples/hello.penis`.
  - Running `peniscc examples/hello.penis example/hello2 \ chmod +x exaples/hello2 \ ./examples/hello2` does execute and is the same binary as [hello](./examples/hello).
- [hi](./examples/hi.penis) — `penis` source file, outputs [hi.txt](./examples/hi.txt) with `peniscc examples/hi.penis example/hi.txt`
