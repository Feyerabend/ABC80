![Reimagined AIR-FIGHT](assets/images/reimagined-air-fight-javascript.gif)

# ABC80 AIR-FIGHT 1981
A recreated program/code from paper trail.
Originally coded for the [ABC80](https://en.wikipedia.org/wiki/ABC_80)
(Advanced Basic Computer for the 80-ties, approx.) in 1981,
inspired by [Atari VCS/2600](https://en.wikipedia.org/wiki/Atari_2600)
[Combat #24](https://en.wikipedia.org/wiki/Combat_(Atari_2600)).

Small timeline of some microcomputers 1975-1984

* 1975
**	MITS Altair 8800
**	IMSAI 8080
* 1976
**	MOS KIM-1
**	Sol-20
**	Apple I
**	Rockwell AIM 65
* 1977
**	RCA COSMAC VIP
**	Apple II (Sweden 1978)
**	Commodore PET
**	Radio Shack TRS-80
**	Atari VCS (2600)
* 1978
**	ABC80
* 1979
**	Atari 400
**	Atari 800
* 1980
**	TRS-80 Pocket Computer
**	Sinclair ZX80
* 1981
**	Commodore VIC-20
**	Sinclair ZX81
**	Osborne 1
**	TI-99/4A
**	IBM PC (Sweden 1983)
**	Acorn BBC Micro
* 1982
**	Victor 9000
**	Sinclair ZX-Spectrum
**	Commodore 64
* 1983
**	Jupiter Ace
**	Apple Lisa
* 1984
**	Apple Macintosh
**	IBM PCjr
...

==U. Kristian Lidberg== did the main code in [BASIC](https://en.wikipedia.org/wiki/BASIC)
and I (to a very small degree) contributed. We were at the time in
the gymnasium ([Brännkyrka gymnasium](https://sv.wikipedia.org/wiki/Br%C3%A4nnkyrka_gymnasium),
Stockholm, Sweden), in 1981. As you might recognize, it is not very
consistent and flawed. We were alternating by the keyboard under some pressure to complete
this (including nights), so there are naturally a lot of those flaws. I guess at maximum we
might have done this in a week, or more probably in some days. (I also did a poster, which
was an illustration of two fighting aeroplanes.)

It was really only once ever used, during an afternoon when parents visited the school.
It also depends on two "joysticks" which were connected to the V24-port
[RS232](https://en.wikipedia.org/wiki/RS-232)
at the back, were tailor-made by us.
We built them from small hotel soap containers, where the inside had cables, switches and diodes.
They were in the same spirit as Atari made them: simple switches, and for each a "fire button".
The keyboard was really no alternative to my recollection because the keyboard roll-over was too
limiting (even if it has "N-key roll-over" 2,5 ms scanning, i.e. 30 hits per second, somehow
interrupt routine might have impacted this?) or was it that joysticks were much too fun?

Historical reflections over Combat can be found in:
Montfort, Nick & Bogost, Ian, *Racing the beam: the Atari Video computer system*, MIT Press,
Cambridge, Mass., 2009. An article (excerpt) can be found at at: http://gamestudies.org/0601/articles/montfort
which explains *Combat* in detail.

![V-24 on ABC80](assets/images/v24-small.jpeg)

AIR-FIGHT has never been published, **until now**, partly because we were probably afraid of possible
copyright infringement at the time. That's why it's also called "AIR-FIGHT" rather than "Combat (#24)"
or something to that effect.

__There might be *spelling mistakes*, as I have not tried to run this code at all.__

It has finally been put here as a remembrance of *Kristian*, as he disappeared without trace in the
mid 80'ties, never to return.

### contrib/JAVASCRIPT
A simple reimagination of the game in JavaScript. It illustrates a kind of "prototyping" in programming.
I. e. testing your ideas as "code".

### contrib/ATARI 2600/VCS
A partially implemented AIR-FIGHT, i.e. just the movement of the aeroplane in code for
[MOS 6502](https://en.wikipedia.org/wiki/MOS_Technology_6502) (6507) Atari 2600/VCS.
It might be that it has to be heavily reimplemented and changed due to the
particularities of this machine, if fully implemented. But it also easily extended as you
push each value for each player (2), and switch every other loop iteration.
It also illustrates my interpretation of the meaning of "code" and "coding".
It has only been tested at the site https://8bitworkshop.com (by Steven Hugg) on the emulator
for Atari 2600.

![Aeroplane on Atari 2600/VCS](assets/images/partial-airfight-atari-vcs.gif)

A good reference is: Hugg, Steven, *Making Games for the Atari 2600: An 8bitworkshop Book* (2016),
CreateSpace Independent Publishing Platform, 2018.

# ABC80 MUSIC 1981
Most simple sample we could come up with that made it to the presentation.

## Reference

- Andersson, Anders (red.), *ABC om BASIC*, (1979) 2. uppl., Didact, Linköping, 1980
- Hugg, Steven, *Making Games for the Atari 2600: An 8bitworkshop Book*, CreateSpace Independent Publishing Platform, 2016
- Isaksson, Anders & Kärrsgård, Örjan, *Avancerad programmering på ABC80*, Studentlitt., Lund, 1980
- Markesjö, Gunnar, *Mikrodatorns ABC: elektroniken i ett mikrodatorsystem*, 1. uppl., Esselte studium, Stockholm, 1978
- Montfort, Nick & Bogost, Ian, *Racing the beam: the Atari Video computer system*, MIT Press, Cambridge, Mass., 2009
- Wolf, Mark J. P. (red.), *The video game explosion: a history from Pong to Playstation and beyond*, Greenwood Press, Westport, Conn., 2008
