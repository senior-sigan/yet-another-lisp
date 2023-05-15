(defun naive-fib (i)
  (if (< i 2) 1
      (+ (naive-fib (- i 1))
         (naive-fib (- i 2)))))

(deftest fib ()
  (should be = 1 (naive-fib 0))
  (should be = 8 (naive-fib 5))
  (should be = 144 (naive-fib 11)))