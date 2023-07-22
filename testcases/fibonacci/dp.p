(function (max a b)
  (block
    (if (>= a b) (return a))
    (return b)
  )
)

(function (fib n)
  (block
    (set buffer (array.create (max (+ n 1) 3)))
    ; set the initial numbers
    (array.set buffer 0 0)
    (array.set buffer 1 1)
    (for (set i 2) (<= i n) (set i (+ i 1))
      (block
        (set fib1 (array.get buffer (- i 1)))
        (set fib2 (array.get buffer (- i 2)))
        (array.set buffer i (+ fib1 fib2))
      )
    )
    (return (array.get buffer n))
  )
)

(function (main)
  (block
    (set n (scan))
    (print (fib n))
  )
)
