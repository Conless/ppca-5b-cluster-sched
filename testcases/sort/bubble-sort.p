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
            (set _ (array.set a j (array.get a (+ j 1))))
            (set _ (array.set a (+ j 1) t))
          )
        )
      )
    )
  )
)

(function (main)
  (block
    ; length of array
    (set n (scan))
    (set a (array.create n))
    ; read array contents
    (for
      (set i 0)
      (< i n)
      (set i (+ i 1))
      (set _ (array.set a i (scan)))
    )
    ; sort the array
    (set _ (bubble-sort a n))
    ; print the sorted array
    (for
      (set i 0)
      (< i n)
      (set i (+ i 1))
      (set _ (print (array.get a i)))
    )
    (return 0)
  )
)
