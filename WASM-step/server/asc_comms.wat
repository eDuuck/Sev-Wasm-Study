(module
  ;; Memory and table setup
  (memory 1)
  (export "memory" (memory 0))

  ;; Main function
  (func $main


    ;; --- Control Instructions ---


    ;; --- Parametric ---
    i32.const 1
    ;; drop later
    i32.const 2
    i32.const 3
    select

    ;; --- Variable ---
    i32.const 10


    ;; --- Memory ---
    i32.const 0
    i32.const 1234
    i32.store ;; store 1234 at 0
    i32.const 0
    i32.load ;; load it back

    ;; --- Constants ---
    i32.const 42
    i64.const 12345678
    f32.const 1.5
    f64.const 2.25

    ;; --- Comparison ---
    i32.const 1
    i32.eqz

    i64.const 2
    i64.eqz

    f32.const 1.0
    f32.const 2.0
    f32.lt

    ;; --- Numeric ---
    i32.const 5
    i32.const 3
    i32.add

    i64.const 7
    i64.const 2
    i64.mul

    f32.const 1.1
    f32.const 2.2
    f32.add

    f64.const 1.5
    f64.const 3.5
    f64.div

    ;; --- Conversion ---
    i64.const 42
    i32.wrap_i64

    f64.const 6.28
    f32.demote_f64

    i32.const 11

    ;; --- Drop everything at the end ---
    drop ;; convert_s/i32 result
    drop ;; demote_f64 result
    drop ;; wrap_i64 result
    drop ;; f64.div
    drop ;; f32.add
    drop ;; i64.mul
    drop ;; i32.add
    drop ;; f32.lt
    drop ;; i64.eqz
    drop ;; i32.eqz
    drop ;; f64.const
    drop ;; f32.const
    drop ;; i64.const
    drop ;; i32.const
    drop ;; i32.load
    drop ;; tee_local result
    drop ;; select result
  )

  ;; Export main
  (export "main" (func $main))

)
