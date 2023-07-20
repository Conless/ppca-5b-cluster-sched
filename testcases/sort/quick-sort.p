(function (partition a p r)
  (block
    (set pivot (array.get a r))
    (set i (- p 1))
    (for
      (set j p)
      (< j r)
      (set j (+ j 1))
      (if
        (<= (array.get a j) pivot)
        (block
          (set i (+ i 1))
          (set t (array.get a i))
          (array.set a i (array.get a j))
          (array.set a j t)
        )
      )
    )
    (set t (array.get a (+ i 1)))
    (array.set a (+ i 1) (array.get a r))
    (array.set a r t)
    (return (+ i 1))
  )
)

(function (quick-sort a p r)
  (block
    (if (>= p r) (return 0))
    (set q (partition a p r))
    (quick-sort a p (- q 1))
    (quick-sort a (+ q 1) r)
  )
)

(function (main)
  (block
    (set n (scan))
    (set a (array.scan n))
    (quick-sort a 0 (- n 1))
    (array.print a)
  )
)
