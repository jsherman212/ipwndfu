#ifndef SYNOPSYS_REGS
#define SYNOPSYS_REGS

/* T8015. For the base of the synopsys USB registers, go to the otgphyctrl
 * node in the device tree. Note down the first entry in reg, then scroll
 * down a bit and find the node for usb-device. Note down the entry in
 * reg, and add those two together + 0x200000000 */
#define T8015_SYNOPSYS_REG_BASE (0x230100000)

#define rGINTSTS *(volatile uint32_t *)(T8015_SYNOPSYS_REG_BASE + 0x14)
#define rGINTMSK *(volatile uint32_t *)(T8015_SYNOPSYS_REG_BASE + 0x18)

#endif
