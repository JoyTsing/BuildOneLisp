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
;创建局部作用域
(fun {let b} {
    ((\ {_} b) ())
})
;逻辑运算
(fun {not x}   {- 1 x})
(fun {or x y}  {+ x y})
(fun {and x y} {* x y})

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
