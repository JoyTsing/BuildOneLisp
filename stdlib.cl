;类型
(def {nil} {})
(def {true} 1)
(def {false} 0)
;简化函数定义
;之后可以形如这样来定义 fun {add x y} {+ x y}
(def {fun} (\ {f b} {
    def (head f)
        (\ (tail f) b)
}))
;list解包和打包 直接使用时pack时记得(pack f)
(fun {unpack f l}{
    eval (join (list f) l)
})
(fun {pack f & xs} {f xs})
(def {curry} unpack)
(def {uncurry} pack)
;按顺序执行
(fun {do & l} {
    if (== l nil)
        {nil}
        {last l}
})
;创建持续更广的局部作用域
(fun {let b} {
    ((\ {_} b) ())
})
;逻辑运算
(fun {not x}   {- 1 x})
(fun {or x y}  {+ x y})
(fun {and x y} {* x y})

;数字运算
(fun {min & xs} {
    if (== (tail xs) nil) {fst xs}
        {do
            (= {rest} (unpack min(tail xs)))
            (= {item} (fst xs))
            (if (< item rest) {item} {rest})
        }
})

(fun {max & xs} {
    if(== (tail xs) nil) {fst xs}
        {do
            (= {rest} (unpack min (tail xs)))
            (= {item} (fst xs))
            (if (< item rest) {item} {rest})
        }
})

;flip使得先处理b再处理a
(fun {flip f a b} {f b a})
;接受任意参数并作为表达式来计算
(fun {ghost & xs} {eval xs})
;用于组合两个函数 先把参数传到g 再传到f中计算 类似管道,第一个参数是给g的
(fun {comp f g x} {f (g x)})
;取列表元素
(fun {fst l} { eval (head l)  })
(fun {snd l} { eval (head (tail l))  })
(fun {trd l} { eval (head (tail (tail l)))  })
;列表长度
(fun {len l} {
   if (== l nil)
    {0}
    {+ 1 (len (tail l))}
})
;取第n个元素
(fun {nth n l} {
   if (== n 0)
    {fst l}
    {nth (- n 1) (tail l)}
})
;列表中的最后一个元素
(fun {last l} {nth (- (len l) 1) l})
;取前n个列表的元素
(fun {take n l} {
   if (== n 0)
    {nil}
    {join (head l) (take (- n 1) (tail l))}
})
;删除前n个列表的元素
(fun {drop n l} {
   if (== n 0)
    {l}
    {drop (- n 1) (tail l)}
})
;分裂第n个元素
(fun {split n l} {list (take n l) (drop n l)})
;判断是否在列表中
(fun {elem x l} {
   if (== l nil)
    {false}
    {if (== x (fst l))
        {true}
        {elem x (tail l)}
    }
})

(fun {take-while f l} {
   if (not (unpack f (head l)))
    {nil}
    {join (head l) (take-while f (tail l))}
})

(fun {drop-while f l} {
   if (not (unpack f (head l)))
    {l}
    {drop-while f (tail l)}
})
;map映射列表中的元素
(fun {map f l} {
   if (== l nil)
    {nil}
    {join (list (f (fst l))) (map f (tail l))}
})
;filter过滤掉一些不满足条件的
;filter (\ {x} {> x 2}) {5 2 11 -7 8 1}
(fun {filter f l} {
    if (== l nil)
     {nil}
     {join (if (f (fst l))
                {head l}
                {nil})
    (filter f (tail l))}
})

;累计作用于第一个数（折叠）
(fun {foldl f zl}{
    if (== l nil)
        {z}
        {fold l (f z (fst l)) (tail l)}
})

(fun {sum l} {foldl + 0 l})
(fun {product l} {foldl * 1 l})

(fun {select &cs} {
    if (== cs nil)
      {error "No Selection Found"}
      {if (fst (fst cs))
        {snd (fst cs)}
        {unpack select (tail cs)}
      }
})

(def {otherwise} true)

;select使用的例子
(fun {month-day-suffix i}{
    select
        {(== i 0) "st"}
        {(== i 1) "nd"}
        {(== i 3) "rd"}
        {otherwise "th"}
})

(fun {case x & cs} {
    if (== cs nil)
        {error "No Case Found"}
        {if (== x (fst (fst cs)))
            {snd (fst cs)}
            {unpack case (join (list x) (tail cs))}
        }
})

(fun {day-name x} {
   case x
        {0 "Monday"}
        {1 "Tuesday"}
        {2 "Wednesday"}
        {3 "Thursday"}
        {4 "Friday"}
        {5 "Saturday"}
        {6 "Sunday"}
})

(fun {fib n} {
    select
        { (== n 0) 0}
        { (== n 1) 1}
        { otherwise (+ (fib (- n 1)) (fib (- n2)))}
})

; 返回list中的一个pair
(fun {lookup x l} {
   if (== l nil)
       {error "No Element Found"}
        {do
            (= {key} (fst (fst l)))
            (= {val} (snd (fst l)))
            (if (== key x) {val} {lookup x (tail l)})
        }
})

;把两个list合并成一个pair的list
(fun {zip x y} {
   if (or (== x nil) (== y nil))
    {nil}
    {join (list (join (head x) (head y))) (zip (tail x) (tail y))}
})

(fun {unzip l} {
   if (== l nil)
    {{nil nil}}
    {do
        (= {x} (fst l))
        (= {xs} (unzip (tail l)))
        (list (join (head x) (fst xs)) (join (tail x) (snd xs)))
    }
})
