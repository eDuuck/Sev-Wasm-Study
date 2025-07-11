(module
  (func (export "main") (result i32)
    ;; First operation
    i32.const 2
    i32.const 3
    i32.add      ;; = 5
    i32.const 4
    i32.mul      ;; = 20

    ;; Second operation
    i32.const 2
    i32.const 3
    i32.add      ;; = 5
    i32.const 4
    i32.mul      ;; = 20

    i32.add      ;; 20 + 20 = 40
  )
)
