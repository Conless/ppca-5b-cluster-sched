(function (fib n)
  (block
    (if (== n 0) (return 0))
    (if (== n 1) (return 1))
    (set fib1 (fib (- n 1)))
    (set fib2 (fib (- n 2)))
    (return (+ fib1 fib2))
  )
)

(function (main)
  (block
    (set n (scan))
    (print (fib n))
  )
)
