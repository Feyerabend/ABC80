
## ABC80 cassettes / ABC80-kassetter

Tools for moving programs between ABC80 cassettes and ordinary files.
Verktyg för att föra program mellan ABC80-kassetter och vanliga filer.

```
                      tobasic/                                 towave/
 tape.wav    --wav2basic.py--> X.BAS (text) <--uni2abc.py-- text.txt
                         └---> X.BAC --bac2bas--> X.BAS
 tape.wav    <-----------------------abc2wav--------------- X.BAS / X.BAC
```

| Directory / Katalog | Contents / Innehåll |
|---------------------|---------------------|
| [`tobasic/`](tobasic/README.md) | WAV → files (`wav2basic.py`); BAC → BASIC text (`bac2bas`, ANSI C, a parallel of the ABC80 ROM's LIST) / WAV → filer; BAC → BASIC-text |
| [`towave/`](towave/README.md) | text → ABC80 text (`uni2abc.py`); file → WAV (`abc2wav`) / text → ABC80-text; fil → WAV |


### English

Each tool does one step and can be used on its own. They can also be
joined with pipes, for example:

```sh
./bac2bas < X.BAC | less
```

Build with `make` in each directory. `wav2basic.py` needs Python 3 and
numpy. `bac2bas` is plain ANSI C (C89).


### Svenska

Varje verktyg gör ett steg och går att använda för sig. Det går också att
koppla ihop dem med rör, till exempel:

```sh
./bac2bas < X.BAC | less
```

Bygg med `make` i respektive katalog. `wav2basic.py` behöver Python 3 och
numpy. `bac2bas` är ren ANSI C (C89).

S. Lonnert 2023, 2026.
Public domain.
