(defun fizzbuzz ()
   (fizzbuzz-aux 1))

(defun fizzbuzz-aux (N)     
  (let ((mult3 (rem N 3))
        (mult5 (rem N 5)))
    (cond     
       ((AND (zerop mult3) (zerop mult5))
       (print 'fizzBuzz))   
    ((zerop mult3)
   		 (print 'Fizz))      
    ((zerop mult5)
   	   (print 'Buzz))      	
    (T (print N)))

  (if (< N 100)     	  
    (fizzbuzz-aux (+ N 1))
    ())))