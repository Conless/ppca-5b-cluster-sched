# PPCA Language

## TL;DR

这个程序展示了 PPCA Language 的语法及语义. 这个程序会求出所有输入数据的平方和.

```lisp
; 注释以分号开头
; 程序入口点为 main 函数
(function (main)
  ; 多条语句需要用代码块包裹起来
  (block
    ; 读入一个整数 n
    ; scan 是一个内置函数
    (set n (scan))
    ; 读入 n 个整数, 存入一个数组
    ; array.scan 也是一个内置函数
    (set xs (array.scan n))
    ; 输出答案
    (print (sum^2 xs n))))

; sum^2 是一个合法的变量名/函数名
(function (sum^2 array length)
  (block
    ; if 语句
    ; == 和 != 是内置函数 (下面的 < + * 都是内置函数)
    (if (== length 0)
      (return 0))
    (if (!= length 0)
      ; 变量的作用域为整个函数;
      ; 即使退出了 if 语句, 在后面 for 中也能使用 sum 变量
      (set sum 0))
    ; for 语句
    (for
      (set i 0) ; i = 0
      (< i length) ; i < length
      (set i (+ i 1)) ; ++i
      (block
        ; value = array[i]
        (set value (array.get array i))
        ; sum = sum + value * value
        (set sum (+ sum (* value value)))))
    ; 返回平方和
    (return sum)))
```

相当于:

```cpp
int main () {
  int n;
  std::cin >> n;
  int *xs = new int[n];
  for (int i = 0; i < n; ++i) std::cin >> xs[i];
  std::cout << sum2(xs, n) << std::endl;
}

int sum2 (int *array, int length) {
  int sum, i, value;

  if (length == 0) return 0;
  if (length != 0) sum = 0;

  for (i = 0; i < length; ++i) {
    value = array[i];
    sum = sum + value * value;
  }

  return sum;
}
```

## 概述

PPCA Language 是一种极简但可用的命令式 (imperative) 编程语言. 这个语言的语法采用类似于 [Lisp](https://en.wikipedia.org/wiki/Lisp_(programming_language)) 的 [S-expression](https://en.wikipedia.org/wiki/S-expression) 形式, 以方便解析与处理. 语法对空白符不敏感, 空格及换行符的数量不影响程序的语义.

PPCA Language 中具有两种值 (value): 数字 (范围及运算同 C++ 中的 `int` 类型) 和定长 `int` 数组 (0-based). 类型系统为动态类型, 源代码中不指定变量的类型.

一个程序 (program) 由若干个函数 (function) 构成, 程序的入口点为 `main` 函数. 函数具有返回值. 若函数执行完成时没有指定返回值, 则返回值为数字 0.

一个函数中包含一条语句 (statement), 语句中可能包含表达式 (expression). 表达式在执行时会具有一个值.

你可以用 `;` 开始一个行注释, 类似于 C/C++ 中的 `//`.

## 语法约定

下文中, 斜体部分表示一个占位符, 例如:

<pre><code>(set <em>variable-name</em> <em>value</em>)</code></pre>

表示这些语句是合法的语句:

```lisp
(set foo 1)
(set xyzzy foo)
```

占位符后面的下标表示占位符的类型.

- id 表示一个变量名或函数名 (identifier, 标识符);
- stmt 表示一个语句 (statement);
- expr 表示一个表达式 (expression).

变量名或函数名可以包含除小括号 `()`、分号 `;` 外的所有可见 ASCII 字符, 但不能以数字开头.

占位符后面的 ... 表示这里可以填写零个、一个或者多个参数, 以空格分隔.

## 表达式 (Expression)

### 整数常量 (IntegerLiteral)

十进制的正整数是整数常量. 可以带有前导 0, 但**不能是负数**. 如果想表示负数, 需要用 0 - x 的形式.

### 变量 (Variable)

直接写出一个变量名即可.

### 函数调用 (CallExpression)

语法如下:

<pre><code>(<em>function-name</em><sub>id</sub> <em>function-arguments</em><sub>expr</sub>...)</code></pre>

即使函数没有参数, 也需要在函数名两侧加上括号.

例如:

```lisp
(print 42)                    ; 打印 42
(print (* 6 7))               ; 打印 42
(print (* (+ 2 4) (+ 3 4)))   ; 打印 42
(print foobar)                ; 打印变量 foobar 的值
(print (+ foobar 42))         ; 打印变量 foobar 的值加上 42
(array.set a 0 42)            ; 将 a 变量所表示的数组第 0 项设为 42
(array.set a 0 (scan))        ; 读入一个数到 a 变量所表示的数组第 0 项
```

每行均为合法的函数调用.

#### 内置函数

内置函数如下: (传入错误类型参数时, 会直接异常终止程序运行)

- `int + (int x, int y)`: 返回 C++ 中 `x + y` 的值.
- `int - (int x, int y)`: 返回 C++ 中 `x - y` 的值.
- `int * (int x, int y)`: 返回 C++ 中 `x * y` 的值.
- `int / (int x, int y)`: 若 `y` 不为 0, 则返回 C++ 中 `x / y` 的值, 否则异常终止运行.
- `int % (int x, int y)`: 若 `y` 不为 0, 则返回 C++ 中 `x % y` 的值, 否则异常终止运行.
- `int < (int x, int y)`: 返回 C++ 中 `x < y` 的值.
- `int > (int x, int y)`: 返回 C++ 中 `x > y` 的值.
- `int <= (int x, int y)`: 返回 C++ 中 `x <= y` 的值.
- `int >= (int x, int y)`: 返回 C++ 中 `x >= y` 的值.
- `int == (int x, int y)`: 返回 C++ 中 `x == y` 的值.
- `int != (int x, int y)`: 返回 C++ 中 `x != y` 的值.
- `int || (int x, int y)`: 返回 C++ 中 `x || y` 的值. 需注意没有短路求值.
- `int && (int x, int y)`: 返回 C++ 中 `x && y` 的值. 需注意没有短路求值.
- `int ! (int x)`: 返回 C++ 中 `!x` 的值.
- `int scan ()`: 读入一个整数.
- `int print (int x)`: 打印整数 `x` 和 `\n` 并返回 0.
- `int[] array.create (int length)`: 创建一个长度为 `length` 的数组, 初始化为 0, 并返回.
- `int[] array.scan (int length)`: 创建一个长度为 `length` 的数组, 读入 `length` 个数到数组中, 并返回这个数组. 类似于 `for (int i = 0; i < length; ++i) std::cin >> array[i];`.
- `int array.print (int[] array)`: 依次打印 `array` 数组中的所有数 (打印完每个数之后有 `\n`), 并返回 0.
- `int array.get (int[] array, int index)`: 返回 `array` 数组的第 `index` 项. 数组越界则终止程序.
- `int array.set (int[] array, int index, int value)`: 将 `array` 数组的第 `index` 项设为 `value`. 数组越界则终止程序.

## 语句 (Statement)

表达式可以单独成为一个语句 (ExpressionStatement).

### 赋值语句 (SetStatement)

<pre><code>(set <em>variable-name</em><sub>id</sub> <em>value</em><sub>expr</sub>)</code></pre>

赋值语句的作用域为函数, 无需提前声明变量. 试图使用一个没有赋值过的变量会导致程序直接异常终止. 例如:

```lisp
(function (main)
  (block
    (set x 1)
    (if 1
      (block
        (set x 2)))
    (print x)))
```

会打印 2 而不是 1.

没有函数指针的概念; 不能把一个函数当成值赋给一个变量, 变量名与函数名也不能重名.

### 条件执行 (IfStatement)

<pre><code>(if <em>condition</em><sub>expr</sub>
  <em>then-body</em><sub>stmt</sub>)</code></pre>

如果 `condition` 所存储的整数不为 0, 则执行 `body`. 如果 `condition` 存储的不是一个整数, 则异常终止程序.

### 循环 (ForStatement)

<pre><code>(for
  <em>init</em><sub>stmt</sub>
  <em>test</em><sub>expr</sub>
  <em>update</em><sub>stmt</sub>
  <em>body</em><sub>stmt</sub>)</code></pre>

语义同 C++ 中的

```C++
for (init; test; update) body;
```

若 `test` 不为整数, 则异常终止程序.

### 代码块 (BlockStatement)

<pre><code>(block
  <em>block-body</em><sub>stmt</sub>...)</code></pre>

依次执行 `block-body` 中的多条语句.

### 返回语句 (ReturnStatement)

<pre><code>(return <em>return-value</em><sub>expr</sub>)</code></pre>

退出函数.

## 函数 (FunctionDeclaration)

<pre><code>(function (<em>function-name</em><sub>id</sub> <em>function-parameters</em><sub>id</sub>...)
  <em>function-body</em><sub>stmt</sub>)</code></pre>

函数的定义顺序对语义没有影响; 前面定义的函数也可以调用后面定义的函数.

例如:

```lisp
(function (add x y)
  (return (+ x y)))
```

相当于:

```c
int add (int x) {
  return x + y;
}
```

例如:

```lisp
(function (abs x)
  (block
    (if (> x 0)
      (return x))
    (return (- 0 x))))
```

相当于:

```c
int abs (int x) {
  if (x > 0) return x;
  return 0 - x;
}
```
