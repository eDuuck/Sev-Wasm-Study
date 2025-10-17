(module
  (func (export "main") (result i32)
    (local $x1 i32) (local $x2 i32) (local $i i32)

    ;; int x1 = 7; 
    i32.const 7
    local.set $x1

    ;; int x2 = x1 * x1;
    local.get $x1
    local.get $x1
    i32.mul
    local.set $x2

    ;; for (i = 30; i >= 0; --i)
    i32.const 30
    local.set $i
    (loop $L
      ;; if (((0xdeadbeef >> i) & 1) == 0) { x2*=x1; x1*=x1; } else { x1*=x2; x2*=x2; }
      i32.const 0xDEADBEEF
      local.get $i
      i32.shr_u
      i32.const 1
      i32.and
      i32.eqz
      if
        local.get $x2
        local.get $x1
        i32.mul
        local.set $x2
        local.get $x1
        local.get $x1
        i32.mul
        local.set $x1
      else
        local.get $x1
        local.get $x2
        i32.mul
        local.set $x1
        local.get $x2
        local.get $x2
        i32.mul
        local.set $x2
      end

      ;; x1 %= 561; x2 %= 561;
      local.get $x1
      i32.const 561
      i32.rem_u
      local.set $x1
      local.get $x2
      i32.const 561
      i32.rem_u
      local.set $x2

      ;; i--
      local.get $i
      i32.const 1
      i32.sub
      local.tee $i
      i32.const 0
      i32.ge_s
      br_if $L
    )
    local.get $x1
  )
)
