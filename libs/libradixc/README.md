# libradixc

`libradixc` is the current freestanding RADix C/POSIX compatibility library.
The x86_64 GRUB target builds this source into both `libradixc.a` and
`libradixc.rso`, then stages those artifacts into its generated root filesystem.

Headers are still staged through the target sysroot while the ABI is evolving.
As the library stabilizes, shared public headers should move into this library
tree or a root-level include tree and be copied into target sysroots by RadBuild.
