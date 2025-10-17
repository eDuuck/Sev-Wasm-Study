(module
  (memory (export "memory") 1)
  
    ;; is_prime(n) takes a (positive) number and returns 1 if this number is
    ;; prime, 0 if it's composite.
    (func $is_prime (param $n i32) (result i32)
        (local $i i32)
        
        ;; n < 2 are not prime
        (i32.lt_u (local.get $n) (i32.const 2))
        if
            i32.const 0
            return
        end

        ;; n == 2 is prime
        (i32.eq (local.get $n) (i32.const 2))
        if
            i32.const 1
            return
        end

        ;; Other even numbers are not prime
        (i32.eq (i32.rem_u (local.get $n) (i32.const 2)) (i32.const 0))
        if
            i32.const 0
            return
        end

        ;; Here we know that n > 2 and that n is odd. Run a loop trying to
        ;; divide it by all odd numbers smaller than it.
        ;;
        ;; for i = 3; i < n; i += 2
        (local.set $i (i32.const 3))
        (loop $testprime (block $breaktestprime
            (i32.ge_u (local.get $i) (local.get $n))
            br_if $breaktestprime

            ;; divisor found; return false
            (i32.eq (i32.rem_u (local.get $n) (local.get $i)) (i32.const 0))
            if
                i32.const 0
                return
            end

            (local.set $i (i32.add (local.get $i) (i32.const 2)))
            br $testprime
        ))

        ;; if we're here, the loop didn't find a divisor
        i32.const 1
    )

  ;; Main function to find first 100 primes
  (func $main (export "_start")
    (local $count i32)
    (local $current i32)
    (local $offset i32)
    
    ;; Initialize variables
    (local.set $count (i32.const 0))
    (local.set $current (i32.const 2))
    (local.set $offset (i32.const 0))
    
    (loop $find_primes
      ;; Check if current number is prime
      (if (call $is_prime (local.get $current))
        (then
          ;; Store prime in memory
          (i32.store (local.get $offset) (local.get $current))
          
          ;; Increment counters
          (local.set $offset (i32.add (local.get $offset) (i32.const 4)))
          (local.set $count (i32.add (local.get $count) (i32.const 1)))
        )
      )
      ;;(call $print_i32 (local.get $current))
      ;; Move to next number
      (local.set $current (i32.add (local.get $current) (i32.const 1)))
      
      ;; Continue until we have 100 primes
      (br_if $find_primes (i32.lt_u (local.get $count) (i32.const 100)))
    )
  )
)