# RadBuild Integration {#radbuild_integration}

RADix-OS uses RadBuild 0.2.1 as the canonical graph entrypoint for repeatable
x86 VM image builds. The repository root `settings.json` defines one
`os.radix` system named `radix-x86-64-grub-slint` and uses the `radix-os`
provider.

## Build Command

Run from the RADix-OS repository root:

```bash
../RadBuild/radbuild/.tools/radbuild.py build os --settings settings.json --json-events
```

The provider executes `tools/embedded/x86_64_grub_slint_smoke.sh` for the
configured QEMU SMP counts. The Crimson 0.1.0 default is two smoke passes:
2 vCPUs and 4 vCPUs.

## Artifact Output

Successful builds stage development artifacts under:

```text
artifacts/radix/x86_64-grub-slint/
```

The staged set includes the GRUB ISO, kernel ELF, ext4 root filesystem, FAT32
image, one serial log per SMP smoke, and `SHA256SUMS`.

RadBuild writes the machine-readable manifest at:

```text
.radmeta/artifacts/radbuild-artifacts.json
```

`artifacts/` and `.radmeta/` are local build outputs and are intentionally not
tracked in the RADix-OS source repository.

## Publication

RADix-OS source and API documentation are published through RadicalPackages as
experimental Crimson documentation. Generated VM images remain local development
artifacts until a separate binary image release channel is defined.
