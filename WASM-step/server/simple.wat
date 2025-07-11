(module
  (func (export "main") (result i32)
    i32.const 2
    i32.const 3
    i32.add      ;; 2 + 3 = 5
    i32.const 4
    i32.mul      ;; 5 * 4 = 20
  )
)