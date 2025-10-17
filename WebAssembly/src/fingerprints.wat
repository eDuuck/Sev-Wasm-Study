(module
  ;; 1 page (64 KiB) of linear memory, 2 globals
  (memory (export "memory") 1)
  (global $g_i32 (mut i32) (i32.const 42))
  (global $f_32 (mut f32) (f32.const 3.14))
  (type $ind_call (func ))

  ;; Function table for indirect calls
  (table funcref (elem $ind_func))

  ;; Actual function implementation (the real target)
  (func $ind_func (type $ind_call) 
    nop
  )
  ;; This function performs an indirect call
  (func $indirect_caller
    ;; call index 0 in table → $ind_func
    (call_indirect (i32.const 0))
  )
  

  (func (export "main") (param $x i32) (result i32)
    (local $tmp i32)

    ;; Op codes (00 - 0F)
    ;; 00 (unreachable) only occurs when x = 0
    ;; 0F (return)      only occurs when x = 0xdeadbeef
    (i32.eq (i32.const 0) (local.get $x))
    (if
      (then
        unreachable  ;; unreachable -> trap
      )
    )
    (i32.eq (i32.const 0xdeadbeef) (local.get $x))
    (if
      (then
        (return (i32.const 0xbadfade5))  ;; unreachable -> trap
      )
      (else
        nop
      )
    )
    (i32.eq (i32.const 1) (i32.const 1))
    (if
      (then
        nop ;;
      )
    )

    ;; block, loop, br_if, br, br_table
    ;;      + Local get/set/tee  (OP Codes (20 - 22))
    (local.set $tmp (i32.const 3))
    (block $exit               ;; exit block
      (loop $loop1 
        (i32.sub (local.get $tmp) (i32.const 1))
        local.tee $tmp
        (block $default
            local.get $tmp
            br_table $exit $default ;; 0 → $case0, otherwise → $default
        )
        ;; default:
        i32.const 1
        i32.gt_s
        br_if $loop1
        br $loop1
      )
    )

    ;;Call and call_inderect
    ;;      OP Codes (10 - 11)
    call $indirect_caller

    
    ;; drop / select
    ;;      OP Codes (1A - 1B)
    (select (i32.const 10) (i32.const 20) (i32.const 0))
    drop

    ;; global get/set (for i32 and f32)
    ;;      OP Codes (23 - 24)
    global.get $g_i32
    (global.set $g_i32 (i32.const 1) (i32.add))

    global.get $f_32
    (global.set $f_32 (f32.const 3.14) (f32.add))

    ;; ---- Memory ops ---------------------------------------------------------------
    ;; Store instructions starting at adress 0x100
    ;;      OP Codes (36 - 3E)
    (i32.store (i32.const 0x100) (i32.const 0x7F7F7F7F))
    (i64.store (i32.const 0x104) (i64.const 0x1122334455667788))
    (f32.store (i32.const 0x10C) (f32.const 1.25))
    (f64.store (i32.const 0x110) (f64.const 3.5))

    ;; store8/16/32 variants
    (i32.store8  (i32.const 0x118) (i32.const 0xAB))
    (i32.store16 (i32.const 0x11A) (i32.const 0xCDEF))
    (i64.store8  (i32.const 0x11C) (i64.const 0x99))
    (i64.store16 (i32.const 0x11E) (i64.const 0x7788))
    (i64.store32 (i32.const 0x120) (i64.const 0xAABBCCDD))


    ;; plain loads (These are before the stores in opcode values
    ;;              but have to be performed after to have existing
    ;;              memory to load.)
    ;;      OP Codes (28 - 2B)
    
    (i32.load (i32.const 0x100)) drop
    (i64.load (i32.const 0x104)) drop
    (f32.load (i32.const 0x10C)) drop
    (f64.load (i32.const 0x110)) drop

    ;; loads with sign/zero extensions
    ;;      OP Codes (2C - 35)
    (i32.load8_s  (i32.const 0x118)) drop
    (i32.load8_u  (i32.const 0x118)) drop
    (i32.load16_s (i32.const 0x11A)) drop
    (i32.load16_u (i32.const 0x11A)) drop
    
    (i64.load8_s  (i32.const 0x11C)) drop
    (i64.load8_u  (i32.const 0x11C)) drop
    
    (i64.load16_s (i32.const 0x11E)) drop
    (i64.load16_u (i32.const 0x11E)) drop
    (i64.load32_s (i32.const 0x120)) drop
    (i64.load32_u (i32.const 0x120)) drop

    ;; memory.size / memory.grow (grow by 0/1/2 to see if difference)
    ;;      OP Codes (3F - 40)
    memory.size drop
    (memory.grow (i32.const 0)) drop
    (memory.grow (i32.const 1)) drop
    (memory.grow (i32.const 2)) drop

    ;; ---- Constants (not really needed as we use this everywhere) ----
    ;;      OP Codes (41 - 44)
    i32.const 5
    i64.const 5
    f32.const 3.14
    f64.const 3.14
    drop drop drop drop
    
    ;; ---- Comparisons --------
    ;;      OP Codes (45-66)
    (i32.eqz (i32.const 0)) drop
    (i32.eq (i32.const 5) (i32.const 5)) drop
    (i32.ne (i32.const 5) (i32.const 6)) drop
    (i32.lt_s (i32.const -1) (i32.const 0)) drop
    (i32.lt_u (i32.const 1) (i32.const 2)) drop
    (i32.gt_s (i32.const 3) (i32.const 1)) drop
    (i32.gt_u (i32.const 3) (i32.const 1)) drop
    (i32.le_s (i32.const 3) (i32.const 3)) drop
    (i32.le_u (i32.const 3) (i32.const 3)) drop
    (i32.ge_s (i32.const 4) (i32.const 3)) drop
    (i32.ge_u (i32.const 4) (i32.const 3)) drop

    (i64.eqz (i64.const 0)) drop
    (i64.eq (i64.const 7) (i64.const 7)) drop
    (i64.ne (i64.const 7) (i64.const 8)) drop
    (i64.lt_s (i64.const -2) (i64.const -1)) drop
    (i64.lt_u (i64.const 2) (i64.const 3)) drop
    (i64.gt_s (i64.const 9) (i64.const 8)) drop
    (i64.gt_u (i64.const 9) (i64.const 8)) drop
    (i64.le_s (i64.const 5) (i64.const 5)) drop
    (i64.le_u (i64.const 5) (i64.const 5)) drop
    (i64.ge_s (i64.const 6) (i64.const 5)) drop
    (i64.ge_u (i64.const 6) (i64.const 5)) drop

    (f32.eq (f32.const 1.0) (f32.const 1.0)) drop
    (f32.ne (f32.const 1.0) (f32.const 0.5)) drop
    (f32.lt (f32.const 1.0) (f32.const 2.0)) drop
    (f32.gt (f32.const 2.0) (f32.const 1.0)) drop
    (f32.le (f32.const 1.0) (f32.const 1.5)) drop
    (f32.ge (f32.const 2.0) (f32.const 2.0)) drop

    (f64.eq (f64.const 1.0) (f64.const 1.0)) drop
    (f64.ne (f64.const 1.0) (f64.const 0.5)) drop
    (f64.lt (f64.const 1.0) (f64.const 2.0)) drop
    (f64.gt (f64.const 2.0) (f64.const 1.0)) drop
    (f64.le (f64.const 1.0) (f64.const 1.5)) drop
    (f64.ge (f64.const 2.0) (f64.const 2.0)) drop

    ;; ---- Numeric ops --------(WASM codes 67-9F) ----
    ;;      OP Codes (67-9F)
    (i32.clz (i32.const 12)) drop
    (i32.ctz (i32.const 12)) drop
    (i32.popcnt (i32.const 12)) drop
    (i32.add (i32.const 7) (i32.const 3)) drop
    (i32.sub (i32.const 7) (i32.const 3)) drop
    (i32.mul (i32.const 7) (i32.const 3)) drop
    (i32.div_s (i32.const 7) (i32.const 3)) drop
    (i32.div_u (i32.const 7) (i32.const 3)) drop
    (i32.rem_s (i32.const 7) (i32.const 3)) drop
    (i32.rem_u (i32.const 7) (i32.const 3)) drop
    (i32.and (i32.const 0x55) (i32.const 0x0F)) drop
    (i32.or (i32.const 0x55) (i32.const 0x0F)) drop
    (i32.xor (i32.const 0x55) (i32.const 0x0F)) drop
    (i32.shl (i32.const 1) (i32.const 4)) drop
    (i32.shr_s (i32.const -8) (i32.const 1)) drop
    (i32.shr_u (i32.const 0xFF00) (i32.const 8)) drop
    (i32.rotl (i32.const 0x12345678) (i32.const 4)) drop
    (i32.rotr (i32.const 0x12345678) (i32.const 4)) drop

    (i64.clz (i64.const 12)) drop
    (i64.ctz (i64.const 12)) drop
    (i64.popcnt (i64.const 12)) drop
    (i64.add (i64.const 7) (i64.const 3)) drop
    (i64.sub (i64.const 7) (i64.const 3)) drop
    (i64.mul (i64.const 7) (i64.const 3)) drop
    (i64.div_s (i64.const 7) (i64.const 3)) drop
    (i64.div_u (i64.const 7) (i64.const 3)) drop
    (i64.rem_s (i64.const 7) (i64.const 3)) drop
    (i64.rem_u (i64.const 7) (i64.const 3)) drop
    (i64.and (i64.const 0x55) (i64.const 0x0F)) drop
    (i64.or (i64.const 0x55) (i64.const 0x0F)) drop
    (i64.xor (i64.const 0x55) (i64.const 0x0F)) drop
    (i64.shl (i64.const 1) (i64.const 5)) drop
    (i64.shr_s (i64.const -8) (i64.const 1)) drop
    (i64.shr_u (i64.const 0xFF00) (i64.const 8)) drop
    (i64.rotl (i64.const 0x1122334455667788) (i64.const 8)) drop
    (i64.rotr (i64.const 0x1122334455667788) (i64.const 8)) drop

    (f32.abs (f32.const 3.5)) drop
    (f32.neg (f32.const -3.5)) drop
    (f32.ceil (f32.const 3.5)) drop
    (f32.floor (f32.const 3.5)) drop
    (f32.trunc (f32.const 3.5)) drop
    (f32.nearest (f32.const 3.5)) drop
    (f32.sqrt (f32.const 4.0)) drop
    (f32.add (f32.const 1.25) (f32.const 0.5)) drop
    (f32.sub (f32.const 1.25) (f32.const 0.5)) drop
    (f32.mul (f32.const 1.25) (f32.const 0.5)) drop
    (f32.div (f32.const 1.25) (f32.const 0.5)) drop
    (f32.min (f32.const 1.25) (f32.const 0.5)) drop
    (f32.max (f32.const 1.25) (f32.const 0.5)) drop
    (f32.copysign (f32.const -1.0) (f32.const 2.0)) drop

    (f64.abs (f64.const 3.5)) drop
    (f64.neg (f64.const -3.5)) drop
    (f64.ceil (f64.const 3.5)) drop
    (f64.floor (f64.const 3.5)) drop
    (f64.trunc (f64.const 3.5)) drop
    (f64.nearest (f64.const 3.5)) drop
    (f64.sqrt (f64.const 4.0)) drop
    (f64.add (f64.const 1.5) (f64.const 0.5)) drop
    (f64.sub (f64.const 1.5) (f64.const 0.5)) drop
    (f64.mul (f64.const 1.5) (f64.const 0.5)) drop
    (f64.div (f64.const 1.5) (f64.const 0.5)) drop
    (f64.min (f64.const 1.5) (f64.const 0.5)) drop
    (f64.max (f64.const 1.5) (f64.const 0.5)) drop
    (f64.copysign (f64.const -1.0) (f64.const 2.0)) drop
    
    ;; Various conversions (WASM codes A7-BF)
    ;;      OP Codes (A7-BF)
    (i32.wrap_i64    (i64.const 0x123456789)) drop
    (i32.trunc_f32_s (f32.const 3.14)) drop
    (i32.trunc_f32_u (f32.const 3.14)) drop
    (i32.trunc_f64_s (f64.const 3.14)) drop
    (i32.trunc_f64_u (f64.const 3.14)) drop

    (i64.extend_i32_s (i32.const -10))  drop
    (i64.extend_i32_u (i32.const 10))   drop
    (i64.trunc_f32_s  (f32.const 3.14)) drop
    (i64.trunc_f32_u  (f32.const 3.14)) drop
    (i64.trunc_f64_s  (f64.const 3.14)) drop
    (i64.trunc_f64_u  (f64.const 3.14)) drop

    (f32.convert_i32_s (i32.const -5))   drop
    (f32.convert_i32_u (i32.const 5))    drop
    (f32.convert_i64_s (i64.const -5))   drop
    (f32.convert_i64_u (i64.const 5))    drop
    (f32.demote_f64    (f64.const 3.25)) drop

    (f64.convert_i32_s (i32.const -5))   drop
    (f64.convert_i32_u (i32.const 5))    drop
    (f64.convert_i64_s (i64.const -5))   drop
    (f64.convert_i64_u (i64.const 5))    drop
    (f64.promote_f32   (f32.const 3.25)) drop

    (i32.reinterpret_f32 (f32.const 1.0))         drop
    (i64.reinterpret_f64 (f64.const 1.0))         drop
    (f32.reinterpret_i32 (i32.const 0x3f800000))  drop
    (f64.reinterpret_i64 (i64.const 0x123456789)) drop

    local.get $x i32.const 0xdeadbeef i32.mul ;;Simply to return something "unique" for every input.
  )
)
