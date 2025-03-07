; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=armv4t-eabi %s -o - | FileCheck %s --check-prefix=V4T
; RUN: llc -mtriple=armv6t2-eabi %s -o - | FileCheck %s --check-prefix=V6T2

; Check for several conditions that should result in SSAT.
; For example, the base test is equivalent to
; x < -k ? -k : (x > k ? k : x) in C. All patterns that bound x
; to the interval [-k, k] where k is a power of 2 can be
; transformed into SSAT. At the end there are some tests
; checking that conditionals are not transformed if they don't
; match the right pattern.

;
; Base tests with different bit widths
;

; x < -k ? -k : (x > k ? k : x)
; 32-bit base test
define i32 @sat_base_32bit(i32 %x) #0 {
; V4T-LABEL: sat_base_32bit:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI0_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI0_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_base_32bit:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp slt i32 %x, 8388607
  %saturateUp = select i1 %0, i32 %x, i32 8388607
  %1 = icmp sgt i32 %saturateUp, -8388608
  %saturateLow = select i1 %1, i32 %saturateUp, i32 -8388608
  ret i32 %saturateLow
}

; x < -k ? -k : (x > k ? k : x)
; 16-bit base test
define i16 @sat_base_16bit(i16 %x) #0 {
; V4T-LABEL: sat_base_16bit:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r2, #255
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    orr r2, r2, #1792
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmp r1, r2
; V4T-NEXT:    movge r0, r2
; V4T-NEXT:    ldr r2, .LCPI1_0
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmn r1, #2048
; V4T-NEXT:    movle r0, r2
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI1_0:
; V4T-NEXT:    .long 4294965248 @ 0xfffff800
;
; V6T2-LABEL: sat_base_16bit:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    movw r2, #2047
; V6T2-NEXT:    cmp r1, r2
; V6T2-NEXT:    movge r0, r2
; V6T2-NEXT:    movw r2, #63488
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    movt r2, #65535
; V6T2-NEXT:    cmn r1, #2048
; V6T2-NEXT:    movle r0, r2
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp slt i16 %x, 2047
  %saturateUp = select i1 %0, i16 %x, i16 2047
  %1 = icmp sgt i16 %saturateUp, -2048
  %saturateLow = select i1 %1, i16 %saturateUp, i16 -2048
  ret i16 %saturateLow
}

; x < -k ? -k : (x > k ? k : x)
; 8-bit base test
define i8 @sat_base_8bit(i8 %x) #0 {
; V4T-LABEL: sat_base_8bit:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    lsl r1, r0, #24
; V4T-NEXT:    asr r1, r1, #24
; V4T-NEXT:    cmp r1, #31
; V4T-NEXT:    movge r0, #31
; V4T-NEXT:    lsl r1, r0, #24
; V4T-NEXT:    asr r1, r1, #24
; V4T-NEXT:    cmn r1, #32
; V4T-NEXT:    mvnle r0, #31
; V4T-NEXT:    bx lr
;
; V6T2-LABEL: sat_base_8bit:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    sxtb r1, r0
; V6T2-NEXT:    cmp r1, #31
; V6T2-NEXT:    movge r0, #31
; V6T2-NEXT:    sxtb r1, r0
; V6T2-NEXT:    cmn r1, #32
; V6T2-NEXT:    mvnle r0, #31
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp slt i8 %x, 31
  %saturateUp = select i1 %0, i8 %x, i8 31
  %1 = icmp sgt i8 %saturateUp, -32
  %saturateLow = select i1 %1, i8 %saturateUp, i8 -32
  ret i8 %saturateLow
}

;
; Tests where the conditionals that check for upper and lower bounds,
; or the < and > operators, are arranged in different ways. Only some
; of the possible combinations that lead to SSAT are tested.
;

; x < -k ? -k : (x < k ? x : k)
define i32 @sat_lower_upper_1(i32 %x) #0 {
; V4T-LABEL: sat_lower_upper_1:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI3_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI3_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_lower_upper_1:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %cmpUp = icmp slt i32 %x, 8388607
  %saturateUp = select i1 %cmpUp, i32 %x, i32 8388607
  %0 = icmp sgt i32 %saturateUp, -8388608
  %saturateLow = select i1 %0, i32 %saturateUp, i32 -8388608
  ret i32 %saturateLow
}

; x > -k ? (x > k ? k : x) : -k
define i32 @sat_lower_upper_2(i32 %x) #0 {
; V4T-LABEL: sat_lower_upper_2:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI4_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI4_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_lower_upper_2:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp slt i32 %x, 8388607
  %saturateUp = select i1 %0, i32 %x, i32 8388607
  %1 = icmp sgt i32 %saturateUp, -8388608
  %saturateLow = select i1 %1, i32 %saturateUp, i32 -8388608
  ret i32 %saturateLow
}

; x < k ? (x < -k ? -k : x) : k
define i32 @sat_upper_lower_1(i32 %x) #0 {
; V4T-LABEL: sat_upper_lower_1:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI5_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI5_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_upper_lower_1:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp sgt i32 %x, -8388608
  %saturateLow = select i1 %0, i32 %x, i32 -8388608
  %1 = icmp slt i32 %saturateLow, 8388607
  %saturateUp = select i1 %1, i32 %saturateLow, i32 8388607
  ret i32 %saturateUp
}

; x > k ? k : (x < -k ? -k : x)
define i32 @sat_upper_lower_2(i32 %x) #0 {
; V4T-LABEL: sat_upper_lower_2:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI6_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI6_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_upper_lower_2:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp sgt i32 %x, -8388608
  %saturateLow = select i1 %0, i32 %x, i32 -8388608
  %1 = icmp slt i32 %saturateLow, 8388607
  %saturateUp = select i1 %1, i32 %saturateLow, i32 8388607
  ret i32 %saturateUp
}

; k < x ? k : (x > -k ? x : -k)
define i32 @sat_upper_lower_3(i32 %x) #0 {
; V4T-LABEL: sat_upper_lower_3:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI7_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI7_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_upper_lower_3:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %cmpLow = icmp sgt i32 %x, -8388608
  %saturateLow = select i1 %cmpLow, i32 %x, i32 -8388608
  %0 = icmp slt i32 %saturateLow, 8388607
  %saturateUp = select i1 %0, i32 %saturateLow, i32 8388607
  ret i32 %saturateUp
}

;
; Miscellanea
;

; Check that >= and <= work the same as > and <
; k <= x ? k : (x >= -k ? x : -k)
define i32 @sat_le_ge(i32 %x) #0 {
; V4T-LABEL: sat_le_ge:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI8_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI8_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: sat_le_ge:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    ssat r0, #24, r0
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp sgt i32 %x, -8388608
  %saturateLow = select i1 %0, i32 %x, i32 -8388608
  %1 = icmp slt i32 %saturateLow, 8388607
  %saturateUp = select i1 %1, i32 %saturateLow, i32 8388607
  ret i32 %saturateUp
}

;
; The following tests check for patterns that should not transform
; into SSAT but are similar enough that could confuse the selector.
;

; x > k ? k : (x > -k ? -k : x)
; First condition upper-saturates, second doesn't lower-saturate.
define i32 @no_sat_missing_lower(i32 %x) #0 {
; V4T-LABEL: no_sat_missing_lower:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r2, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r2, r2, #-1073741824
; V4T-NEXT:    ldr r1, .LCPI9_0
; V4T-NEXT:    movlt r2, r0
; V4T-NEXT:    cmp r0, #8388608
; V4T-NEXT:    movlt r1, r2
; V4T-NEXT:    mov r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI9_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: no_sat_missing_lower:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    movlt r1, r0
; V6T2-NEXT:    cmp r0, #8388608
; V6T2-NEXT:    movwge r1, #65535
; V6T2-NEXT:    movtge r1, #127
; V6T2-NEXT:    mov r0, r1
; V6T2-NEXT:    bx lr
entry:
  %cmpUp = icmp sgt i32 %x, 8388607
  %0 = icmp slt i32 %x, -8388608
  %saturateLow = select i1 %0, i32 %x, i32 -8388608
  %saturateUp = select i1 %cmpUp, i32 8388607, i32 %saturateLow
  ret i32 %saturateUp
}

; x < k ? k : (x < -k ? -k : x)
; Second condition lower-saturates, first doesn't upper-saturate.
define i32 @no_sat_missing_upper(i32 %x) #0 {
; V4T-LABEL: no_sat_missing_upper:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    ldr r2, .LCPI10_0
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movgt r1, r0
; V4T-NEXT:    cmp r0, r2
; V4T-NEXT:    movlt r1, r2
; V4T-NEXT:    mov r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI10_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: no_sat_missing_upper:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    movw r2, #65535
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movgt r1, r0
; V6T2-NEXT:    movt r2, #127
; V6T2-NEXT:    cmp r0, r2
; V6T2-NEXT:    movwlt r1, #65535
; V6T2-NEXT:    movtlt r1, #127
; V6T2-NEXT:    mov r0, r1
; V6T2-NEXT:    bx lr
entry:
  %cmpUp = icmp slt i32 %x, 8388607
  %0 = icmp sgt i32 %x, -8388608
  %saturateLow = select i1 %0, i32 %x, i32 -8388608
  %saturateUp = select i1 %cmpUp, i32 8388607, i32 %saturateLow
  ret i32 %saturateUp
}

; Lower constant is different in the select and in the compare
define i32 @no_sat_incorrect_constant(i32 %x) #0 {
; V4T-LABEL: no_sat_incorrect_constant:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    mov r2, r0
; V4T-NEXT:    orrlt r2, r1, #1
; V4T-NEXT:    ldr r1, .LCPI11_0
; V4T-NEXT:    cmp r0, #8388608
; V4T-NEXT:    movlt r1, r2
; V4T-NEXT:    mov r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI11_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: no_sat_incorrect_constant:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r2, #0
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    mov r1, r0
; V6T2-NEXT:    movt r2, #65408
; V6T2-NEXT:    orrlt r1, r2, #1
; V6T2-NEXT:    cmp r0, #8388608
; V6T2-NEXT:    movwge r1, #65535
; V6T2-NEXT:    movtge r1, #127
; V6T2-NEXT:    mov r0, r1
; V6T2-NEXT:    bx lr
entry:
  %cmpUp = icmp sgt i32 %x, 8388607
  %cmpLow = icmp slt i32 %x, -8388608
  %saturateLow = select i1 %cmpLow, i32 -8388607, i32 %x
  %saturateUp = select i1 %cmpUp, i32 8388607, i32 %saturateLow
  ret i32 %saturateUp
}

; The interval is not [k, ~k]
define i32 @no_sat_incorrect_interval(i32 %x) #0 {
; V4T-LABEL: no_sat_incorrect_interval:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI12_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI12_1
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI12_0:
; V4T-NEXT:    .long 4275878552 @ 0xfedcba98
; V4T-NEXT:  .LCPI12_1:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: no_sat_incorrect_interval:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #47768
; V6T2-NEXT:    movt r1, #65244
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = icmp sgt i32 %x, -19088744
  %saturateLow = select i1 %0, i32 %x, i32 -19088744
  %1 = icmp slt i32 %saturateLow, 8388607
  %saturateUp = select i1 %1, i32 %saturateLow, i32 8388607
  ret i32 %saturateUp
}

; The returned value (y) is not the same as the tested value (x).
define i32 @no_sat_incorrect_return(i32 %x, i32 %y) #0 {
; V4T-LABEL: no_sat_incorrect_return:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r2, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r2, r2, #-1073741824
; V4T-NEXT:    movge r2, r1
; V4T-NEXT:    ldr r1, .LCPI13_0
; V4T-NEXT:    cmp r0, #8388608
; V4T-NEXT:    movlt r1, r2
; V4T-NEXT:    mov r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI13_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: no_sat_incorrect_return:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movwlt r1, #0
; V6T2-NEXT:    movtlt r1, #65408
; V6T2-NEXT:    cmp r0, #8388608
; V6T2-NEXT:    movwge r1, #65535
; V6T2-NEXT:    movtge r1, #127
; V6T2-NEXT:    mov r0, r1
; V6T2-NEXT:    bx lr
entry:
  %cmpUp = icmp sgt i32 %x, 8388607
  %cmpLow = icmp slt i32 %x, -8388608
  %saturateLow = select i1 %cmpLow, i32 -8388608, i32 %y
  %saturateUp = select i1 %cmpUp, i32 8388607, i32 %saturateLow
  ret i32 %saturateUp
}

; One of the values in a compare (y) is not the same as the rest
; of the compare and select values (x).
define i32 @no_sat_incorrect_compare(i32 %x, i32 %y) #0 {
; V4T-LABEL: no_sat_incorrect_compare:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r2, #1065353216
; V4T-NEXT:    cmn r1, #8388608
; V4T-NEXT:    orr r2, r2, #-1073741824
; V4T-NEXT:    ldr r1, .LCPI14_0
; V4T-NEXT:    movge r2, r0
; V4T-NEXT:    cmp r0, #8388608
; V4T-NEXT:    movlt r1, r2
; V4T-NEXT:    mov r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI14_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: no_sat_incorrect_compare:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    cmn r1, #8388608
; V6T2-NEXT:    mov r1, r0
; V6T2-NEXT:    movwlt r1, #0
; V6T2-NEXT:    movtlt r1, #65408
; V6T2-NEXT:    cmp r0, #8388608
; V6T2-NEXT:    movwge r1, #65535
; V6T2-NEXT:    movtge r1, #127
; V6T2-NEXT:    mov r0, r1
; V6T2-NEXT:    bx lr
entry:
  %cmpUp = icmp sgt i32 %x, 8388607
  %cmpLow = icmp slt i32 %y, -8388608
  %saturateLow = select i1 %cmpLow, i32 -8388608, i32 %x
  %saturateUp = select i1 %cmpUp, i32 8388607, i32 %saturateLow
  ret i32 %saturateUp
}

define void @extended(i32 %xx, i16 signext %y, i8* nocapture %z) {
; V4T-LABEL: extended:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    add r0, r1, r0, lsr #16
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmp r1, #127
; V4T-NEXT:    movge r0, #127
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmn r1, #128
; V4T-NEXT:    mvnle r0, #127
; V4T-NEXT:    strb r0, [r2]
; V4T-NEXT:    bx lr
;
; V6T2-LABEL: extended:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    add r0, r1, r0, lsr #16
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    cmp r1, #127
; V6T2-NEXT:    movge r0, #127
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    cmn r1, #128
; V6T2-NEXT:    mvnle r0, #127
; V6T2-NEXT:    strb r0, [r2]
; V6T2-NEXT:    bx lr
entry:
  %0 = lshr i32 %xx, 16
  %1 = trunc i32 %0 to i16
  %conv3 = add i16 %1, %y
  %cmp.i = icmp slt i16 %conv3, 127
  %cond.i = select i1 %cmp.i, i16 %conv3, i16 127
  %cmp.i11 = icmp sgt i16 %cond.i, -128
  %cond.i12 = select i1 %cmp.i11, i16 %cond.i, i16 -128
  %conv5 = trunc i16 %cond.i12 to i8
  store i8 %conv5, i8* %z, align 1
  ret void
}


define i32 @formulated_valid(i32 %a) {
; V4T-LABEL: formulated_valid:
; V4T:       @ %bb.0:
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmp r1, #127
; V4T-NEXT:    movge r0, #127
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmn r1, #128
; V4T-NEXT:    mov r1, #255
; V4T-NEXT:    mvnle r0, #127
; V4T-NEXT:    orr r1, r1, #65280
; V4T-NEXT:    and r0, r0, r1
; V4T-NEXT:    bx lr
;
; V6T2-LABEL: formulated_valid:
; V6T2:       @ %bb.0:
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    cmp r1, #127
; V6T2-NEXT:    movge r0, #127
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    cmn r1, #128
; V6T2-NEXT:    mvnle r0, #127
; V6T2-NEXT:    uxth r0, r0
; V6T2-NEXT:    bx lr
  %x1 = trunc i32 %a to i16
  %x2 = sext i16 %x1 to i32
  %c1 = icmp slt i32 %x2, 127
  %s1 = select i1 %c1, i32 %a, i32 127
  %y1 = trunc i32 %s1 to i16
  %y2 = sext i16 %y1 to i32
  %c2 = icmp sgt i32 %y2, -128
  %s2 = select i1 %c2, i32 %s1, i32 -128
  %r = and i32 %s2, 65535
  ret i32 %r
}

define i32 @formulated_invalid(i32 %a) {
; V4T-LABEL: formulated_invalid:
; V4T:       @ %bb.0:
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmp r1, #127
; V4T-NEXT:    movge r0, #127
; V4T-NEXT:    lsl r1, r0, #16
; V4T-NEXT:    asr r1, r1, #16
; V4T-NEXT:    cmn r1, #128
; V4T-NEXT:    mvnle r0, #127
; V4T-NEXT:    bic r0, r0, #-16777216
; V4T-NEXT:    bx lr
;
; V6T2-LABEL: formulated_invalid:
; V6T2:       @ %bb.0:
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    cmp r1, #127
; V6T2-NEXT:    movge r0, #127
; V6T2-NEXT:    sxth r1, r0
; V6T2-NEXT:    cmn r1, #128
; V6T2-NEXT:    mvnle r0, #127
; V6T2-NEXT:    bic r0, r0, #-16777216
; V6T2-NEXT:    bx lr
  %x1 = trunc i32 %a to i16
  %x2 = sext i16 %x1 to i32
  %c1 = icmp slt i32 %x2, 127
  %s1 = select i1 %c1, i32 %a, i32 127
  %y1 = trunc i32 %s1 to i16
  %y2 = sext i16 %y1 to i32
  %c2 = icmp sgt i32 %y2, -128
  %s2 = select i1 %c2, i32 %s1, i32 -128
  %r = and i32 %s2, 16777215
  ret i32 %r
}


define i32 @mm_sat_base_32bit(i32 %x) {
; V4T-LABEL: mm_sat_base_32bit:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI18_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI18_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_base_32bit:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smin.i32(i32 %x, i32 8388607)
  %1 = call i32 @llvm.smax.i32(i32 %0, i32 -8388608)
  ret i32 %1
}

define i16 @mm_sat_base_16bit(i16 %x) {
; V4T-LABEL: mm_sat_base_16bit:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r2, #255
; V4T-NEXT:    lsl r0, r0, #16
; V4T-NEXT:    orr r2, r2, #1792
; V4T-NEXT:    asr r1, r0, #16
; V4T-NEXT:    cmp r1, r2
; V4T-NEXT:    asrlt r2, r0, #16
; V4T-NEXT:    ldr r0, .LCPI19_0
; V4T-NEXT:    cmn r2, #2048
; V4T-NEXT:    movgt r0, r2
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI19_0:
; V4T-NEXT:    .long 4294965248 @ 0xfffff800
;
; V6T2-LABEL: mm_sat_base_16bit:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    sxth r0, r0
; V6T2-NEXT:    movw r1, #2047
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movlt r1, r0
; V6T2-NEXT:    movw r0, #63488
; V6T2-NEXT:    movt r0, #65535
; V6T2-NEXT:    cmn r1, #2048
; V6T2-NEXT:    movgt r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i16 @llvm.smin.i16(i16 %x, i16 2047)
  %1 = call i16 @llvm.smax.i16(i16 %0, i16 -2048)
  ret i16 %1
}

define i8 @mm_sat_base_8bit(i8 %x) {
; V4T-LABEL: mm_sat_base_8bit:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    lsl r1, r0, #24
; V4T-NEXT:    mov r0, #31
; V4T-NEXT:    asr r2, r1, #24
; V4T-NEXT:    cmp r2, #31
; V4T-NEXT:    asrlt r0, r1, #24
; V4T-NEXT:    cmn r0, #32
; V4T-NEXT:    mvnle r0, #31
; V4T-NEXT:    bx lr
;
; V6T2-LABEL: mm_sat_base_8bit:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    sxtb r0, r0
; V6T2-NEXT:    cmp r0, #31
; V6T2-NEXT:    movge r0, #31
; V6T2-NEXT:    cmn r0, #32
; V6T2-NEXT:    mvnle r0, #31
; V6T2-NEXT:    bx lr
entry:
  %0 = call i8 @llvm.smin.i8(i8 %x, i8 31)
  %1 = call i8 @llvm.smax.i8(i8 %0, i8 -32)
  ret i8 %1
}

define i32 @mm_sat_lower_upper_1(i32 %x) {
; V4T-LABEL: mm_sat_lower_upper_1:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI21_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI21_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_lower_upper_1:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smin.i32(i32 %x, i32 8388607)
  %1 = call i32 @llvm.smax.i32(i32 %0, i32 -8388608)
  ret i32 %1
}

define i32 @mm_sat_lower_upper_2(i32 %x) {
; V4T-LABEL: mm_sat_lower_upper_2:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI22_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI22_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_lower_upper_2:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smin.i32(i32 %x, i32 8388607)
  %1 = call i32 @llvm.smax.i32(i32 %0, i32 -8388608)
  ret i32 %1
}

define i32 @mm_sat_upper_lower_1(i32 %x) {
; V4T-LABEL: mm_sat_upper_lower_1:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI23_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI23_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_upper_lower_1:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smax.i32(i32 %x, i32 -8388608)
  %1 = call i32 @llvm.smin.i32(i32 %0, i32 8388607)
  ret i32 %1
}

define i32 @mm_sat_upper_lower_2(i32 %x) {
; V4T-LABEL: mm_sat_upper_lower_2:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI24_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI24_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_upper_lower_2:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smax.i32(i32 %x, i32 -8388608)
  %1 = call i32 @llvm.smin.i32(i32 %0, i32 8388607)
  ret i32 %1
}

define i32 @mm_sat_upper_lower_3(i32 %x) {
; V4T-LABEL: mm_sat_upper_lower_3:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI25_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI25_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_upper_lower_3:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smax.i32(i32 %x, i32 -8388608)
  %1 = call i32 @llvm.smin.i32(i32 %0, i32 8388607)
  ret i32 %1
}

define i32 @mm_sat_le_ge(i32 %x) {
; V4T-LABEL: mm_sat_le_ge:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    mov r1, #1065353216
; V4T-NEXT:    cmn r0, #8388608
; V4T-NEXT:    orr r1, r1, #-1073741824
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI26_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI26_0:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_sat_le_ge:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #0
; V6T2-NEXT:    cmn r0, #8388608
; V6T2-NEXT:    movt r1, #65408
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smax.i32(i32 %x, i32 -8388608)
  %1 = call i32 @llvm.smin.i32(i32 %0, i32 8388607)
  ret i32 %1
}

define i32 @mm_no_sat_incorrect_interval(i32 %x) {
; V4T-LABEL: mm_no_sat_incorrect_interval:
; V4T:       @ %bb.0: @ %entry
; V4T-NEXT:    ldr r1, .LCPI27_0
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movle r0, r1
; V4T-NEXT:    ldr r1, .LCPI27_1
; V4T-NEXT:    cmp r0, r1
; V4T-NEXT:    movge r0, r1
; V4T-NEXT:    bx lr
; V4T-NEXT:    .p2align 2
; V4T-NEXT:  @ %bb.1:
; V4T-NEXT:  .LCPI27_0:
; V4T-NEXT:    .long 4275878552 @ 0xfedcba98
; V4T-NEXT:  .LCPI27_1:
; V4T-NEXT:    .long 8388607 @ 0x7fffff
;
; V6T2-LABEL: mm_no_sat_incorrect_interval:
; V6T2:       @ %bb.0: @ %entry
; V6T2-NEXT:    movw r1, #47768
; V6T2-NEXT:    movt r1, #65244
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movle r0, r1
; V6T2-NEXT:    movw r1, #65535
; V6T2-NEXT:    movt r1, #127
; V6T2-NEXT:    cmp r0, r1
; V6T2-NEXT:    movge r0, r1
; V6T2-NEXT:    bx lr
entry:
  %0 = call i32 @llvm.smax.i32(i32 %x, i32 -19088744)
  %1 = call i32 @llvm.smin.i32(i32 %0, i32 8388607)
  ret i32 %1
}

declare i32 @llvm.smin.i32(i32, i32)
declare i32 @llvm.smax.i32(i32, i32)
declare i16 @llvm.smin.i16(i16, i16)
declare i16 @llvm.smax.i16(i16, i16)
declare i8 @llvm.smin.i8(i8, i8)
declare i8 @llvm.smax.i8(i8, i8)


