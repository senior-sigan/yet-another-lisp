(defun hanoi (from to via num)
  (unless (= num 0)
    (hanoi from via to (1- num))
    (format t "~a: ~a => ~a~&" num from via)
    (hanoi to from via (1- num))))