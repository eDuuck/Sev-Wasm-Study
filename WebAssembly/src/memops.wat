(module
  (memory (export "memory") 1)

  ;; Record layout (starting at ptr):
  ;; byte 0: age (u8)
  ;; byte 1-2: height (u16)
  ;; byte 3-6: score (i32)
  ;; 7 bytes total (padded to 8 for alignment)

  (func $write_record (param $ptr i32) (param $age i32)
                      (param $height i32) (param $score i32)
    (i32.store8 (local.get $ptr) (local.get $age))
    (i32.store16 (i32.add (local.get $ptr) (i32.const 1)) (local.get $height))
    (i32.store (i32.add (local.get $ptr) (i32.const 3)) (local.get $score))
  )

  (func $read_record_sum (param $ptr i32) (result i32)
    (local $age i32)
    (local $height i32)
    (local $score i32)
    (local.set $age (i32.load8_u (local.get $ptr)))
    (local.set $height (i32.load16_u (i32.add (local.get $ptr) (i32.const 1))))
    (local.set $score (i32.load (i32.add (local.get $ptr) (i32.const 3))))
    (i32.add (local.get $score)
      (i32.add (local.get $age) (local.get $height)))
      return
  )

  (func $negate_and_reload (param $ptr i32)
    (local $negscore i32)
    (local.set $negscore
      (i32.sub (i32.const 0) (i32.load (i32.add (local.get $ptr) (i32.const 3))))
    )
    (i32.store16 (i32.add (local.get $ptr) (i32.const 3)) (local.get $negscore))
  )

  (func (export "main")
    (local $base i32)
    (local.set $base (i32.const 0))

    ;; Write 10 different records (8 bytes each)
    (call $write_record (i32.add (local.get $base) (i32.const 0))   (i32.const 21) (i32.const 170) (i32.const 1000))
    (call $write_record (i32.add (local.get $base) (i32.const 8))   (i32.const 25) (i32.const 175) (i32.const 1100))
    (call $write_record (i32.add (local.get $base) (i32.const 16))  (i32.const 30) (i32.const 180) (i32.const 1200))
    (call $write_record (i32.add (local.get $base) (i32.const 24))  (i32.const 35) (i32.const 185) (i32.const 1300))
    (call $write_record (i32.add (local.get $base) (i32.const 32))  (i32.const 40) (i32.const 190) (i32.const 1400))
    (call $write_record (i32.add (local.get $base) (i32.const 40))  (i32.const 45) (i32.const 195) (i32.const 1500))
    (call $write_record (i32.add (local.get $base) (i32.const 48))  (i32.const 50) (i32.const 200) (i32.const 1600))
    (call $write_record (i32.add (local.get $base) (i32.const 56))  (i32.const 55) (i32.const 205) (i32.const 1700))
    (call $write_record (i32.add (local.get $base) (i32.const 64))  (i32.const 60) (i32.const 210) (i32.const 1800))
    (call $write_record (i32.add (local.get $base) (i32.const 72))  (i32.const 65) (i32.const 215) (i32.const 1900))

    ;; Read and sum all records to simulate verification
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 0))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 8))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 16))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 24))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 32))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 40))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 48))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 56))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 64))))
    (drop (call $read_record_sum (i32.add (local.get $base) (i32.const 72))))

    ;; Apply negation on some records for signed load/store testing
    (call $negate_and_reload (i32.add (local.get $base) (i32.const 16)))
    (call $negate_and_reload (i32.add (local.get $base) (i32.const 40)))
    (call $negate_and_reload (i32.add (local.get $base) (i32.const 72)))

    ;; Verify negative writes via signed 16-bit load
    (drop (i32.load16_s (i32.add (local.get $base) (i32.const 19))))
    (drop (i32.load16_s (i32.add (local.get $base) (i32.const 43))))
    (drop (i32.load16_s (i32.add (local.get $base) (i32.const 75))))
  )
)
