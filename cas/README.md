# Cassette routines for transferring files to/from ABC80

*Software for transferring BASIC programs between a modern computer
and ABC80 using the tape recorder connection.*

In the early days of home computing, cassette tapes were used very
much for storage. Mainly due to the affordability of tapes and machines,
but also their wide spread use and the general understanding of
their mechanics: play, record, pause, fast forward and backward
winding.


## the principle

*At this moment when writing, I do not have yet tested this. But
from what I understand by reading and viewing similar experiments,
this should be able to work. I'll retiurn later to review and
explain more when I have the physical experience.*

The hardware setup is as follows:

```
ABC80 <-> 5 pin DIN connector [cord] 3.5 mm audio plug <-> modern computer
```

With a suitable program such as e.g. Audacity installed on your
modern computer, you can record and play sound, just as with a
cassette recorder.

In this case the handling of sound files will be through the
IBM/Microsoft [Wave](https://en.wikipedia.org/wiki/WAV) format.



## from BASIC to WAVE

So if you have a program in BASIC as text for the ABC80, you can
transfer the file by first run the program through a Python program
`uni2abc.py` to adopt it for ABC80. It will translate from Unicode
to 7 bit ASCII, but as a Swedish character set there are some
character substitutions to allow for e.g. umlauts.

Next step is to translate the newly adopted text into a Wave-file.
This time I used the experience of two previous developers Robert
Juhasz for the [original code](towave/alt/abccas/abccas.c),
and Stefano Bodrato for some [updates](towave/alt/abc80.c.txt),
to make my version in C. Through `abc2wav.c` the sound file can be
used for the final transfer to the target: ABC80.

```
sample.bas (unicode) -> [uni2abc.py] -> sample.abc -> [abc2wav.c] -> sample.wav 
```

PENDING TEXT

## from WAVE to BASIC

```
sample.wav -> [wav2bin.c] -> sample.bin -> [bin2basic.py] -> sample.bas (unicode)
```

PENDING TEXT


## References

- Andersson, Anders (red.), *ABC om BASIC*, (1979) 2. uppl., Didact, Linköping, 1980
- Finnved, Johan, *Programvaran i persondatorn i ABC80*, manuskript, Inst. för tillämpad elektronik, Kungl. Tekniska Högskolan, 1979.
- Isaksson, Anders & Kärrsgård, Örjan, *Avancerad programmering på ABC80*, Studentlitt., Lund, 1980
- Markesjö, Gunnar, *Mikrodatorns ABC: elektroniken i ett mikrodatorsystem*, 1. uppl., Esselte studium, Stockholm, 1978
