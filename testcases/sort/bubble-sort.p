(function (bubble-sort a n)
  (block
    (for
      (set i 0)
      (< i (- n 1))
      (set i (+ i 1))
      (for
        (set j 0)
        (< j (- (- n i) 1))
        (set j (+ j 1))
        (if (>= (array.get a j) (array.get a (+ j 1)))
          (block
            (set t (array.get a j))
            (array.set a j (array.get a (+ j 1)))
            (array.set a (+ j 1) t)
          )
        )
      )
    )
  )
)

(function (main)
  (block
    (set n (scan))
    (set a (array.scan n))
    (bubble-sort a n)
    (array.print a)
  )
)
