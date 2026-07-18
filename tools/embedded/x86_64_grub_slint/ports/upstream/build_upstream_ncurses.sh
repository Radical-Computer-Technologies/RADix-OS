#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 ]]; then
    echo "usage: $0 <ncurses-source-dir> <build-dir> <install-dir> <base-sysroot>" >&2
    exit 2
fi

source_dir="$1"
build_dir="$2"
install_dir="$3"
base_sysroot="$4"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cc="${RAD_CC:-x86_64-linux-gnu-gcc}"
host_cc="${RAD_BUILD_CC:-gcc}"
# Architecture knobs (defaults preserve the original x86_64 behavior; the
# ZuBoard aarch64 build overrides all four -- see rad_zuboard_1cg/Makefile):
#   RAD_PORT_STATIC_ARCH_FLAGS / RAD_PORT_PIC_ARCH_FLAGS: arch codegen flags
#   RAD_PORT_TTEXT: userland text base (matches the target's ELF loader)
#   RAD_PORT_HOST_TRIPLET: autoconf --host
#   RAD_PORT_CRT0_SOURCE: crt0 .S path; empty selects the inlined x86 stub
static_arch_flags="${RAD_PORT_STATIC_ARCH_FLAGS:--mno-red-zone}"
pic_arch_flags="${RAD_PORT_PIC_ARCH_FLAGS:--fcf-protection=none -mno-red-zone}"
text_base="${RAD_PORT_TTEXT:-0x40000000}"
host_triplet="${RAD_PORT_HOST_TRIPLET:-x86_64-none}"
crt0_source="${RAD_PORT_CRT0_SOURCE:-}"
pic_compile_flags="-ffreestanding -fno-stack-protector -fPIC ${pic_arch_flags} -nostdinc -U__linux__ -Ulinux -U__gnu_linux__"
static_compile_flags="-ffreestanding -fno-stack-protector -fno-pic -fno-pie ${static_arch_flags} -nostdinc -U__linux__ -Ulinux -U__gnu_linux__"
compile_flags="${static_compile_flags}"
if [[ "${RAD_NCURSES_PIC:-0}" == "1" ]]; then
    compile_flags="${pic_compile_flags}"
fi

mkdir -p "$(dirname "${source_dir}")" "${build_dir}" "${install_dir}"

if [[ ! -f "${source_dir}/configure" ]]; then
    source_archive="${RAD_NCURSES_ARCHIVE:-}"
    if [[ -z "${source_archive}" ]]; then
        source_archive="$(cd "$(dirname "${source_dir}")/.." && pwd)/sources/ncurses-6.6.tar.gz"
    fi
    if [[ ! -f "${source_archive}" ]]; then
        mkdir -p "$(dirname "${source_archive}")"
        curl -L --fail --show-error -o "${source_archive}" "https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz"
    fi
    rm -rf "${source_dir}"
    mkdir -p "$(dirname "${source_dir}")"
    tar -xzf "${source_archive}" -C "$(dirname "${source_dir}")"
fi

if [[ ! -f "${source_dir}/configure" ]]; then
    echo "ncurses source tree does not contain configure: ${source_dir}" >&2
    exit 3
fi

cat >"${build_dir}/rad-cc" <<EOF
#!/usr/bin/env bash
compile_only=0
preprocess_only=0
for arg in "\$@"; do
    case "\${arg}" in
        -c|-S|-E) compile_only=1 ;;
    esac
    case "\${arg}" in
        -E) preprocess_only=1 ;;
    esac
done
if [[ "\${compile_only}" == "1" ]]; then
    if [[ "\${preprocess_only}" == "1" ]]; then
        exec "${cc}" ${compile_flags} "\$@" -I"${base_sysroot}/include"
    fi
    exec "${cc}" ${compile_flags} "\$@" -I"${base_sysroot}/include"
fi
exec "${cc}" ${compile_flags} "\$@" -I"${base_sysroot}/include" -nostdlib -static -no-pie -Wl,-Ttext=${text_base} -Wl,--build-id=none "${build_dir}/rad_crt0.o" -L"${build_dir}" -lradc
EOF
chmod +x "${build_dir}/rad-cc"

if [[ -n "${crt0_source}" ]]; then
    cp "${crt0_source}" "${build_dir}/rad_crt0.S"
else
    cat >"${build_dir}/rad_crt0.S" <<'EOF'
.section .text
.code64
.global _start
.type _start, @function
_start:
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    lea 16(%rsp,%rdi,8), %rdx
    call main
    mov %rax, %rdi
    mov $10, %rax
    int $0x80
1:
    jmp 1b
EOF
fi

"${cc}" ${compile_flags} -c "${build_dir}/rad_crt0.S" -o "${build_dir}/rad_crt0.o"
cp "${RAD_LIBC_ARCHIVE}" "${build_dir}/libradc.a"

cd "${build_dir}"
CONFIG_SITE="${RAD_NCURSES_CONFIG_SITE:-${script_dir}/rad-ncurses-config.site}" \
CC="${build_dir}/rad-cc" \
BUILD_CC="${host_cc}" \
HOSTCC="${host_cc}" \
CFLAGS="-Os -D__rad__=1" \
CPPFLAGS="-I${base_sysroot}/include" \
LDFLAGS="" \
PKG_CONFIG=false \
"${source_dir}/configure" \
    --host="${host_triplet}" \
    --build=x86_64-pc-linux-gnu \
    --prefix=/usr \
    --without-shared \
    --with-normal \
    --without-debug \
    --without-cxx \
    --without-ada \
    --without-manpages \
    --without-progs \
    --without-tests \
    --without-cxx-binding \
    --without-pthread \
    --with-termlib=tinfo \
    --disable-sigwinch \
    --disable-db-install \
    --disable-home-terminfo \
    --enable-widec \
    --enable-ext-funcs \
    --enable-sp-funcs \
    --with-tic-path="${TIC_EXECUTABLE:-tic}" \
    --with-fallbacks=xterm-256color \
    --with-default-terminfo-dir=/usr/share/terminfo \
    --with-terminfo-dirs=/usr/share/terminfo

make -j"${RAD_BUILD_JOBS:-2}" libs
make DESTDIR="${install_dir}" install.libs install.includes

test -f "${install_dir}/usr/lib/libncursesw.a"
test -f "${install_dir}/usr/lib/libtinfo.a"
test -f "${install_dir}/usr/include/ncurses.h" -o -f "${install_dir}/usr/include/curses.h"
if [[ -d "${install_dir}/usr/include/ncursesw" ]]; then
    cp "${install_dir}/usr/include/ncursesw/ncurses.h" "${install_dir}/usr/include/ncurses.h"
    cp "${install_dir}/usr/include/ncursesw/curses.h" "${install_dir}/usr/include/curses.h"
    cp "${install_dir}/usr/include/ncursesw/term.h" "${install_dir}/usr/include/term.h"
fi
