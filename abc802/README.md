
# BENCHMARKS

## Samples of BASIC II

Some different versions of samples in BASIC II for ABC802 to show how it can work.

* [SNAKE1.BAS](SNAKE1.BAS) - more or less regular BASIC of the time, selecting and jumping by using `GOSUB` och `GOTO`.
  First attempt of making separate routines.
* [SNAKE2.BAS](SNAKE2.BAS) - an attempt of using `DEF FN` to replace some previous jumping. All essential variables are
  *globals*, thus no real gain of the structure were performed.
* [SNAKE2.BAS](SNAKE3.BAS) - some more shakeing at the code strucure, but parameters to functions `FN` only allows single 
  values when called (from variables) to be passed, thus limiting the usefulness as procedures or functions as
  we know them from (contemporary) other languages such as C or Pascal.

### Representation of numbers

Integer, Float, Double, ASCII ..


## Timing in BASIC II with ABC802

Simple timing with approx. seconds:
```BASIC
10 POKE 65524%,0%,0%
...
1000 PRINT "Time: ”;PEEK(65524%)+(PEEK(65525%)/100)
```

Timing for programs that takes a longer time, result in seconds:
```BASIC
10 T1$=RIGHT$(TIME$,12)
...
1000 T2$=RIGHT(TIME$,12)
1010 T=(VAL(LEFT$(T2$,2))-VAL(LEFT$(T1$,2)))*3600+VAL(RIGHT$(T2$,7))-VAL(RIGHT$(T1$,7))
1020 PRINT "Time: ";T+(VAL(MID$(T2$,4,2))-VAL(MID$(T1$,4,2)))*60;
```

## Benchmark results

| Program       | Mods    | Seconds         |
| ------------- | ------- | --------------- |
| NOEL.BAS      | none    | 19<sup>1</sup>  |
| NOEL1.BAS     | %       | 8<sup>2</sup>   |
| NOEL2.BAS     | SINGLE  | 26              |
| NOEL3.BAS     | DOUBLE  | 26              |
| SCRUSS2.BAS   | none    | 246<sup>3</sup> |
| MANDEL1.BAS   | % some  | 3317            |
| MANDEL2.BAS   | none    | 93              |
| MANDEL3.BAS   | % some  | 88              |

__Notes__

<sup>1</sup> Same result as BBC Micro.

<sup>2</sup> Slightly better than BBC Micro w/ second 6502 using integers (here BBC Micro: 9 sec).

<sup>3</sup> In this case a higher number the better.
Cf. *BBC BASIC* at 202, *Commodore 64 BASIC* at 100. And alas *ABC802* at 246.



### Noel[^noel]

* [NOEL.BAS](NOEL.BAS) Original test.
* [NOEL1.BAS](NOEL1.BAS) Introduce % all over.
* [NOEL2.BAS](NOEL2.BAS) First line SINGLE.
* [NOEL3.BAS](NOEL3.BAS) First line DOUBLE.

[^noel]: https://www.youtube.com/@NoelsRetroLab


### Scruss[^scruss]

I have kept to the original program as close as possible. The only thing that was changed was
the timing, as necessary. However, the restriction to a syntactical alignment is troublesome
as no one, even then in the 80's, would make a program without adopting internal speedups
in available BASIC, such as using integer where possible instead of the always slower floating
point. At least if it was known to the programmer. 

* [SCRUSS.BAS](SCRUSS.BAS) Original program.
* [SCRUSS2.BAS](SCRUSS2.BAS) Changed timing for ABC802.

I do agree with *not* using any special `POKE` e.g. turning off interrupts, using machine code
or any assembly routines. *It's not BASIC*. It doesn't measure the BASIC.

[^scruss]: https://scruss.com/blog/2020/12/17/bench64-a-new-basic-benchmark-index-for-8-bit-computers/


### Mandelbrot Set[^mandel]

A traditional benchmark for programming languages have been the Mandelbrot set. Especially when colours are
present, it shows speed in zooming and the spread of colours in the palette.

* [MANDEL1.BAS](MANDEL1.BAS) Plotted with `TX POINT`. Adopted from ZX81 sample.
* [MANDEL2.BAS](MANDEL2.BAS) Printed with characters. This seems to have been written for an emulator (e.g. some
  oddities in represented chars).[^mandel2]
* [MANDEL3.BAS](MANDEL3.BAS) Printed with characters, and somewhat optimized slightly better than the above.

[^mandel]: https://en.wikipedia.org/wiki/Mandelbrot_set
[^mandel2]: http://forum.6502.org/viewtopic.php?p=87398, and https://gitlab.com/retroabandon/bascode/-/blob/master/abc800/mandel-abc800.bas?ref_type=heads.

## References

- Andersson, Anders (red.), *ABC om BASIC*, (1979) 2. uppl., Didact, Linköping, 1980.
- *Bit för bit med ABC 800*, Luxor datorer, Motala, 1984
- Isaksson, Anders & Kärrsgård, Örjan, *Avancerad programmering på ABC80*, Studentlitt., Lund, 1980.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken*, 1. uppl., Emmdata, Umeå, 1982
- Lundgren, Jan & Thornell, Sören, *BASIC II boken för ABC 802*, 1. uppl., Emmdata, Umeå, 1983
- Markesjö, Gunnar, *Mikrodatorns ABC: elektroniken i ett mikrodatorsystem*, 1. uppl., Esselte studium, Stockholm, 1978 [URL](https://www.abc80.org/docs/Mikrodatorns_ABC.pdf).
