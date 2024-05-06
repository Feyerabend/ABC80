
![ABC802](../assets/images/abc802-front.jpg)

# BENCHMARKS

I'm not really a fan of benchmarks. I find them too restrictive in the way they only measure what they measure.
That is, they only could give you an estimate of something (that they measure), not an evaluation of what the
machine can do in general. It doesn't indicate the possibilities, they only convey a test of something.
Mostly is has been for speed ..

Today when almost all interests on 'retro computers' are in the spotlight of how and if they are able to play
games, and in this area the ABC802 can not compete, benchmarks shows other aspects.

Besides that, they can be fun to look at. The following are benchmarks that have been done through the early
years of personal computing, when programming in BASIC was the primary choice. It was the time when the BASIC
came in ROMs, and basically was the computer software/operating system. But also there are some new retro
community contributions of benchmarks, which are fun.

The ABC802, which is the focus here, was a follow-up to the ancestor ABC80. It was almost the same computer,
but had an improved BASIC, the BASIC II. The BASIC II was used in the similar machines ABC800 and ABC806,
but also there was a version made for PC-DOS/MS-DOS, intended for easier transitioning to the future PC-line
of computers.

Even if the IBM PC first was launced with a *BASIC in ROM*, the ABC802 also marked the end of an era started
with SOL, Apple II, Commodore PET, TRS-80, and the like. The instant BASIC at startup had passed its prime
time. This was true for the *business market* where ABC802 belonged, when you either made your own program
or bought from it on the market from small developers, small firms. That was about to change. On the other
hand there was a compleatly different *home computer* domain in which BASIC in ROM right at this time rised
and thrived, where it stayed for some time longer and much stronger, almost into the next decade. Hand in
hand with the rise and fall of *BASIC in ROM* was the *cassette tape recoder/player* used as data/program
storage. If you think about it .. not that surprising. The diskette, and eventually the hard drive, changed
the personal computer to using a *disc operating system* (yeah, DOS) as a focus point, instead.

But as can be witnessed below, BASIC II in __ABC802__ was quite fast for its time. Often very near (below or
above) the highly prasied the BBC BASIC of the __BBC Micro__. It also had the possible choice between different
number types in trade-off between speed and precision. Thus making it versatile in many applications.

### ABC802

Some relevant specs, introduced in Feb. 1983:
* Z80 3 Mhz
* 24k ROM BASIC II (made by DIAB AB)
* 32k RAM (+ additional 32k RAM-disc, or combined let you have 64k CP/M)


## Samples of BASIC II

Some different versions of samples in BASIC II for ABC802 to show how it can work.

* [SNAKE1.BAS](SNAKE1.BAS) - more or less regular BASIC of the time, selecting and jumping by using `GOSUB` och `GOTO`.
  First attempt of making separate routines. Sample of 'spagetti code'.
* [SNAKE2.BAS](SNAKE2.BAS) - an attempt of using `DEF FN` to replace some previous jumping. All essential variables are
  *globals*, thus no real gain of the structure were performed.
* [SNAKE3.BAS](SNAKE3.BAS) - some more shakeing at the code strucure, but parameters to functions `FN` only allows for
  single values when called (from variables) to be passed, thus limiting the usefulness as procedures or functions as
  we know them from (contemporary) other languages such as C or Pascal. Even though this may count as 'structured BASIC'
  the language still has too weak constructs, or in itself is not sufficient.


### Representation of numbers

Integer, Float, Double, ASCII ..
Something noticeable about the built-in number types in BASIC II is that there is a clear trade-off between
precision and speed, which is probably one of the best reasons for the different types offered.


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

### Kilobaud 1977, Personal Computer World 1978, Mikrodatorn 1982, Hobbydata 1984[^rugg]

One of the earliest comprehensive benchmarks for primarily home computing (but also occationally
larger machines) came in 1977 with Rugg-Feldman samples in 1977 *Kilobaud* magazine. Later *Personal
Computer World* added a benchmark in 1978, below 'B8.BAS'.

* [B1.BAS](B1.BAS)
* [B2.BAS](B2.BAS)
* [B3.BAS](B3.BAS)
* [B4.BAS](B4.BAS)
* [B5.BAS](B5.BAS)
* [B6.BAS](B6.BAS)
* [B7.BAS](B7.BAS)
* [B8.BAS](B8.BAS) PCW Feb. 1978

The samples built most on each other progressively towards more advanced. Later two
Swedish magazines extended the list with other computers, unsurprisingly from the
ABC-line.

[^rugg]: https://en.wikipedia.org/wiki/Rugg/Feldman_benchmarks


### Creative Computing 1984 Ahl's Simple Benchmark

* [AHLS.BAS](AHLS.BAS) This is the only benchmark among the selected ones here, that also gives a hint
  about *accuracy* in number crunching, beside speed.


### Interface Age 1980 Prime Numbers
..


### BYTE 1981

* ...


### FizzBuzz[^fizzbuzz]

Perhaps a bit odd to introduce FizzBuzz as a benchmark. But why not? It is easy
to change and adopt to different flavours of BASIC.

* [FIZZBUZZ.BAS](FIZZBUZZ.BAS) Original program. Increased to 1000 instead of 100,
  shows smaller diviations more explicit.
* [FIZZBUZ1.BAS](FIZZBUZ1.BAS) Introduce % everywhere.
* [FIZZBUZ2.BAS](FIZZBUZ2.BAS) SINGLE.
* [FIZZBUZ3.BAS](FIZZBUZ3.BAS) DOUBLE.

[^fizzbuzz]: https://en.wikipedia.org/wiki/Fizz_buzz


### Noel[^noel]

* [NOEL.BAS](NOEL.BAS) Original test.
* [NOEL1.BAS](NOEL1.BAS) Introduce % all over.
* [NOEL2.BAS](NOEL2.BAS) SINGLE.
* [NOEL3.BAS](NOEL3.BAS) DOUBLE.

[^noel]: https://www.youtube.com/@NoelsRetroLab


### Scruss[^scruss]

I have kept to the original program as close as possible. The only thing that was changed was
the timing, as necessary. However, the restriction to a syntactical alignment is troublesome
as no one, even then in the 80's, would make a program without adopting internal speedups
in available BASIC, such as using integer where possible instead of the always slower floating
point. At least if it was known to the programmer. 

* [SCRUSS.BAS](SCRUSS.BAS) Original program. Can not be used as is.
* [SCRUSS2.BAS](SCRUSS2.BAS) Changed for timing in ABC802.

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


## Summary benchmark results

| Program       | Mods    | Seconds         |
| ------------- | ------- | --------------- |
| B1.BAS        | none    | ?               |
| B2.BAS        | none    | ?               |
| B3.BAS        | none    | ?               |
| B4.BAS        | none    | ?               |
| B5.BAS        | none    | ?               |
| B6.BAS        | none    | ?               |
| B7.BAS        | none    | ?               |
| B8.BAS        | none    | ?               |
| FIZZBUZZ.BAS  | none    | ?               |
| FIZZBUZ1.BAS  | %       | ?               |
| FIZZBUZ2.BAS  | SINGLE  | ?               |
| FIZZBUZ3.BAS  | DOUBLE  | ?               |
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

<sup>2</sup> Slightly better than BBC Micro using integers (here BBC Micro at 9 sec).

<sup>3</sup> In this case a higher number the better.
Cf. *BBC BASIC* at 202, *Commodore 64 BASIC* at 100. And alas *ABC802* at 246.




## References

- Andersson, Anders (red.), *ABC om BASIC*, (1979) 2. uppl., Didact, Linköping, 1980.
- *Bit för bit med ABC 800*, Luxor datorer, Motala, 1984.
- Isaksson, Anders & Kärrsgård, Örjan, *Avancerad programmering på ABC80*, Studentlitt., Lund, 1980.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken*, 1. uppl., Emmdata, Umeå, 1982.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken för ABC 802*, 1. uppl., Emmdata, Umeå, 1983.
- Markesjö, Gunnar, *Mikrodatorns ABC: elektroniken i ett mikrodatorsystem*, 1. uppl., Esselte studium, Stockholm, 1978 [URL](https://www.abc80.org/docs/Mikrodatorns_ABC.pdf).
