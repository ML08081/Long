cmd_/opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.o := loongarch64-linux-gnu-gcc -Wp,-MD,/opPJ/PatrolSystem/kernel-modules/compat/.vb2compat_mod.o.d -nostdinc -isystem /home/molim/toolchains/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin/../lib/gcc/loongarch64-linux-gnu/8.3.0/include -I/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include -I./arch/loongarch/include/generated  -I/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include -I./include -I/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi -I./arch/loongarch/include/generated/uapi -I/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi -I./include/generated/uapi -include /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kconfig.h -include /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/compiler_types.h  -I/opPJ/PatrolSystem/kernel-modules/compat -I/opPJ/PatrolSystem/kernel-modules/compat -D__KERNEL__ -DCC_USING_PATCHABLE_FUNCTION_ENTRY -DVMLINUX_LOAD_ADDRESS=0x9000000000200000 -DDATAOFFSET=0 -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -Werror-implicit-function-declaration -Werror=return-type -Wno-format-security -std=gnu89 -fno-PIE -mno-check-zero-division -mabi=lp64 -G0 -pipe -msoft-float -DGAS_HAS_SET_HARDFLOAT -Wa,-msoft-float -ffreestanding -ULOONGARCHEB -U_LOONGARCHEB -U__LOONGARCHEB -U__LOONGARCHEB__ -ULOONGARCHEL -U_LOONGARCHEL -U__LOONGARCHEL -U__LOONGARCHEL__ -DLOONGARCHEL -D_LOONGARCHEL -D__LOONGARCHEL -D__LOONGARCHEL__ -U_LOONGARCH_ISA -D_LOONGARCH_ISA=_LOONGARCH_ISA_LOONGARCH64  -I/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/mach-la64  -I/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/mach-generic -fno-asynchronous-unwind-tables -mstrict-align -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-int-in-bool-context -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fno-stack-protector -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-var-tracking-assignments -fpatchable-function-entry=2 -Wdeclaration-after-statement -Wno-pointer-sign -Wno-stringop-truncation -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -fmacro-prefix-map=/opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/= -Wno-packed-not-aligned  -DMODULE -fplt -Wa,-mla-global-with-abs,-mla-local-with-abs  -DKBUILD_BASENAME='"vb2compat_mod"' -DKBUILD_MODNAME='"vb2compat"' -c -o /opPJ/PatrolSystem/kernel-modules/compat/.tmp_vb2compat_mod.o /opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.c

source_/opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.o := /opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.c

deps_/opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.o := \
    $(wildcard include/config/frame/vector.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/compiler-gcc.h \
    $(wildcard include/config/arm64.h) \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/module.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/sysfs.h) \
    $(wildcard include/config/modules/tree/lookup.h) \
    $(wildcard include/config/livepatch.h) \
    $(wildcard include/config/unused/symbols.h) \
    $(wildcard include/config/module/sig.h) \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/kallsyms.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/tracepoints.h) \
    $(wildcard include/config/jump/label.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/event/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
    $(wildcard include/config/module/unload.h) \
    $(wildcard include/config/constructors.h) \
    $(wildcard include/config/function/error/injection.h) \
    $(wildcard include/config/strict/module/rwx.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/int-ll64.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/int-ll64.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/bitsperlong.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitsperlong.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/bitsperlong.h \
  arch/loongarch/include/generated/uapi/asm/types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/posix_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/stddef.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/stddef.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/compiler_types.h \
  arch/loongarch/include/generated/asm/posix_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/posix_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/poison.h \
    $(wildcard include/config/illegal/pointer/value.h) \
    $(wildcard include/config/page/poisoning/zero.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/const.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/const.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/arch/has/refcount.h) \
    $(wildcard include/config/panic/timeout.h) \
  /home/molim/toolchains/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/lib/gcc/loongarch64-linux-gnu/8.3.0/include/stdarg.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/linkage.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/stringify.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/export.h \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/module/rel/crcs.h) \
    $(wildcard include/config/have/arch/prel32/relocations.h) \
    $(wildcard include/config/trim/unused/ksyms.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/linkage.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/debug/entry.h) \
    $(wildcard include/config/kasan.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/barrier.h \
    $(wildcard include/config/weak/ordering.h) \
    $(wildcard include/config/weak/reordering/beyond/llsc.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/addrspace.h \
    $(wildcard include/config/32bit.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/loongarchregs.h \
    $(wildcard include/config/page/size/4kb.h) \
    $(wildcard include/config/page/size/16kb.h) \
    $(wildcard include/config/page/size/64kb.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/bits.h \
  /home/molim/toolchains/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/lib/gcc/loongarch64-linux-gnu/8.3.0/include/larchintrin.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/barrier.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kasan-checks.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/bitops.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/bitops.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/builtin-ffs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/builtin-fls.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/builtin-__ffs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/builtin-__fls.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/ffz.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/fls64.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/sched.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/hweight.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/arch_hweight.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/const_hweight.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/atomic.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/atomic.h \
    $(wildcard include/config/generic/atomic64.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/atomic.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/cpu-features.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/cpu.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/cpu-info.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/kernel.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/sysinfo.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/cache.h \
    $(wildcard include/config/l1/cache/shift.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/cmpxchg.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/typecheck.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/irqflags.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/build_bug.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/atomic-long.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/non-atomic.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/lock.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/le.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/byteorder.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/byteorder/little_endian.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/byteorder/little_endian.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/swab.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/swab.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/swab.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/byteorder/generic.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bitops/ext2-atomic.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/printk.h \
    $(wildcard include/config/message/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/quiet.h) \
    $(wildcard include/config/early/printk.h) \
    $(wildcard include/config/printk/nmi.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/init.h \
    $(wildcard include/config/strict/kernel/rwx.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kern_levels.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/stat.h \
  arch/loongarch/include/generated/asm/stat.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/stat.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/stat.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/time.h \
    $(wildcard include/config/arch/uses/gettimeoffset.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/seqlock.h \
    $(wildcard include/config/debug/lock/alloc.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/spinlock.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/preempt.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/preempt.h \
    $(wildcard include/config/preempt/count.h) \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/trace/preempt/toggle.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  arch/loongarch/include/generated/asm/preempt.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/preempt.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/thread_info.h \
    $(wildcard include/config/thread/info/in/task.h) \
    $(wildcard include/config/have/arch/within/stack/frames.h) \
    $(wildcard include/config/hardened/usercopy.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/bug.h \
    $(wildcard include/config/bug/on/data/corruption.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/bug.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/asm-bug.h \
    $(wildcard include/config/debug/bugverbose.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/break.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/bug.h \
    $(wildcard include/config/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/restart_block.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/time64.h \
    $(wildcard include/config/64bit/time.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/math64.h \
    $(wildcard include/config/arch/supports/int128.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/div64.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/div64.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/time.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/errno.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/errno.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/errno.h \
  arch/loongarch/include/generated/uapi/asm/errno.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/errno.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/errno-base.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/thread_info.h \
    $(wildcard include/config/cpu/has/lbt.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/processor.h \
    $(wildcard include/config/va/bits/40.h) \
    $(wildcard include/config/va/bits/48.h) \
    $(wildcard include/config/cpu/has/lasx.h) \
    $(wildcard include/config/cpu/has/lsx.h) \
    $(wildcard include/config/cpu/has/prefetch.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/bitmap.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
    $(wildcard include/config/fortify/source.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/string.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/string.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sizes.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/ptrace.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/bottom_half.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/spinlock_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/spinlock_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/paravirt.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/qrwlock_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/lockdep.h \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/lock/stat.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rwlock_types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/spinlock.h \
  arch/loongarch/include/generated/asm/qrwlock.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/qrwlock.h \
    $(wildcard include/config/cpu/loongson3.h) \
    $(wildcard include/config/cpu/loongson2k.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/qspinlock.h \
    $(wildcard include/config/paravirt/spinlocks.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/paravirt.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/kvm_para.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/kvm_para.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/qspinlock.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rwlock.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/inline/spin/lock.h) \
    $(wildcard include/config/inline/spin/lock/bh.h) \
    $(wildcard include/config/inline/spin/lock/irq.h) \
    $(wildcard include/config/inline/spin/lock/irqsave.h) \
    $(wildcard include/config/inline/spin/trylock.h) \
    $(wildcard include/config/inline/spin/trylock/bh.h) \
    $(wildcard include/config/uninline/spin/unlock.h) \
    $(wildcard include/config/inline/spin/unlock/bh.h) \
    $(wildcard include/config/inline/spin/unlock/irq.h) \
    $(wildcard include/config/inline/spin/unlock/irqrestore.h) \
    $(wildcard include/config/generic/lockbreak.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/inline/read/lock.h) \
    $(wildcard include/config/inline/write/lock.h) \
    $(wildcard include/config/inline/read/lock/bh.h) \
    $(wildcard include/config/inline/write/lock/bh.h) \
    $(wildcard include/config/inline/read/lock/irq.h) \
    $(wildcard include/config/inline/write/lock/irq.h) \
    $(wildcard include/config/inline/read/lock/irqsave.h) \
    $(wildcard include/config/inline/write/lock/irqsave.h) \
    $(wildcard include/config/inline/read/trylock.h) \
    $(wildcard include/config/inline/write/trylock.h) \
    $(wildcard include/config/inline/read/unlock.h) \
    $(wildcard include/config/inline/write/unlock.h) \
    $(wildcard include/config/inline/read/unlock/bh.h) \
    $(wildcard include/config/inline/write/unlock/bh.h) \
    $(wildcard include/config/inline/read/unlock/irq.h) \
    $(wildcard include/config/inline/write/unlock/irq.h) \
    $(wildcard include/config/inline/read/unlock/irqrestore.h) \
    $(wildcard include/config/inline/write/unlock/irqrestore.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/time32.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/uidgid.h \
    $(wildcard include/config/multiuser.h) \
    $(wildcard include/config/user/ns.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/highuid.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kmod.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/umh.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/gfp.h \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/zone/dma32.h) \
    $(wildcard include/config/zone/device.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/pm/sleep.h) \
    $(wildcard include/config/memory/isolation.h) \
    $(wildcard include/config/compaction.h) \
    $(wildcard include/config/cma.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/mmdebug.h \
    $(wildcard include/config/debug/vm.h) \
    $(wildcard include/config/debug/virtual.h) \
    $(wildcard include/config/debug/vm/pgflags.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/mmzone.h \
    $(wildcard include/config/force/max/zoneorder.h) \
    $(wildcard include/config/zsmalloc.h) \
    $(wildcard include/config/memcg.h) \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/flat/node/mem/map.h) \
    $(wildcard include/config/page/extension.h) \
    $(wildcard include/config/no/bootmem.h) \
    $(wildcard include/config/deferred/struct/page/init.h) \
    $(wildcard include/config/transparent/hugepage.h) \
    $(wildcard include/config/have/memory/present.h) \
    $(wildcard include/config/have/memoryless/nodes.h) \
    $(wildcard include/config/have/memblock/node/map.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
    $(wildcard include/config/have/arch/early/pfn/to/nid.h) \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem/extreme.h) \
    $(wildcard include/config/memory/hotremove.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
    $(wildcard include/config/holes/in/zone.h) \
    $(wildcard include/config/arch/has/holes/memorymodel.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/wait.h \
  arch/loongarch/include/generated/asm/current.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/current.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/wait.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/numa.h \
    $(wildcard include/config/nodes/shift.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/nodemask.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/pageblock-flags.h \
    $(wildcard include/config/hugetlb/page.h) \
    $(wildcard include/config/hugetlb/page/size/variable.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/page-flags-layout.h \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/numa/balancing.h) \
  include/generated/bounds.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/sparsemem.h \
    $(wildcard include/config/loongarch/huge/tlb/support.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/mm_types.h \
    $(wildcard include/config/have/aligned/struct/page.h) \
    $(wildcard include/config/userfaultfd.h) \
    $(wildcard include/config/have/arch/compat/mmap/bases.h) \
    $(wildcard include/config/membarrier.h) \
    $(wildcard include/config/aio.h) \
    $(wildcard include/config/mmu/notifier.h) \
    $(wildcard include/config/arch/want/batched/unmap/tlb/flush.h) \
    $(wildcard include/config/hmm.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/mm_types_task.h \
    $(wildcard include/config/split/ptlock/cpus.h) \
    $(wildcard include/config/arch/enable/split/pmd/ptlock.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/page.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/pfn.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/io.h \
    $(wildcard include/config/ioremap/with/tlb.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/pgtable-bits.h \
  arch/loongarch/include/generated/asm/early_ioremap.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/early_ioremap.h \
    $(wildcard include/config/generic/early/ioremap.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/io.h \
    $(wildcard include/config/generic/iomap.h) \
    $(wildcard include/config/has/ioport/map.h) \
    $(wildcard include/config/virt/to/bus.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/pci_iomap.h \
    $(wildcard include/config/pci.h) \
    $(wildcard include/config/no/generic/pci/ioport/map.h) \
    $(wildcard include/config/generic/pci/iomap.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/logic_pio.h \
    $(wildcard include/config/indirect/pio.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/fwnode.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/vmalloc.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/llist.h \
    $(wildcard include/config/arch/have/nmi/safe/cmpxchg.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rbtree.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rcupdate.h \
    $(wildcard include/config/preempt/rcu.h) \
    $(wildcard include/config/rcu/stall/common.h) \
    $(wildcard include/config/no/hz/full.h) \
    $(wildcard include/config/rcu/nocb/cpu.h) \
    $(wildcard include/config/tasks/rcu.h) \
    $(wildcard include/config/tree/rcu.h) \
    $(wildcard include/config/tiny/rcu.h) \
    $(wildcard include/config/debug/objects/rcu/head.h) \
    $(wildcard include/config/prove/rcu.h) \
    $(wildcard include/config/rcu/boost.h) \
    $(wildcard include/config/arch/weak/release/acquire.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rcutree.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/overflow.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/limits.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/memory_model.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/getorder.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/auxvec.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/auxvec.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/auxvec.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rwsem.h \
    $(wildcard include/config/rwsem/spin/on/owner.h) \
    $(wildcard include/config/rwsem/generic/spinlock.h) \
    $(wildcard include/config/node/cache/thrash/optimization.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/err.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/osq_lock.h \
  arch/loongarch/include/generated/asm/rwsem.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/rwsem.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/completion.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/uprobes.h \
    $(wildcard include/config/uprobes.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/uprobes.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/notifier.h \
    $(wildcard include/config/tree/srcu.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/mutex.h \
    $(wildcard include/config/mutex/spin/on/owner.h) \
    $(wildcard include/config/debug/mutexes.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/debug_locks.h \
    $(wildcard include/config/debug/locking/api/selftests.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/srcu.h \
    $(wildcard include/config/tiny/srcu.h) \
    $(wildcard include/config/srcu.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/workqueue.h \
    $(wildcard include/config/debug/objects/work.h) \
    $(wildcard include/config/freezer.h) \
    $(wildcard include/config/wq/watchdog.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/timer.h \
    $(wildcard include/config/debug/objects/timers.h) \
    $(wildcard include/config/no/hz/common.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/ktime.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/jiffies.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/timex.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/timex.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/param.h \
  arch/loongarch/include/generated/asm/param.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/param.h \
    $(wildcard include/config/hz.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/param.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/timex.h \
  include/generated/timeconst.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/timekeeping.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/timekeeping32.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/debugobjects.h \
    $(wildcard include/config/debug/objects.h) \
    $(wildcard include/config/debug/objects/free.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rcu_segcblist.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/srcutree.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rcu_node_tree.h \
    $(wildcard include/config/rcu/fanout.h) \
    $(wildcard include/config/rcu/fanout/leaf.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/inst.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/asm.h \
    $(wildcard include/config/sgi/ip28.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/abidefs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/ptrace.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/inst.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/bitfield.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/mmu.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/memory_hotplug.h \
    $(wildcard include/config/arch/has/add/pages.h) \
    $(wildcard include/config/have/arch/nodedata/extension.h) \
    $(wildcard include/config/have/bootmem/info/node.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/topology.h \
    $(wildcard include/config/use/percpu/numa/node/id.h) \
    $(wildcard include/config/sched/smt.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/smp.h \
    $(wildcard include/config/up/late/init.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/smp.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/percpu.h \
    $(wildcard include/config/need/per/cpu/embed/first/chunk.h) \
    $(wildcard include/config/need/per/cpu/page/first/chunk.h) \
    $(wildcard include/config/have/setup/per/cpu/area.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/percpu.h \
    $(wildcard include/config/cpu/loongson64.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/percpu.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/percpu-defs.h \
    $(wildcard include/config/debug/force/weak/per/cpu.h) \
    $(wildcard include/config/amd/mem/encrypt.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/topology.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/topology.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/mach-la64/topology.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sysctl.h \
    $(wildcard include/config/sysctl.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/sysctl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/elf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/elf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/fs.h \
    $(wildcard include/config/fs/posix/acl.h) \
    $(wildcard include/config/security.h) \
    $(wildcard include/config/cgroup/writeback.h) \
    $(wildcard include/config/ima.h) \
    $(wildcard include/config/fsnotify.h) \
    $(wildcard include/config/fs/encryption.h) \
    $(wildcard include/config/epoll.h) \
    $(wildcard include/config/file/locking.h) \
    $(wildcard include/config/quota.h) \
    $(wildcard include/config/fs/dax.h) \
    $(wildcard include/config/block.h) \
    $(wildcard include/config/mandatory/file/locking.h) \
    $(wildcard include/config/migration.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/wait_bit.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kdev_t.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/kdev_t.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/dcache.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rculist.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rculist_bl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/list_bl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/bit_spinlock.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/lockref.h \
    $(wildcard include/config/arch/use/cmpxchg/lockref.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/stringhash.h \
    $(wildcard include/config/dcache/word/access.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/hash.h \
    $(wildcard include/config/have/arch/hash.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/path.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/list_lru.h \
    $(wildcard include/config/memcg/kmem.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/shrinker.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/radix-tree.h \
    $(wildcard include/config/radix/tree/multiorder.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/xarray.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/pid.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/capability.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/capability.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/semaphore.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/fcntl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/fcntl.h \
  arch/loongarch/include/generated/asm/fcntl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/fcntl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/fiemap.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/migrate_mode.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/percpu-rwsem.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rcuwait.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rcu_sync.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/delayed_call.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/uuid.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/uuid.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/errseq.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/ioprio.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sched.h \
    $(wildcard include/config/virt/cpu/accounting/native.h) \
    $(wildcard include/config/sched/info.h) \
    $(wildcard include/config/schedstats.h) \
    $(wildcard include/config/fair/group/sched.h) \
    $(wildcard include/config/rt/group/sched.h) \
    $(wildcard include/config/cgroup/sched.h) \
    $(wildcard include/config/blk/dev/io/trace.h) \
    $(wildcard include/config/compat/brk.h) \
    $(wildcard include/config/cgroups.h) \
    $(wildcard include/config/blk/cgroup.h) \
    $(wildcard include/config/stackprotector.h) \
    $(wildcard include/config/arch/has/scaled/cputime.h) \
    $(wildcard include/config/virt/cpu/accounting/gen.h) \
    $(wildcard include/config/posix/timers.h) \
    $(wildcard include/config/sysvipc.h) \
    $(wildcard include/config/detect/hung/task.h) \
    $(wildcard include/config/auditsyscall.h) \
    $(wildcard include/config/rt/mutexes.h) \
    $(wildcard include/config/ubsan.h) \
    $(wildcard include/config/task/xacct.h) \
    $(wildcard include/config/cpusets.h) \
    $(wildcard include/config/intel/rdt.h) \
    $(wildcard include/config/futex.h) \
    $(wildcard include/config/compat.h) \
    $(wildcard include/config/perf/events.h) \
    $(wildcard include/config/rseq.h) \
    $(wildcard include/config/task/delay/acct.h) \
    $(wildcard include/config/fault/injection.h) \
    $(wildcard include/config/latencytop.h) \
    $(wildcard include/config/function/graph/tracer.h) \
    $(wildcard include/config/kcov.h) \
    $(wildcard include/config/bcache.h) \
    $(wildcard include/config/vmap/stack.h) \
    $(wildcard include/config/arch/task/struct/on/stack.h) \
    $(wildcard include/config/debug/rseq.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/sched.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sem.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/sem.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/ipc.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rhashtable-types.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/ipc.h \
  arch/loongarch/include/generated/uapi/asm/ipcbuf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/ipcbuf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/refcount.h \
    $(wildcard include/config/refcount/full.h) \
  arch/loongarch/include/generated/asm/sembuf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/sembuf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/shm.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/shm.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/hugetlb_encode.h \
  arch/loongarch/include/generated/asm/shmbuf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/shmbuf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/shmparam.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kcov.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/kcov.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/plist.h \
    $(wildcard include/config/debug/pi/list.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/hrtimer.h \
    $(wildcard include/config/high/res/timers.h) \
    $(wildcard include/config/time/low/res.h) \
    $(wildcard include/config/timerfd.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/timerqueue.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/seccomp.h \
    $(wildcard include/config/seccomp.h) \
    $(wildcard include/config/have/arch/seccomp/filter.h) \
    $(wildcard include/config/seccomp/filter.h) \
    $(wildcard include/config/checkpoint/restore.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/seccomp.h \
  arch/loongarch/include/generated/asm/seccomp.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/seccomp.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/unistd.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/unistd.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/unistd.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/unistd.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/unistd.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/resource.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/resource.h \
  arch/loongarch/include/generated/asm/resource.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/resource.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/resource.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/latencytop.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sched/prio.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/signal_types.h \
    $(wildcard include/config/old/sigaction.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/signal.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/signal.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/signal.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/signal-defs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/sigcontext.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/siginfo.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/siginfo.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/task_io_accounting.h \
    $(wildcard include/config/task/io/accounting.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/rseq.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sched/rt.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/iocontext.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/fs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/ioctl.h \
  arch/loongarch/include/generated/asm/ioctl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/ioctl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/asm-generic/ioctl.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/quota.h \
    $(wildcard include/config/quota/netlink/interface.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/percpu_counter.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/dqblk_xfs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/dqblk_v1.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/dqblk_v2.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/dqblk_qtree.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/projid.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/quota.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/nfs_fs_i.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/elf.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/uapi/linux/elf-em.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/vdso.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/uapi/asm/hwcap.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kobject.h \
    $(wildcard include/config/uevent/helper.h) \
    $(wildcard include/config/debug/kobject/release.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/sysfs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kernfs.h \
    $(wildcard include/config/kernfs.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/idr.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kobject_ns.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/kref.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/moduleparam.h \
    $(wildcard include/config/alpha.h) \
    $(wildcard include/config/ia64.h) \
    $(wildcard include/config/ppc64.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/jump_label.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/rbtree_latch.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/error-injection.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/error-injection.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/tracepoint-defs.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/linux/static_key.h \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/module.h \
    $(wildcard include/config/unwinder/orc.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/include/asm-generic/module.h \
    $(wildcard include/config/have/mod/arch/specific.h) \
    $(wildcard include/config/modules/use/elf/rel.h) \
    $(wildcard include/config/modules/use/elf/rela.h) \
  /opPJ/LS2K0300-linux-4.19-main/LS2K0300-linux-4.19-main/arch/loongarch/include/asm/orc_types.h \

/opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.o: $(deps_/opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.o)

$(deps_/opPJ/PatrolSystem/kernel-modules/compat/vb2compat_mod.o):
