# Cassette routines for transferring files to/from ABC80

*Software for transferring BASIC programs between a modern computer
and ABC80 using the tape recorder connection.*

In the early days of home computing, cassette tapes were used
very much for storage. Mainly due to the affordability of tapes
and machines, but also their wide spread use and the general
understanding of their mechanics: *play*, *record*, *pause*, *fast*
*forward* and *backward* winding.

When you bought a brand new ABC80 as a package, there was a special
monitor including the power supply for the computer, and a
special tape recorder. As a company that usually manufactured
receivers, stereo equipment, TVs etc. Luxor could naturally also
provide the recorder: ABC820.

![Kassettminne](../assets/images/kminne.jpg)


## The principle

*At this moment when writing, I do not have yet tested this. But
from what I understand by reading and viewing similar experiments,
this should be able to work. I'll return later to review and
explain more when I have the physical experience.*

![Kassettminne - ABC80](../assets/images/connect.jpg)

The original hardware has a cable connection of three wires, one for
input, one for output and one for ground. At each end it has a 5 pol.
DIN male connector. It also have a motor control cable that acts as
control from the computer for stopping the motor and starting when
the tape is played.

The new hardware setup is roughly as follows:

```
ABC80 <-> 5 pin DIN connector [cord] 3.5 mm audio plug <-> modern computer
```

Or, other suitable connection to your computer.
The important thing is obviously how the wires are connected.

With a program such as e.g. [Audacity](https://www.audacityteam.org/)
installed on your modern computer, you can record and play
sound, just as with a cassette recorder. The computer thus
replaces the recorder.

In this case the handling of sound files will be through the
IBM/Microsoft [Wave](https://en.wikipedia.org/wiki/WAV) format.

Requirements for these programs below is to install Python
(preferably any 3-version) and to compile the C-program.
You can start compiling through the tool `make` which
produces an object-file (.o) and a library linked executable.

The decision to do this into two separate steps are mostly due to
following the previous programs in C for BASIC to WAVE, and then
later on adding a Python-script. Python is much more concise and
powerful for these types of tasks, and you might consider to
rewrite everything into Python.


## From BASIC to WAVE

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

### Converting files

Prerequisites: install or make sure you have `Python3` and `gcc`
(or equivalent) and optional `make`. Compile 'abc2wav.c' to an
executable.

```
> Python3 uni2abc.py -i sample.bas -o sample.abc
> make abc2wav
> ./abc2wav -i sample.abc -o sample.wav
```


## From WAVE to BASIC

For going the other way around, from the produced wave file to BASIC,
there is a bit of "parsing" once the binary encoding has been decoded.

```
sample.wav -> [wav2bin.c] -> sample.bin -> [bin2basic.py] -> sample.bas (unicode)
```

PENDING TEXT

### Converting files

Prerequisites: install or make sure you have `Python3` and `gcc`
(or equivalent) and optional `make`. Compile 'wav2bin.c' to an
executable.

```
> make wav2bin
> ./wav2bin -i sample.wav -o sample.bin
> Python3 bin2basic.py -i sample.bin -o sample.bas
```


## References

- Andersson, Anders (red.), *ABC om BASIC*, (1979) 2. uppl., Didact, Linköping, 1980
- Finnved, Johan, *Programvaran i persondatorn i ABC80*, manuskript, Inst. för tillämpad elektronik, Kungl. Tekniska Högskolan, 1979.
- Isaksson, Anders & Kärrsgård, Örjan, *Avancerad programmering på ABC80*, Studentlitt., Lund, 1980
- Markesjö, Gunnar, *Mikrodatorns ABC: elektroniken i ett mikrodatorsystem*, 1. uppl., Esselte studium, Stockholm, 1978 [URL](https://www.abc80.org/docs/Mikrodatorns_ABC.pdf)
