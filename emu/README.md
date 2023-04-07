# Barely functional emulator for ABC80

There are naturally several emulators for ABC80. But this one
may be the simplest, although barely functional.
Built upon "tinyz80": https://github.com/kspalaiologos/tinyz80
it illustrates some simple concepts such as:

```basic
10 ; "Hejsan ";
20 GOTO 10
```

![Running the emu.](../assets/images/abc80emu.gif)

## Compile and run

...

## License

As tinyz80 is licensed with a "greedy" version of GNU 3.0, every
file that works with it must follow the same license.

The `abcprom.h` file however is data and *not* part of the program.
Dataindustrier AB (DIAB) gave Jonas Yngvesson permission to distribute
the PROM-contents. Jonas Yngvesson also made a proper simluator/emulator
for ABC80.
