(function (matrix-multiply op1 op2 ret)
  (block
    ; | 0 1 |  | 0 1 |
    ; | 2 3 |  | 2 3 |
    (array.set ret 0 (+
      (* (array.get op1 0) (array.get op2 0))
      (* (array.get op1 1) (array.get op2 2))
    ))
    (array.set ret 1 (+
      (* (array.get op1 0) (array.get op2 1))
      (* (array.get op1 1) (array.get op2 3))
    ))
    (array.set ret 2 (+
      (* (array.get op1 2) (array.get op2 0))
      (* (array.get op1 3) (array.get op2 2))
    ))
    (array.set ret 3 (+
      (* (array.get op1 2) (array.get op2 1))
      (* (array.get op1 3) (array.get op2 3))
    ))
  )
)

(function (init-matrix matrix)
  (block
    (array.set matrix 0 1)
    (array.set matrix 1 1)
    (array.set matrix 2 1)
    (array.set matrix 3 0)
  )
)

(function (copy source target)
  (block
    (array.set target 0 (array.get source 0))
    (array.set target 1 (array.get source 1))
    (array.set target 2 (array.get source 2))
    (array.set target 3 (array.get source 3))
  )
)

(function (fib n)
  (block
    (if (== n 0) (return 0))
    (set tmp1  (array.create 4))
    (set tmp2  (array.create 4))
    (set exp   (array.create 4))
    (set value (array.create 4))
    (init-matrix exp)
    (array.set value 0 1)
    (array.set value 1 0)
    (array.set value 2 0)
    (array.set value 3 1)
    (set number n)
    (for (set i 1) (<= i n) (set i (* i 2))
      (block
        (if (== (% number 2) 1)
          (block
            (matrix-multiply exp value tmp2)
            (copy tmp2 value)
          )
        )
        (matrix-multiply exp exp tmp1)
        (copy tmp1 exp)
        (set number (/ number 2))
      )
      
    )
    (return (array.get value 1))
  )
)

(function (main)
  (block
    (set n (scan))
    (print (fib n))
  )
)
