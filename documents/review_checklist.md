# Driver review checklist (growing list)

This file records review items that must be checked whenever the IQS9151 driver is touched. Add new items as they arise so they remain visible to future reviewers.

## I2C Update Key / I2C Slave Address safety

- The I2C Update Key (`0x1176`) must **never** be written with the unlock value (`0xA3`). Keep it at `0x00`.
- The driver must not write the I2C Slave Address register (`0x1177`). The device default `0x56` should remain unchanged.
- Block writes must exclude both registers so that accidental key changes cannot unlock address updates. In the current driver, the configuration block starts at `0x1178`, skipping `0x1176–0x1177`.

## Memory allocation safety

- Zephyr/ZMK builds often disable the heap; driver code must not rely on `k_malloc`/`k_free`.
- I2C write buffers should be fixed-length. Use a static buffer sized for the largest init block; do not allocate per-call buffers.
- Follow the nRF52840 guidance: ≤32 B payloads are safe anywhere; 33–64 B payloads should use static storage; ≥65 B should be split into smaller transfers rather than sent in one shot.

## Stack usage outside the driver work queue

- Code reached from `settings_load()` runs on the main thread, which only has `CONFIG_MAIN_STACK_SIZE` (1 KB by default). Settings blobs and `struct iqs9151_params` work copies must live in static storage, not on that stack.
- Overflowing that stack corrupts the adjacent idle stack and the board stops booting without a usable fault message. Keep large buffers static and serialize access with a mutex instead of relying on a bigger stack.
