
![Emulated CP/M](../assets/images/cpm.png)

# CP/M

In the late 1970s and early 1980s, many contemporary computer manufacturers
expanded their proprietary solutions by incorporating CP/M[^cpmwiki] capabilities,
either through add-on cards or by redesigning their machines. This trend
emerged as CP/M gained market traction. For instance, the Apple II could
run CP/M software using third-party Z80 processor cards, such as the Microsoft
SoftCard[^softcard]. Similarly, the Commodore 128 included a Z80 processor, enabling
it to run CP/M alongside its native operating systems.[^Commodore128]

Additionally, some systems came with CP/M pre-installed from the start.
Notable examples include the Osborne 1, an early portable computer that
bundled software like WordStar and SuperCalc, and the Kaypro II. These
integrations highlighted CP/M's versatility and widespread adoption during
that era.

*Not to be unfashionable, the ABC802 did also came prepared for a possible
extension of running CP/M.*[^cpmabc800]

[^cpmwiki]: Elementary on CP/M: https://en.wikipedia.org/wiki/CP/M.
[^softcard]: Z80 SoftCard: https://en.wikipedia.org/wiki/Z-80_SoftCard.
[^Commodore128]: Commodore 128: https://en.wikipedia.org/wiki/Commodore_128
[^cpmabc800]: Almost the same computer, CP/M for ABC800:
https://www.abc80.net/archive/luxor/sw/CPM/MYAB-Bruksanvisning-for-cpm-pa-ABC800.pdf


### De facto standardisation

Developed by Gary Kildall (Digital Research Inc.), CP/M (Control Program
for Microcomputers, Control Program / Monitor) is one of the seminal operating
systems in the history of personal computing. CP/M began in 1973 when Gary
Kildall wrote the initial version to run on Intel 8080/85-based microcomputers.
Officially released in 1974, CP/M quickly became one of the first widely
used de facto operating systems for personal computers.

In the pre-CP/M era, early microcomputers lacked standardised operating
systems. Probably it could not be fitted in them. Sometimes they had though
"monitors", early basic communications with the system for debugging, status etc.
Users often had to write their own software to interact more directly with
the hardware, which was a significant barrier to wider adoption and usability.
By providing a de facto standardised operating system that could run on various
hardware platforms, CP/M played a pivotal role in the growth of the personal
computer market during the late 1970s and early 1980s. It enabled software
developers to write programs that could be distributed widely and run on a
variety of machines, paving the way for a burgeoning software industry.

CP/M featured a simple, yet effective file system and command-line interface,
which was user-friendly for its time. One significant strength was its design,
which made it easily adaptable to various hardware platforms. This portability
was a crucial factor in its widespread use. CP/M utilised a BIOS
(Basic Input/Output System) to handle hardware-specific operations.
This abstraction layer enabled the operating system to run on different
machines with minimal modifications, enhancing its portability and flexibility.
The BIOS is also one of the features that translated well to the future MS-DOS era.

CP/M fostered a rich ecosystem of software applications and development tools.
Popular software like WordStar (word processing) and dBASE II (database management)
were first developed for CP/M. Microsoft's MS-DOS, which became the standard
operating system for IBM PCs, was heavily influenced by CP/M. The initial version
of MS-DOS (also known as 86-DOS or QDOS) was even designed to be compatible with
(legacy) CP/M software, facilitating an easier transition for users and developers.


On the hardware side, there was also a development. The S-100 bus, originally
known as the Altair bus, was introduced in 1974 with the MITS Altair 8800, one of
the first commercially successful personal computers. Initially designed for the
Intel 8080 microprocessor, it was later adapted for other processors, including the
Zilog Z80 and the Intel 8085. The bus consisted of a 100-pin edge connector, providing
a standardised interface for attaching CPU, memory, and peripheral cards. Users could
easily add or replace components to upgrade their systems, which promoted a thriving
third-party hardware market. The standardisation of the bus allowed components from
different manufacturers to work together.

*It is important to note that CP/M and the S-100 bus were not dependent on each other.
They were separate standards, each contributing independently to the diverse and
varied development of the personal computer market.*


### Decline

The rise of CP/M and the S-100 bus standard occurred roughly in the same time span,
creating a synergistic environment where microcomputer users could benefit from a
standardised software platform *CP/M* and a standardised hardware interface *S-100*.
This combination contributed to the growth of the personal computer industry by making
it somewhat easier for users to expand and customise their systems and for developers
to create compatible hardware and software.

But eventually these standards had their drawbacks, as time progressed. The S-100 bus
large and complex backplanes and cards made systems bulky and costly to design and maintain.
The parallel bus architecture was prone to electrical noise and signal integrity issues,
leading to potential data corruption and system instability, as more cards were added.
High power consumption due to its wide range of supported voltages required robust power
supplies and cooling solutions, further increasing costs. Despite being a standard,
variations in implementation among manufacturers caused compatibility issues. The bus
also suffered from relatively slow data transfer speeds, becoming a performance bottleneck.
Additionally, the connectors were not robust, leading to mechanical issues and the need
for frequent maintenance. As newer, more efficient bus standards like the ISA (internal
16-bit of IBM PC/AT) emerged, the S-100 bus quickly became outdated, struggling to keep
up with evolving hardware capabilities.

CP/M, originally designed for 8-bit microprocessors, had several drawbacks that limited its
effectiveness as computing technology advanced. Its memory management capabilities were
restricted, allowing it to address only up to 64KB of memory. The absence of built-in
networking capabilities hindered its usefulness in networked environments, becoming a
significant limitation as businesses and individuals began connecting computers in local
area networks (LANs) and using modems for remote communication -- although there were
applications running e.g. terminal emulations. CP/M's command-line interface,
though functional, was basic and started to feel outdated as graphical user interfaces
like those in e.g. the Apple Macintosh and later Microsoft Windows emerged, offering
more intuitive and visually appealing experiences. Additionally, CP/M struggled to support
emerging hardware technologies such as high-resolution graphics, advanced sound systems,
and sophisticated input devices, making it less appealing for multimedia applications.
Its single-tasking nature, which allowed only *one program to run at a time*, became a
significant drawback as multitasking grew in importance for productivity and user
convenience. Furthermore, the existence of many variations of CP/M created by different
hardware manufacturers led to compatibility issues, with software written for one version
often not running correctly on another. These limitations collectively contributed to
CP/M's decline as a previous dominant operating system.

#### Summary

IBM’s decision to use MS-DOS for its PCs was a major blow to CP/M. IBM PCs quickly
became the industry standard, and MS-DOS emerged as the dominant operating system.
It is important to note that IBM also offered CP/M and UCSD Pascal as alternatives
to MS-DOS, but the pricing for CP/M was significantly higher. The saying
'You couldn't go wrong with buying an IBM,' originally used when considering
mainframes, underscored the reliability and growing popularity of the IBM PC.
Even though PC-DOS / MS-DOS was built on a 'clone' of CP/M, it evolved and laid the
foundation for a large developer market.

*This shift created a competitive software market for the PC which stood
for a standard in hardware and software, where previously proprietary individual
computer brands / computers with proprietary solutions, had fought for dominance.*


### Alternative routes

Computer manufacturers faced critical decisions about their future direction.
The importance of the PC was evident, but the extent of its impact and the
direction of the market were uncertain. Key questions included: Where was
the technology heading? How crucial was legacy support?

*Business as Usual*: Some manufacturers adopted a "business as usual" approach,
assuming that their customers would remain satisfied with whatever they produced
and innovated. This group often targeted the low-end market, where affordability
and entertainment, such as gaming, were key factors. They produced cheaper
computers that fit well within this (rather large) niche.

*Partial Compatibility*: Another group chose to create "clones" of the IBM PC with
a twist. These manufacturers, including local and national companies like Nokia
and Ericsson, made PCs that were not fully compatible with the original IBM PC.
They aimed to offer a superior product by adding unique features or improvements,
hoping for customer loyalty. However, they often miscalculated the market risks
and faced high costs, which led to their decline within a few years.

*Full Compatibility*: The third group understood the critical importance of full
compatibility with the IBM PC. They focused on reverse engineering, including
replicating the BIOS, to ensure that their PCs were entirely compatible with
IBM PCs. This approach allowed them to capitalize on the existing software
ecosystem and meet the demands of users who required seamless compatibility.
Companies like Compaq successfully adopted this strategy, becoming significant
players in the market by offering reliable IBM PC clones.

But there was at least one group more: they bet on *UNIX* as the future.[^Xenix]
[^Xenix]: Among them were in fact Micorsoft with *Xenix*.


### UNIX

Unix was developed in the late 1960s and early 1970s at AT&T's Bell Labs by
Ken Thompson, Dennis Ritchie, and some others. That is years before CP/M.
Initially, it was a small, flexible operating system for the prevalent and
current *minicomputers* (most famously the PDP/11). Thus it was aimed at
significantly different computers than CP/M. But Unix quickly gained popularity
in academic and research institutions due to its portability, powerful
features, and the availability of its source code for educational purposes.

In the late 1970s, AT&T began licensing Unix to commercial vendors, which
led to a proliferation of Unix versions. Different vendors developed their
own variants, leading to extreme *fragmentation*.

One significant variant was the Berkeley Software Distribution (BSD),
developed at the University of California, Berkeley. BSD introduced many
enhancements and became a foundation for later Unix systems. But they also
had license fees to be paid to AT&T for some Unix software.

In the years to come, overcoming *fragmentation* was a significant issue,
perhaps the largest of them all. Besides technological challenges, licensing
restrictions and the use of the Unix brand also hindered companies, development,
and market expansion.



### The proliferation of standards, ABCNIX or D-NIX, among them

The rise of multiple standards, derivations of Unix in a lot of *nix,
exemplifies a trend towards diverse platforms as well as fragmentation.
The creators of ABC802 and its variants at DIAB AB believed that
developing a Unix-based system was the next logical step in
technological evolution. In this they were not alone. They further
introduced their own “ABCNIX,” named after the renowned ABC computer series.
This system, alternatively known as D-NIX, was even licensed or possibly
just rebranded as *Cromix* for Cromenco.[^D_NIX]
D-NIX was used in computers such as ABC1600[^ABC1600] and ABC9000[^ABC9000]
introduced in 1984/1985.

Here somewhere the start of what to become the "Unix wars" started, a period
of competition and fragmentation continuing into the 1990s. This era was
characterized by disputes among various Unix vendors and standards bodies,
leading to the proliferation of different Unix variants and standards.

In addition to the fragmentation and proliferation of incompatible Unix
flavors, there were some achievements in standardization efforts such as
POSIX and somewhat of the Single UNIX Specification (SUS). However,
rivalry among vendors, intense competition, legal disputes over licensing,
and ongoing compatibility issues continued to pose significant challenges.
Competition often led to proprietary extensions, which further contributed
to fragmentation.

That history, however, extends far beyond the lifespan of the ideas surrounding
the ABC802 and its relatives.

[^D_NIX]: https://sv.wikipedia.org/wiki/D-NIX
[^ABC1600]: https://sv.wikipedia.org/wiki/ABC_1600
[^ABC9000]: https://sv.wikipedia.org/wiki/DIAB_Serie_90


## Comments on operating systems and legacy

*An operating system is a convenient way of handling a computer. Contrary
to common opinion: It is not a necessity. As can be seen from history,
rudimentary system control and basic support for hardware can go a long way.
ABC802 had some control in keyboard and screen routines, timing and the like,
but also e.g. some additional fundamental routines for printers or for DOS
in ROM. We could do what we could call “elementary” computer work such as word
processing, accounting, or register/database management.*

*Today we can emulate CP/M in a Raspberry Pi Pico, a very cheap microcontroller,
cheaper than lunch. Not only the processor Z80 is emulated, but also the terminal
interface to the screen and keyboard. It really shows that software or as a clear
sample: word processing, has not evolved at the same pace as the hardware has.
WordStar is not that far from Word. Some very elementary software can be run on
every elementary hardware, not using a complex system and overly sophisticated
hardware.*

*Historically operating systems have not been prevalent in game consoles or word
processors. They have managed anyway. They often have had special hardware.
Operating systems have occurred, but when there is no need for extra software,
layers only get in the way of reaching the raw metal in the case of games.
But as games are written by third party developers, a familiar environment
and tools are easier and probably more efficient to work with (read:
conventional operating system in a PC), than special hardware and new
development tools in a new game console.*

*This leads us to the question of legacy. Not only is the familiarity of the system
from the user highly valuable, but also the possibility of using previous software.
Backward compatibility is gold in this context. These are also some reasons why
hardware for PC’s have evolved in a very evolutionary way, in contrast to a revolutionary
one. Much of the software developed some time ago can still be run on software years
later. Binary compatibility of programs, and implied processor legacy keeps the computer
relevant. E.g. processors for PC’s x86 internally run in a more modern RISC fashion,
although the instruction set looks like it is decades old CISC from the viewpoint
of the program.*


### A new proposal

*Legacy systems significantly restrict the mainstream evolution of consumer computers,
imposing tight boundaries on innovation. The operating system, with its multiple layers
of abstraction, slows down the interaction between programs and hardware.
Is there a better way? I believe so.*

*One solution could be for software vendors to also develop hardware, effectively
bridging the gap between the two. Standards could emerge organically or be established
by a unified body that defines how hardware components should communicate, akin to
a standardized “bus.” Instead of relying on __APIs__ to mediate between programs, the
operating system, and at last hardware, development could be guided by __protocols__
that facilitate direct and efficient communication. This can be done either between
hardware to hardware through the "bus", or through software to software, or again
directly beween software and hardware, bypassing multiple layers of abstraction.*

*One solution could be for software vendors to also develop hardware, thereby bridging
the gap between the two. Standards could emerge either organically or through a unified
body that dictates how hardware components should communicate (similar to a standardized
“bus”). Instead of relying on APIs from program to operating system and down to hardware,
development could be guided by protocols either between software parts or hardware,
ensuring more direct and efficient communication between software and hardware. Not
through layers. Data obviously should be standardised.*

What would be gained from this approach?

.. to be continued ..

* Energy Efficiency: Streamlined communication between hardware and software could
  reduce power consumption. Special hardware suitable for only the software it runs,
  reduce unnecessary energy consumption. There should be no accelerated costs in
  hardware, if competition is fair.
* Cost Reduction: Standardised protocols and closer integration could lower development
  and maintenance costs.
  Software developers are alone responsible for how their software works, not the
  combination of systems the software runs on top of, beside the software itself.
* Simplified Maintenance: A unified system could make maintenance more straightforward
  and less fragmented.


## Technical comparation  CP/M, Unix, MS-DOS/IBM-DOS

#### Target audience

__CP/M__: Designed for microcomputers using Intel 8080 or Zilog Z80 processors,
aimed at hobbyists, small businesses, and early personal computer users.

__Unix__: Initially designed for minicomputers and mainframes used by researchers,
academics, and enterprise environments. Early Unix required more powerful
hardware compared to CP/M.

__MS-DOS/IBM-DOS__: Designed for IBM PCs and compatible microcomputers using
Intel x86 processors, aimed at personal and business users.


#### Architecture and Design

__*CP/M*__: Simple, single-tasking operating system with a command-line interface.
* __File System__: Utilised a flat file system with an 8.3 filename convention (eight
characters for the name, three for the extension).
* __Memory Management__: Limited to 64 KB of RAM, reflecting the hardware constraints
of early microcomputers.
* __Modularity__: Relied on a system BIOS to handle hardware-specific functions,
requiring customization for different hardware.


__*Unix*__: Multiuser, multitasking operating system with a hierarchical file system.
* __File System__: Supported long filenames and a complex directory structure.
Utilised a hierarchical file system with nested directories.
* __Memory Management__: More advanced memory management, supporting larger amounts
of RAM and virtual memory.
* __Modularity__: Designed to be highly portable across different hardware platforms.
Unix was written in C, which facilitated easier adaptation to various systems.

__*MS-DOS/IBM-DOS*__: Single-tasking operating system with a command-line interface.
* __File System__: Initially used the FAT12 file system, which also had an 8.3 filename
convention. Later versions supported FAT16 and FAT32.
* __Memory Management__: Initially limited to 640 KB of conventional memory, with later
enhancements for extended and expanded memory.
* __Modularity__: Less modular compared to Unix but more standardised than CP/M, as
MS-DOS was designed to run on IBM PCs and compatible with a standard BIOS interface.


#### User Interface and Usability

__*CP/M*__: Command-line interface (CLI) with a simple set of commands.
* __Usability__: Geared towards users with some technical knowledge, but less complex
than Unix. Limited user interface features compared to Unix.

__*Unix*__: Command-line interface with powerful shell scripting capabilities. Early
Unix systems used shells like the Bourne shell (sh).
* __Usability__: More complex commands and utilities, providing greater power and
flexibility but requiring more technical expertise. Unix systems often included a
rich set of command-line tools and utilities.
* __GUI__: Some systems had some (propriatary) graphical interfaces, like D-NIX.
Later on not uncommon with X-Windows.

__*MS-DOS/IBM-DOS*__: Command-line interface (CLI) with a straightforward set of commands.
* __Usability__: Designed to be user-friendly for business and personal use, simpler
than Unix but more advanced than CP/M in terms of user interface and available commands.
* __GUI__: MS-DOS/IBM-DOS itself did not include a GUI, but it could run graphical
environments like Microsoft Windows, which started out as an add-on (graphical shell) to
MS-DOS.


#### Software and Ecosystem

__*CP/M*__
* __Software__: Had a significant library of early business and productivity software,
including word processors, spreadsheets, and database programs.
* __Ecosystem__: Dominated the early microcomputer market before being overtaken by MS-DOS.
Limited networking and multiuser capabilities.

__*Unix*__
* __Software__: Known for its rich set of development tools, including compilers,
text processing utilities, and networking tools. Early Unix also supported
multiuser environments and networked operations.
* __Ecosystem__: Widely used in academic, research, and enterprise environments.
Unix systems contributed to the development of the Internet and networking protocols.

__*MS-DOS/IBM-DOS*__
* __Software__: Initially focused on providing a command-line interface and basic
system utilities. Over time, MS-DOS amassed a significant library of applications
including early versions of Microsoft Word and Excel, as well as various business
and gaming software.
* __Ecosystem__: Became dominant in the IBM PC-compatible market, eventually overtaking
CP/M. MS-DOS lacked built-in networking and multiuser capabilities in its early
versions, focusing primarily on single-user desktop computing.


#### Networking and Multiuser Capabilities

__*CP/M*__
* __Networking__: Very limited networking capabilities. Primarily designed for standalone
systems.
* __Multiuser__: Not designed for multiuser operation. Single-tasking environment.

__*Unix*__
* __Networking__: Strong networking capabilities, supporting early development of TCP/IP and
networked computing.
* __Multiuser__: Designed from the ground up to support multiple users simultaneously.
Included features like file permissions and process management to handle multiple users
and tasks.

__*MS-DOS/IBM-DOS*__
* __Networking__: Limited native networking capabilities in early versions. Networking
support evolved over time with add-on software and later versions of DOS.
* __Multiuser__: Designed as a single-user operating system, lacking inherent multiuser
capabilities. Later versions of DOS (like MS-DOS 5.0 and later) included features for
multitasking and enhanced memory management, but true multiuser support was __not__ a
native feature.


#### Portability and Adaptability

__*CP/M*__
* __Portability__: Required a custom BIOS for each hardware platform, which limited its
adaptability to new systems.
* __Adaptability__: Less portable compared to Unix. Each new hardware platform necessitated
significant modifications.

__*Unix*__
* __Portability__: Highly portable due to being written in the C programming language.
Unix could be adapted to run on a wide variety of hardware platforms.
* __Adaptability__: Designed to be easily modified and extended. Unix’s modular
architecture and use of C made it easier to port and adapt to different environments.

__*MS-DOS/IBM-DOS*__
* __Portability__: Relied heavily on hardware-specific drivers and configurations,
tied closely to the IBM PC architecture. Compatibility across different hardware
platforms was limited without significant modifications.
* __Adaptability__: Adapted primarily through OEM versions tailored for specific
hardware configurations. Microsoft allowed OEMs to customize MS-DOS for their hardware,
contributing to its widespread adoption on various PC clones. (Not unlike their experience
with adaptability of MSBASIC.)


## References

- *Bit för bit med ABC 800*, Luxor datorer, Motala, 1984.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken*, 1. uppl., Emmdata, Umeå, 1982.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken för ABC 802*, 1. uppl., Emmdata, Umeå, 1983.
