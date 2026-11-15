/* SPDX-License-Identifier: BSD-3-Clause */
ENTRY(Reset_Handler)
MEMORY {
 FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
 RAM (rw) : ORIGIN = 0x20000000, LENGTH = 64K
}
/* Application state and stack use tightly coupled SRAM. DMA never accesses it. */
_estack = ORIGIN(RAM) + LENGTH(RAM);
_heap_end = _estack - 16K;
PHDRS { flash PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) } > FLASH :flash
 .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } > FLASH :flash
 .ARM.extab : { *(.ARM.extab*) } > FLASH :flash
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH :flash
 .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH :flash
 .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH :flash
 .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH :flash
 _sidata = LOADADDR(.data);
 .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH :data
 .bss (NOLOAD) : { . = ALIGN(8); _sbss = .; __bss_start__ = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; __bss_end__ = .; _end = .; end = .; } > RAM :data
 ASSERT(_end <= _heap_end, "DTCM data overlaps the reserved stack")
 .ARM.attributes 0 : { *(.ARM.attributes) }
}
