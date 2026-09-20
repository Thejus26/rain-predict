# Error Report: ERR-044 - Duplicate nano.specs Linker Flags Causing Spec Rename Collision

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-044` |
| **Date & Time** | 2026-09-20 12:26:00 +05:30 |
| **Commit SHA** | [`99828d8`](https://github.com/Thejus26/rain-predict/commit/99828d8134b27fded94425ea4963edfed43c79a0) |
| **Component / Subsystem** | Build System & Linker Configuration (CMake, nano.specs) |
| **Sprint & Task** | Sprint 6 (`S6-T3.1` 8-State Application State Machine & Target ELF Linking) |
| **Severity** | High (CI Embedded Cross-Compilation Final ELF Link Failure) |
| **Impacted Files** | [`CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/CMakeLists.txt)<br>[`cmake/toolchain-arm-none-eabi.cmake`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/cmake/toolchain-arm-none-eabi.cmake) |

---

## 1. Description & Symptoms

During automated CI execution on GitHub Actions runner (`ninja` cross-compilation target build), compilation of all object files succeeded, but the final executable link step for `rain_predict.elf` failed with a fatal compiler specs error:

```text
FAILED: [code=1] rain_predict.elf 
: && /usr/bin/arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -ffunction-sections -fdata-sections -O3 -DNDEBUG -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 \
  -mfloat-abi=hard -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs \
  -T/home/runner/work/rain-predict/rain-predict/firmware/core/src/STM32WLE5XX_FLASH.ld \
  -Wl,-Map=/home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.map \
  -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs \
  CMakeFiles/rain_predict.elf.dir/firmware/core/src/main.c.obj -o rain_predict.elf \
  firmware/app/libfirmware_app.a firmware/middleware/libfirmware_middleware.a \
  firmware/drivers/libfirmware_drivers.a firmware/core/libfirmware_core.a -lm \
  && cd /home/runner/work/rain-predict/rain-predict/build-arm \
  && /usr/bin/arm-none-eabi-objcopy -O ihex /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.elf /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.hex \
  && /usr/bin/arm-none-eabi-objcopy -O binary /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.elf /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.bin \
  && /usr/bin/arm-none-eabi-size /home/runner/work/rain-predict/rain-predict/build-arm/rain_predict.elf

arm-none-eabi-gcc: fatal error: /usr/lib/gcc/arm-none-eabi/10.3.1/../../../arm-none-eabi/lib/nano.specs: attempt to rename spec 'link' to already defined spec 'nano_link'
compilation terminated.
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

### GCC Spec Processing Mechanism

The GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`) provides specs files such as `nano.specs` to substitute standard newlib C runtime libraries with memory-optimized `newlib-nano` implementations.

Inside `nano.specs`, the spec directs the GCC driver to alter the link specs:
```text
%rename link nano_link
```
This renames the built-in GCC `link` spec rule to `nano_link`, allowing `nano.specs` to inject its own linking rules.

However, if `--specs=nano.specs` is provided **more than once** on the compiler driver command line, GCC reads and processes the spec directives a second time. When attempting to execute `%rename link nano_link` again:
- The spec `nano_link` already exists from the first processing pass.
- GCC terminates immediately with:
  ```text
  fatal error: .../nano.specs: attempt to rename spec 'link' to already defined spec 'nano_link'
  ```

### Configuration Duplication

Inspecting the build configuration reveals `--specs=nano.specs` (and `--specs=nosys.specs`) was specified in two separate locations:

1. **Toolchain File**: [`cmake/toolchain-arm-none-eabi.cmake`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/cmake/toolchain-arm-none-eabi.cmake):
   ```cmake
   set(CMAKE_EXE_LINKER_FLAGS_INIT "${ARM_ARCH_FLAGS} -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs")
   ```
   CMake automatically incorporates `CMAKE_EXE_LINKER_FLAGS_INIT` into `CMAKE_EXE_LINKER_FLAGS`, which is prepended to every executable link command.

2. **Root Build Script**: [`CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/CMakeLists.txt):
   ```cmake
   target_link_options(${PROJECT_NAME}.elf PRIVATE
       -T${LINKER_SCRIPT}
       -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
       -Wl,--gc-sections
       --specs=nano.specs
       --specs=nosys.specs
   )
   ```

Because both the global `CMAKE_EXE_LINKER_FLAGS` and the target-level `target_link_options` contained `--specs=nano.specs --specs=nosys.specs -Wl,--gc-sections`, the final linker command string duplicated all three options, triggering the fatal spec rename collision in GCC.

---

## 3. Resolution & Code Changes

*Status: Resolved & Verified*

### Concrete Resolution Steps Taken

1. **Elimination of Redundant Target Linker Options**:
   In [`CMakeLists.txt`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/CMakeLists.txt), removed `--specs=nano.specs`, `--specs=nosys.specs`, and `-Wl,--gc-sections` from `target_link_options(${PROJECT_NAME}.elf PRIVATE ...)`.
2. **Preservation of Global Toolchain Initialization**:
   Retained the canonical architectural linker configuration in [`cmake/toolchain-arm-none-eabi.cmake`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/cmake/toolchain-arm-none-eabi.cmake) (`CMAKE_EXE_LINKER_FLAGS_INIT`), ensuring `--specs=nano.specs`, `--specs=nosys.specs`, and `-Wl,--gc-sections` are applied globally to target executables without duplication.
3. **Single Spec Expansion**:
   The final link command generated by CMake / Ninja now contains `--specs=nano.specs` exactly once, enabling GCC to rename the link spec without encountering collision errors.

### Code Changes (Unified Diff)

```diff
diff --git a/CMakeLists.txt b/CMakeLists.txt
index e991207..563cfd0 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -92,9 +92,6 @@ if(BUILD_TARGET_FIRMWARE AND CMAKE_CROSSCOMPILING)
         target_link_options(${PROJECT_NAME}.elf PRIVATE
             -T${LINKER_SCRIPT}
             -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
-            -Wl,--gc-sections
-            --specs=nano.specs
-            --specs=nosys.specs
         )
 
         # Generate .hex, .bin, and print memory footprint
```

---

## 4. Verification & Prevention Guidelines

### Prevention Rules

1. **Linker Flag Normalization**:
   Never define the same GCC `--specs` flags in both a toolchain file and a CMake `target_link_options` directive. Unlike flags that are idempotent (such as `-O3` or `-Wall`), GCC spec file options `--specs=*` are **not** idempotent and will fail fatally if passed multiple times.
2. **Linker String Verification**:
   Inspect the final generated Ninja link command in `build-arm/build.ninja` to verify that each `--specs` option appears exactly once.
