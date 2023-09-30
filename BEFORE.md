
# BEFORE ABC80

## A note on historical contexts

History can be written in many ways. This includes the history of technology
and computer science. One way of looking at things relates to often the context
in which something appeared. Let's take the "western" type of keyboard layout
which many of us are used to: the *qwerty* keyboard.[^qwerty]

An argument of *why* we have this might not be as interesting as the question
of why it became som *popular and wide spread*. The question of why can be
construed in hindsight (even contrafactual), and might not be the actual
reason (or in case: reasons) for why it became to be so popular. The reason,
it could be argued, the keyboard layout was due to technical reasons, as the
(maybe overlapping) typebars should not collide to often (as the often used
keys are spread out). Those of us who have typed on an ordinary typewriter
may agree to this, and draw the immediate conclusion that this must have
*caused* the layout to be the still dominant way of manufacturing keyboards today.
This historical "hunt" for such causal relationships may or may not be the
actual case, naturally. But, perhaps, rather than internal arguments, we
could look for emergence of *streams of technological innovation* where
some find their way through the changing landscape and derive larger and
larger streams, but where others stops and go no futher.
To keep with the analogy, success (or failure) might *not* have to do with the
strength of the stream itself from the start, but may very well have to do
with the form of the landscape, hills and valleys, soft and hard ground, and
add to this the constant change through time (which deviates from the impervious
landscape).

We can thus speculate in the historical success of "qwerty" as the result
of a lot of 'things' influencing its path. One of these influences can be
the technological reason mentioned above. But it not even might be
*the* cause, if such a thing actually exists.

[^qwerty]: https://en.wikipedia.org/wiki/QWERTY


## Datapoint 2200

![Datapoint patent](assets/images/datapoint-2200.jpeg)

Image from patent: https://patents.google.com/patent/USD224415S/en.
[^patent]

[^patent]: Seems to show Datapoint 1100 which had one cassette interface,
rather than two as in Datapoint 2200, as only one lid on the top is visible.


In 1970 Computer Terminal Corporation (CTC) annonced the terminal/computer
*Datapoint 2200*.[^datapoint] Later, in 1971, the company shipped the
product to customers. The rather recent video terminals[^terminal] had some
inheritance from the old teletype.[^teletype] But as many video terminals
relied on *replacing* the interface to the computer from teletype, papertape
and the like, the new Datapoint 2200 introduced more of a proper *computer*
inside the case, besides being a terminal emulating Model 33. This was
innovation. Therefore there are some claims as it was the start of the
computer that was personal, or private to the operator, i.e. a "PC".

There was several periferals that could be connected such as removable
cartridge hard disk drive, modems, priters, serial and parallel interface,
but also, later on, it was first with an 8" disk drive. But also there was
software which could independently run, not only emulate different terminal
protocols. It had its own processor, its own CPU. The idea from start was
that a CPU could be designed such that it would be possible to have the
main functions in one chip (to reduce problems with heat). CTC designed
this chip called 1201, and went to Intel[^intel] for planning ultimately
in production. Also Texas Instrument was asked to compete. However, after
some time, CTC went ahead and made the CPU by TTL-logic in hardware instead.
They were not satified with what was delivered to them. TTL-logic was thus
the hardware base when Datapoint 2200 first went into production.
At the time CTC dropped the idea of a CPU on a chip, and transferred
the design to Intel.[^datap] This happend almost concurrently with the
design of Intel 4004 for Busicom and their calculator, released in 1971.[^intelf]
At this time *memory* was the really expensive part in machines, and
therefore decisions on design and production had a different focus
than a decade later on. A CPU (or part of a CPU) on a chip would however
reduce costs as well as the heat issues, even though it were some issues
with speed in the first of designs.

The Datapoint 2200 was very popular, and had a relatively long life.
It also changed its internals over time, where the driving CPU in TTL
was soon replaced by only one chip. On the other hand it could only
reach a limited set of customers, as it was leased and not sold at
the start, exactly as IBM's products were at the time from which the
marketing model was copied.

A renewed interest in the 1201 design came up as Seiko found interest
for a scientific calculator. An improved design turned into
Intel 8008, a microprocessor that could claim to have started
the microprocessor/ microcomputer revolution. But also the lineage to its
processors for ordinary PCs of today in x86.[^xintel]
*This is one of the strains from the stream.* The fact that the core of
instruction set, or the use of little endian kept making progress for
decades to come, was the demarkation of a strong hold. A physical habit,
one might call it. Hard to overturn.

![Datapoint patent](assets/images/cw20dec1972.jpeg)

Advertisment from Datapoint (recently renamed from CTC)
in *Computerworld*, December 1972, p.12.


[^datapoint]: https://en.wikipedia.org/wiki/Datapoint_2200
and https://en.wikipedia.org/wiki/Datapoint

[^teletype]: https://en.wikipedia.org/wiki/Teleprinter

[^terminal]: https://en.wikipedia.org/wiki/Computer_terminal

[^intel]: https://en.wikipedia.org/wiki/Intel

[^datap]: https://en.wikipedia.org/wiki/Datapoint

[^intelf]: https://en.wikipedia.org/wiki/Intel_4004

[^xintel]: https://en.wikipedia.org/wiki/X86



## Seven-S

![Seven-S](assets/images/sevens.jpeg)

Image from Peter Häll - https://digitaltmuseum.se/021026362412/dator,
CC BY 4.0, https://commons.wikimedia.org/w/index.php?curid=80713670

The man behind the technological part of ABC80, Lars Karlsson, started
in 1971 his company Dataindustrier AB (approximately translated as
"Computer Industries ltd.").[^diab] The core products was for a long
time based on the bus "Data Board 4680". The numbers in the name came
from the intended support of Intel 4004, Motorola 6800 and Zilog Z80.
Besides a versatile board computer driving the bus, cards could be
added in the style of the S-100 bus to build custom computers
suitable for the intended audience of industrial applications.

In 1974 a memorandum describes ideas for a new computer, the
"Seven-S". At the end of the memorandum, the main competition
is mentioned:[^triad]

> The main competitor is Datapoint 2200 made by Computer Terminal
Corporation, USA. This unit has been on the market for about four
years and some 6000 units have been sold, of which 300 in Sweden.

> Datapoint 2200 has been a success and has open a market for a new
generation of decentralized computers. Many new brands will follow
but Seven-S will take an economical and technological lead, and
based on aggressive marketing, will reach most of the potential
markets inside and outside of Sweden.

> Seven-S will be the new weapon to fight the computer giants
like IBM, Honeywell-Bull, CDC etc.

Maybe a bit ironic is the cirumstance that DIAB (as the company
later was called), was later on bought in 1994 by Bull recently
renationalized.

The functions of the computer is described such as it could
be used for word processing, data collection, as a terminal,
remote batching, stock-keeping, inventory, personnel, salaries,
etc. suitable for then current office needs. A remark
is also that smaller companies could use this computer, as
well as larger (through the terminal function).

Also other applications are possible, programmable calculator
systems, process control, and industrial programmable
controller system are mentioned.



[^diab]: https://en.wikipedia.org/wiki/Dataindustrier_AB
[^diabsv]: https://sv.wikipedia.org/wiki/Dataindustrier_AB
[^triad]: See https://www.df.lth.se/~triad/diab/archive/Seven%20S/1974%20SevenS%20konceptstudie.pdf (probably archived by Linus Walleij).



## References

* A short history on Datapoint 2200: https://www.old-computers.com/museum/computer.asp?c=596
* More on Datapoint 2200: https://web.archive.org/web/20080819031221/http://www.computerworld.com/action/article.do?command=printArticleBasic&articleId=9111341


