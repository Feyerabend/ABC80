
## Samples of BASIC II

Some different versions of samples in BASIC II for ABC802 to show how it can work.

* [__SNAKE1.BAS__](SNAKE1.BAS) - more or less regular BASIC of the time, selecting and jumping by using `GOSUB` och `GOTO`.
  First attempt of making separate routines.
* [__SNAKE2.BAS__](SNAKE2.BAS) - an attempt of using `DEF FN` to replace some previous jumping. All essential variables are
  *globals*, thus no real gain of the structure were performed.
* [__SNAKE2.BAS__](SNAKE3.BAS) - some more shakeing at the code strucure, but parameters to functions `FN` only allows single 
  values when called (from variables) to be passed, thus limiting the usefulness as procedures or functions as
  we know them from (contemporary) other languages such as C or Pascal.

Simple timing with approx. seconds:
```BASIC
10 POKE 65524%,0%,0%
...
1000 PRINT "Time: ”;PEEK(65524%)+(PEEK(65525%)/100)
```

Timing which takes longer time, result in seconds:
```BASIC
10 T1$=RIGHT$(TIME$,12)
...
1000 T2$=RIGHT(TIME$,12)
1010 T=(VAL(LEFT$(T2$,2))-VAL(LEFT$(T1$,2)))*3600+VAL(RIGHT$(T2$,7))-VAL(RIGHT$(T1$,7))
1020 PRINT T+(VAL(MID$(T2$,4,2))-VAL(MID$(T1$,4,2)))*60;
```


### ..





## References

- Andersson, Anders (red.), *ABC om BASIC*, (1979) 2. uppl., Didact, Linköping, 1980.
- *Bit för bit med ABC 800*, Luxor datorer, Motala, 1984
- Isaksson, Anders & Kärrsgård, Örjan, *Avancerad programmering på ABC80*, Studentlitt., Lund, 1980.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken*, 1. uppl., Emmdata, Umeå, 1982
- Lundgren, Jan & Thornell, Sören, *BASIC II boken för ABC 802*, 1. uppl., Emmdata, Umeå, 1983
- Markesjö, Gunnar, *Mikrodatorns ABC: elektroniken i ett mikrodatorsystem*, 1. uppl., Esselte studium, Stockholm, 1978 [URL](https://www.abc80.org/docs/Mikrodatorns_ABC.pdf).
