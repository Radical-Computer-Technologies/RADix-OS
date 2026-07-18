# libradc

`libradc` is the current freestanding RADPx-OS C/POSIX compatibility library.
The x86_64 GRUB target builds this source into both `libradc.a` and
`libradc.rso`, then stages those artifacts into its generated root filesystem.

Public compatibility headers live in `include/`. Target builds copy those
headers into generated sysroots rather than owning separate source copies.
