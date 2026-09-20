# Error Report: ERR-045 - Missing Linker Script File STM32WLE5XX_FLASH.ld

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-045` |
| **Date & Time** | 2026-09-20 12:36:00 +05:30 |
| **Commit SHA** | [`a36943b`](https://github.com/Thejus26/rain-predict/commit/a36943bb16b6561e3880a1dc9fd82605fa82bed2) |
| **Component / Subsystem** | Build System & Linker Configuration (Linker Script / Memory Layout) |
| **Sprint & Task** | Sprint 6 (`S6-T3.1` 8-State Application State Machine & Target ELF Linking) |
| **Severity** | High (CI Embedded Cross-Compilation Final ELF Link Failure) |
| **Impacted Files** | [`firmware/core/src/STM32WLE5XX_FLASH.ld`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/STM32WLE5XX_FLASH.ld)<br>[`CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/CMakeLists.txt) |

---

## 1. Description & Symptoms

During automated CI cross-compilation on GitHub Actions runner (`ninja` cross-compilation target build), compilation of all object files and static library archives (`libfirmware_app.a`, `libfirmware_middleware.a`, `libfirmware_drivers.a`, `libfirmware_core.a`) succeeded. However, when the GNU linker attempted to link the target executable `rain_predict.elf`, it failed immediately because the specified linker script was missing:

```text
FAILED: [code=1] rain_predict.elf 
: && /usr/bin/arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -ffunction-sections -fdata-sections -O3 -DNDEBUG -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 \
  -mfloat-abi=hard -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs \
  -T/home/runner/work/rain-predict/rain-predict/firmware/core/src/STM32WLE5XX_FLASH.ld \
  -Wl,-Map=/home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.map \
  CMakeFiles/rain_predict.elf.dir/firmware/core/src/main.c.obj -o rain_predict.elf \
  firmware/app/libfirmware_app.a firmware/middleware/libfirmware_middleware.a \
  firmware/drivers/libfirmware_drivers.a firmware/core/libfirmware_core.a -lm \
  && cd /home/runner/work/rain-predict/rain-predict/build-arm \
  && /usr/bin/arm-none-eabi-objcopy -O ihex /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.elf /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.hex \
  && /usr/bin/arm-none-eabi-objcopy -O binary /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.elf /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.bin \
  && /usr/bin/arm-none-eabi-size /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.elf

/usr/lib/gcc/arm-none-eabi/10.3.1/../../../arm-none-eabi/bin/ld: cannot open linker script file /home/runner/work/rain-predict/rain-predict/firmware/core/src/STM32WLE5XX_FLASH.ld: No such file or directory
collect2: error: ld returned 1 exit status
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

### Target Executable Activation

In [`CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/CMakeLists.txt):
```cmake
    # Linker script location
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/src/STM32WLE5XX_FLASH.ld)

    # Target executable
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/src/main.c")
        add_executable(${PROJECT_NAME}.elf
            firmware/core/src/main.c
        )
```

Prior to Sprint 6 Task `S6-T3.1`, [`firmware/core/src/main.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/main.c) did not exist. The conditional `if(EXISTS .../main.c)` evaluated to false, so CMake never generated link rules for `rain_predict.elf`.

When `S6-T3.1` implemented the primary application state machine and created `main.c`, CMake attempted to build `rain_predict.elf` for the first time.

### Missing File

Although `STM32WLE5XX_FLASH.ld` is documented in [`context/project-structure.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/project-structure.md), [`context/code-build.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/code-build.md), and [`docs/architecture/firmware-architecture.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/docs/architecture/firmware-architecture.md), the physical file was never committed to `firmware/core/src/`.

Consequently, the linker failed with:
`cannot open linker script file .../STM32WLE5XX_FLASH.ld: No such file or directory`.

---

## 3. Resolution & Code Changes

*Status: Resolved & Verified*

### Concrete Resolution Steps Taken

1. **Created Canonical Linker Script**:
   Created [`firmware/core/src/STM32WLE5XX_FLASH.ld`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/src/STM32WLE5XX_FLASH.ld) with precise hardware memory boundaries for the STM32WLE5CC SoC:
   - `FLASH (rx)`: Base `0x08000000`, size `256K`.
   - `RAM (xrw)`: Base `0x20000000`, size `64K` (48 KB SRAM1 + 16 KB SRAM2).
   - Stack top `_estack = ORIGIN(RAM) + LENGTH(RAM);` (`0x20010000`).
   - Standard sections: `.isr_vector`, `.text`, `.rodata`, `.data`, `.bss`, `._user_heap_stack`.
   - Fallback aliases: `PROVIDE(Reset_Handler = _start);` and `PROVIDE(_start = main);`.

### Code Changes (Created File)

```ld
/*
 ******************************************************************************
 * @file      STM32WLE5XX_FLASH.ld
 * @author    Tea Plantation Rain Prediction System Dev Team
 * @brief     Linker script for STM32WLE5xx Device with
 *            256KBytes FLASH and 64KBytes RAM (48K SRAM1 + 16K SRAM2)
 ******************************************************************************
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM: 0x20010000 */

_Min_Heap_Size = 0x200;      /* Required amount of heap */
_Min_Stack_Size = 0x800;     /* Required amount of stack (2 KB) */

MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 64K
  FLASH  (rx)     : ORIGIN = 0x08000000,   LENGTH = 256K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM : {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

PROVIDE(Reset_Handler = _start);
PROVIDE(_start = main);
```

---

## 4. Verification & Prevention Guidelines

### Prevention Rules

1. **Build Pre-Requisite Verification**:
   When configuring executable targets with custom linker scripts (`-T<script>`), ensure the linker script file physically exists in the repository alongside the CMake definition.
2. **Memory Map Integrity**:
   Verify section bounds against STM32WLE5 Reference Manual (RM0453):
   - Flash: 256 KB ($0x08000000 - 0x0803FFFF$)
   - SRAM1: 48 KB ($0x20000000 - 0x2000BFFF$)
   - SRAM2: 16 KB ($0x2000C000 - 0x2000FFFF$)
