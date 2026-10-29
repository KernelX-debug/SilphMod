.text
.macro save_context
    sub sp, sp, #800
    .cfi_def_cfa_offset 800
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    .cfi_offset 19, -648
    .cfi_offset 20, -640
    .cfi_offset 21, -632
    .cfi_offset 22, -624
    .cfi_offset 23, -616
    .cfi_offset 24, -608
    .cfi_offset 25, -600
    .cfi_offset 26, -592
    .cfi_offset 27, -584
    .cfi_offset 28, -576
    .cfi_offset 29, -568
    .cfi_offset 30, -560
    add x9, sp, #800
    str x9, [sp, #248]
    mrs x9, nzcv
    str x9, [sp, #256]
    mrs x9, fpcr
    str x9, [sp, #264]
    mrs x9, fpsr
    str x9, [sp, #272]
    stp q0, q1, [sp, #288]
    stp q2, q3, [sp, #320]
    stp q4, q5, [sp, #352]
    stp q6, q7, [sp, #384]
    stp q8, q9, [sp, #416]
    stp q10, q11, [sp, #448]
    stp q12, q13, [sp, #480]
    stp q14, q15, [sp, #512]
    stp q16, q17, [sp, #544]
    stp q18, q19, [sp, #576]
    stp q20, q21, [sp, #608]
    stp q22, q23, [sp, #640]
    stp q24, q25, [sp, #672]
    stp q26, q27, [sp, #704]
    stp q28, q29, [sp, #736]
    stp q30, q31, [sp, #768]
.endm
.macro restore_context
    ldr x9, [sp, #264]
    msr fpcr, x9
    ldr x9, [sp, #272]
    msr fpsr, x9
    ldr x9, [sp, #256]
    msr nzcv, x9
    ldp q0, q1, [sp, #288]
    ldp q2, q3, [sp, #320]
    ldp q4, q5, [sp, #352]
    ldp q6, q7, [sp, #384]
    ldp q8, q9, [sp, #416]
    ldp q10, q11, [sp, #448]
    ldp q12, q13, [sp, #480]
    ldp q14, q15, [sp, #512]
    ldp q16, q17, [sp, #544]
    ldp q18, q19, [sp, #576]
    ldp q20, q21, [sp, #608]
    ldp q22, q23, [sp, #640]
    ldp q24, q25, [sp, #672]
    ldp q26, q27, [sp, #704]
    ldp q28, q29, [sp, #736]
    ldp q30, q31, [sp, #768]
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
.endm
.macro event_gate name, decide, original
    .p2align 2
    .global \name
    .hidden \name
    .type \name, %function
\name:
    .cfi_startproc
    save_context
    mov x0, sp
    bl \decide
    cbnz x0, 1f
    .cfi_remember_state
    restore_context
    adrp x17, \original
    ldr x17, [x17, :lo12:\original]
    add sp, sp, #800
    .cfi_def_cfa_offset 0
    br x17
1:
    .cfi_restore_state
    restore_context
    add sp, sp, #800
    .cfi_def_cfa_offset 0
    ret
    .cfi_endproc
    .size \name, .-\name
.endm
event_gate silph_event_enter_gate, silph_event_enter_decide, silph_event_enter_original
event_gate silph_event_selected_gate, silph_event_enter_decide, silph_event_selected_original
.section .note.GNU-stack,"",%progbits
