
# CP/M

In the late 1970s and early 1980s, many contemporary computer manufacturers
expanded their proprietary solutions by incorporating CP/M capabilities,
either through add-on cards or by redesigning their machines. This trend
emerged as CP/M gained market traction. For instance, the Apple II could
run CP/M software using third-party Z80 processor cards, such as the Microsoft
SoftCard. Similarly, the Commodore 128 included a Z80 processor, enabling
it to run CP/M alongside its native operating systems.

Additionally, some systems came with CP/M pre-installed from the start.
Notable examples include the Osborne 1, an early portable computer that
bundled software like WordStar and SuperCalc, and the Kaypro II. These
integrations highlighted CP/M's versatility and widespread adoption during
that era.

*Not to be unfashionable, the ABC802 did also came prepared for a possible
extension of running CP/M.*[^cpm]

[^cpm]: Almost the same computer, CP/M for ABC800:
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

The rise of CP/M and the S-100 bus standard occurred roughly in the same time span, creating a
synergistic environment where microcomputer users could benefit from a standardised software
platform *CP/M* and a standardised hardware interface *S-100*. This combination contributed
to the growth of the personal computer industry by making it somewhat easier for users to expand
and customise their systems and for developers to create compatible hardware and software.

But eventually these standards had their drawbacks, as time progressed. The S-100 bus large
and complex backplanes and cards made systems bulky and costly to design and maintain. The parallel
bus architecture was prone to electrical noise and signal integrity issues, leading to potential
data corruption and system instability, as more cards were added. High power consumption due
to its wide range of supported voltages required robust power supplies and cooling solutions,
further increasing costs. Despite being a standard, variations in implementation among manufacturers
caused compatibility issues. The bus also suffered from relatively slow data transfer speeds,
becoming a performance bottleneck. Additionally, the connectors were not robust, leading to
mechanical issues and the need for frequent maintenance. As newer, more efficient bus standards
like the ISA (internal 16-bit of IBM PC/AT) emerged, the S-100 bus quickly became outdated,
struggling to keep up with evolving hardware capabilities.

CP/M, designed for 8-bit microprocessors, had several drawbacks that limited its effectiveness
as computing technology advanced. Its memory management capabilities were restricted, allowing
it to address only up to 64KB of memory. The absence of built-in networking capabilities hindered
its usefulness in networked environments, becoming a significant limitation as businesses and
individuals began connecting computers in local area networks (LANs) and using modems for remot
communication. CP/M's command-line interface, though functional, was basic and started to feel
outdated as graphical user interfaces like those in the Apple Macintosh and later Microsoft
Windows emerged, offering more intuitive and visually appealing experiences. Additionally,
CP/M struggled to support emerging hardware technologies such as high-resolution graphics,
advanced sound systems, and sophisticated input devices, making it less appealing for multimedia
applications. Its single-tasking nature, which allowed only *one program to run at a time*, became
a significant drawback as multitasking grew in importance for productivity and user convenience.
Furthermore, the existence of many variations of CP/M created by different hardware manufacturers
led to compatibility issues, with software written for one version often not running correctly on
another. These limitations collectively contributed to CP/M's decline as a previous dominant
operating system.




## ABCNIX, D-NIX, ..

Cromix ...

https://sv.wikipedia.org/wiki/D-NIX


## References

- *Bit för bit med ABC 800*, Luxor datorer, Motala, 1984.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken*, 1. uppl., Emmdata, Umeå, 1982.
- Lundgren, Jan & Thornell, Sören, *BASIC II boken för ABC 802*, 1. uppl., Emmdata, Umeå, 1983.
