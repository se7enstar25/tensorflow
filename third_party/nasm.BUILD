# Description:
#   NASM is a portable assembler in the Intel/Microsoft tradition.

licenses(["notice"])  # BSD 2-clause

exports_files(["LICENSE"])

cc_binary(
    name = "nasm",
    srcs = [
        "version.h",
        "include/compiler.h",
        "include/disp8.h",
        "include/error.h",
        "include/hashtbl.h",
        "include/iflag.h",
        "include/insns.h",
        "include/labels.h",
        "include/md5.h",
        "include/nasm.h",
        "include/nasmint.h",
        "include/nasmlib.h",
        "include/opflags.h",
        "include/perfhash.h",
        "include/raa.h",
        "include/rbtree.h",
        "include/rdoff.h",
        "include/saa.h",
        "include/strlist.h",
        "include/tables.h",
        "include/ver.h",
        "asm/nasm.c",
        "stdlib/snprintf.c",
        "stdlib/vsnprintf.c",
        "stdlib/strlcpy.c",
        "stdlib/strnlen.c",
        "nasmlib/ver.c",
        "nasmlib/crc64.c",
        "nasmlib/malloc.c",
        "nasmlib/md5c.c",
        "nasmlib/string.c",
        "nasmlib/file.c",
        "nasmlib/file.h",
        "nasmlib/mmap.c",
        "nasmlib/ilog2.c",
        "nasmlib/realpath.c",
        "nasmlib/path.c",
        "nasmlib/filename.c",
        "nasmlib/srcfile.c",
        "nasmlib/zerobuf.c",
        "nasmlib/readnum.c",
        "nasmlib/bsi.c",
        "nasmlib/rbtree.c",
        "nasmlib/hashtbl.c",
        "nasmlib/raa.c",
        "nasmlib/saa.c",
        "nasmlib/strlist.c",
        "nasmlib/perfhash.c",
        "nasmlib/badenum.c",
        "common/common.c",
        "x86/insnsa.c",
        "x86/insnsb.c",
        "x86/insnsd.c",
        "x86/insnsn.c",
        "x86/regs.c",
        "x86/regvals.c",
        "x86/regflags.c",
        "x86/regdis.c",
        "x86/disp8.c",
        "x86/iflag.c",
        "asm/error.c",
        "asm/float.c",
        "asm/directiv.c",
        "asm/directbl.c",
        "asm/pragma.c",
        "asm/assemble.c",
        "asm/labels.c",
        "asm/parser.c",
        "asm/preproc.c",
        "asm/quote.c",
        "asm/pptok.c",
        "asm/listing.c",
        "asm/eval.c",
        "asm/exprlib.c",
        "asm/exprdump.c",
        "asm/stdscan.c",
        "asm/strfunc.c",
        "asm/tokhash.c",
        "asm/segalloc.c",
        "asm/preproc-nop.c",
        "asm/rdstrnum.c",
        "macros/macros.c",
        "output/outform.c",
        "output/outlib.c",
        "output/legacy.c",
        "output/nulldbg.c",
        "output/nullout.c",
        "output/outbin.c",
        "output/outaout.c",
        "output/outcoff.c",
        "output/outelf.c",
        "output/outobj.c",
        "output/outas86.c",
        "output/outrdf2.c",
        "output/outdbg.c",
        "output/outieee.c",
        "output/outmacho.c",
        "output/codeview.c",
        "disasm/disasm.c",
        "disasm/disasm.h",
        "disasm/sync.c",
        "disasm/sync.h",
        "config/unknown.h",
    ],
    includes = [
        "include",
        "asm",
        "output",
        "x86",
    ],
    copts = select({
        ":windows": [],
        ":windows_msvc": [],
        "//conditions:default": [
            "-w",
            "-std=c99",
        ],
    }),
    defines = select({
        ":windows": [],
        ":windows_msvc": [],
        "//conditions:default": [
            "HAVE_SNPRINTF",
            "HAVE_SYS_TYPES_H",
        ],
    }),
    visibility = ["@jpeg//:__pkg__"],
)

config_setting(
    name = "windows_msvc",
    values = {
        "cpu": "x64_windows_msvc",
    },
)

config_setting(
    name = "windows",
    values = {
        "cpu": "x64_windows",
    },
)
