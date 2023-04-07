# Barely functional emulator for ABC80

There are naturally several emulators for ABC80. But this one
may be the simplest, although barely functional.
It lacks most features, graphics, sound etc. but it shows how
simple programs (text based) can work. Starting from a simple
Z80 emulator, the program copies what is in the screen memory
and recognizes some keys as input.
Built upon "tinyz80"[^tiny] in C it illustrates some simple
programming concepts such as a sample:

[^tiny]: https://github.com/kspalaiologos/tinyz80

```basic
10 ; "Hejsan ";
20 GOTO 10
```

![Running the emu.](../assets/images/abc80emu.gif)


## Compile and run

To compile you need `cmake` as we use `ncurses` for display and keyboard.
Create a folder `build` in parallel to (same parent as) `src`. From the
`build` type:

```sh
> cmake ../
> make
> bin/abc
```


## License

As tinyz80 is licensed with a "greedy" version 3 of GNU, every
file that works with it must follow the same license.

*The `abcprom.h` file however is data and __not__ part of the program.
Dataindustrier AB (DIAB) gave Jonas Yngvesson permission to distribute
the PROM-contents. Jonas Yngvesson also made a proper simulator/emulator
for ABC80.*
