# Lab4.2 实验报告

**姓名：**孙晨阳 **学号：**PB20081594

## 实验要求

根据实验文档提供的GVN算法介绍，和助教提供的代码框架，在`lightIR`中实现GVN数据流分析算法。

## 实验难点

### 关于instruction和value expression的理解

在伪代码中每个`value expression`表现为vi，每个表达式都是具体的x=a+b。但是在代码中`value expression`却是一个抽象的类，而表达式也变成了`Instruction`，那么对于`Instruction`而言，一个IR指令，变量x是什么，表达式a+b又是什么，又如何确定x=a+b的`value expression`？

经过反复地看代码lightIR那部分，明白了每个Instruction都是链式连接的，类似的，变量x具有两个指针，分别指向变量a，和变量b，同理a和b又有与x的相似结构，就类似下面简单代码展示的效果

```c++
struct Var{
	int op;
	struct Var* left_operand;
	struct Var* right_operand;
}
```

更简单的来讲`var = instruction`，只不过var没有存在的必要故用`instruction`表示

同理一个`instruction`的`value expression`也是链式连接

~~感觉这部分就是看的多了自然就理解，现在也感受不到最初的那种一脸懵逼的感觉了~~

### 等价类的实现

在伪代码中的等价类为{vi, x, y}，但是在代码中该怎么去写？index，leader，value_expr, value_phi, members是什么？其实除了leader其他几个也都是还能理解的，但是leader实在是不知道怎么写，也不知道是做什么的。

直到后来才发现leader的作用主要是在常量传播上面，如果不做常量传播似乎可有可无（？

同时index和value_expr也造成了极大的混淆，由于v1，v2的观念先入为主，导致我很长时间以为index才是表示vi的，但完成之后才发现，index才是最可有可无的（不过好像相同index的等价类在partition里面好像只能存在一个，因此推测没有index的话可能导致partition自己无法区分等价类，而导致不同等价类归并为一个）

### phi指令转化为copy statement

如何假设一个phi指令存在于前继块中而不是在当前块中呢？

这部分在伪代码中并未提到，后来才明白是遇到phi指令单独处理。transfer函数中不处理phi指令，对每一个块的指令处理结束后，处理它后继块的phi指令，在当前块中找到后继块中phi指令的来源指令所在的等价类，并插入。因此在join函数中，对于两个前驱都能够找到phi指令并取交集

## 实验设计

对于部分较为简单的函数便不再讲述

### detectEquivalences

1. 对pout_初始化为顶元
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230111210929141.png" alt="image-20230111210929141" style="zoom:75%;" />

2. 按照bb块列表进行遍历
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230111211023918.png" alt="image-20230111211023918" style="zoom:75%;" />

3. 根据当前遍历块的前驱块的个数进行不同的初始化，当为label_entry时处理为label_entry块添加函数参数和全局变量的等价类

   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230111211135069.png" alt="image-20230111211135069" style="zoom:75%;" />

4. 对bb块中的每个指令进行遍历，不需要处理的则跳过
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230111211421842.png" alt="image-20230111211421842" style="zoom:75%;" />

5. 处理当前块后继块中的phi指令，将后继块中的phi指令转换为当前块中copy statement并添加到对应的等价类中
   如果当前块仍为顶元，则说明这个块有两个前驱，且两个块前驱并未处理，则此时暂不处理此块的后继块中的phi指令
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230111211512472.png" alt="image-20230111211512472" style="zoom:67%;" />

### valueExpr

按照指令的操作数分类处理

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112103320639.png" alt="image-20230112103320639" style="zoom:67%;" />

#### PhiExpression

若phi指令的两个操作数均不是常数，则用valueExpr递归处理左右操作数

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112103523075.png" alt="image-20230112103523075" style="zoom:67%;" />

#### CallExpression

先判断是否是纯函数，如果是纯函数，则先将函数类型作为参数插入Expression中，继续读取参数，并将参数插入CallExpression中

如果不是纯函数，则返回一个只跟此instruction有关的CallExpression实例

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112104458672.png" alt="image-20230112104458672" style="zoom:67%;" />

#### GepExpression

针对地址运算指令getelementptr单独设置的一个表达式

处理方式与CallExpression大致相同，遍历指令的参数并插入GepExpression实例中

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112104902560.png" alt="image-20230112104902560" style="zoom:67%;" />

#### BinaryExpression or CmpExpression or FCmpExpression

这一部分由于设计到常量折叠因此可能看起来比较繁琐

但实际设计思路是对左右操作数先判断是否是常数，然后转换为ConstantExpression或在pin中查找其值表达式，最后如果左右操作数都是常数，则进行常量折叠，否则根据instruction的不同类型，返回不同的值表达式

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112105148810.png" alt="image-20230112105148810" style="zoom:67%;" />

#### SingleExpression

针对单操作数进行分析，如果获得的单操作数的值表达式是个ConstantExpression，则进行常量折叠，否则返回SingleExpression

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112105605977.png" alt="image-20230112105605977" style="zoom:67%;" />

#### VarExpression

在本次实验中并未要求对load指令的冗余去除，因此，对alloca和load指令返回一个只与instruction有关的值表达式

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112105811437.png" alt="image-20230112105811437" style="zoom:67%;" />

### transferFunction

由于伪代码与具体代码的不同，我删去了伪代码中的`if x is in a class Ci in POUTs delete x`，其余与伪代码大致相同

先处理`ve = valueExpr(e)`，再处理`vpf = valuePhiFunc(ve,PINs)`，最后处理`if ve or vpf is in a class Ci in POUTs`

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112110130850.png" alt="image-20230112110130850" style="zoom:67%;" />

### valuePhiFunc

1. 先判断是否是$\phi k(v_{i1},v_{j1})+\phi k(v_{i2},v_{j2})$的格式
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112111133078.png" alt="image-20230112111133078" style="zoom:67%;" />
2. 判断$v_{i1}$$v_{i2}$是否来自同一个BB块，$v_{j1}$$v_{j2}$是否来自同一个BB块
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112111304864.png" alt="image-20230112111304864" style="zoom:67%;" />
3. 获得$BB_{k1}$，组成$v_{i1}+v_{i2}$，先用getVN判断当前BB块中是否有此值表达式，不存在则在前继块中递归寻找
   <img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112111334711.png" alt="image-20230112111334711" style="zoom:67%;" />
4. vj的获得方式同vi
5. 最后返回`PhiExpression`

### intersect

包括join，intersect的其他部分基本与伪代码相同

仅在此处设置新的等价类的vk和vpf为同一个

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112111829017.png" alt="image-20230112111829017" style="zoom:67%;" />

### operator==

先比较`partitions`之间的等价类的大小，再比较其中的每个等价类是否相等

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112112331806.png" alt="image-20230112112331806" style="zoom:67%;" />

等价类之间比较其`members_`是否相同

`leader_`之间的比较是为了防止同一个等价类`leader_`被更换为常量而不能被识别出来

<img src="C:\Users\Wisgy\AppData\Roaming\Typora\typora-user-images\image-20230112112439692.png" alt="image-20230112112439692" style="zoom:67%;" />

### 优化前后的IR对比

#### 优化前

```llvm
define i32 @main() {
label_entry:
  %op0 = call i32 @input()
  %op1 = call i32 @input()
  %op2 = icmp sgt i32 %op0, %op1
  %op3 = zext i1 %op2 to i32
  %op4 = icmp ne i32 %op3, 0
  br i1 %op4, label %label5, label %label14
label5:                                                ; preds = %label_entry
  %op6 = add i32 33, 33
  %op7 = add i32 44, 44
  %op8 = add i32 %op6, %op7
  br label %label9
label9:                                                ; preds = %label5, %label14
  %op10 = phi i32 [ %op8, %label5 ], [ %op17, %label14 ]
  %op11 = phi i32 [ %op7, %label5 ], [ %op16, %label14 ]
  %op12 = phi i32 [ %op6, %label5 ], [ %op15, %label14 ]
  call void @output(i32 %op10)
  %op13 = add i32 %op12, %op11
  call void @output(i32 %op13)
  ret i32 0
label14:                                                ; preds = %label_entry
  %op15 = add i32 55, 55
  %op16 = add i32 66, 66
  %op17 = add i32 %op15, %op16
  br label %label9
}
```

#### 优化后

```
define i32 @main() {
label_entry:
  %op0 = call i32 @input()
  %op1 = call i32 @input()
  %op2 = icmp sgt i32 %op0, %op1
  %op3 = zext i1 %op2 to i32
  %op4 = icmp ne i32 %op3, 0
  br i1 %op4, label %label5, label %label14
label5:                                                ; preds = %label_entry
  br label %label9
label9:                                                ; preds = %label5, %label14
  %op10 = phi i32 [ 154, %label5 ], [ 242, %label14 ]
  call void @output(i32 %op10)
  call void @output(i32 %op10)
  ret i32 0
label14:                                                ; preds = %label_entry
  br label %label9
}
```

#### 简要分析

对bin.cminus优化前后进行了对比，可以发现`label5`和`label14`的代码几乎全部删除，这部分的优化关键在于优化前IR中的%op10和%op13等价，经过GVN分析之后用%op10替换了%op13，因此`label9`中的%op12和%op11成为了冗余代码而被删除，同时通过常量传播，用常量替换了%op10的来源，因此`label5`和`label14`中的add指令全部成为了冗余代码而被删除

### 思考题

1. 请简要分析你的算法复杂度
   假设一个程序有$n$个指令，则至多有n个等价类，设每个等价类有v个变量，设共有j个join块

   - join函数要做一个$partitions.size\times partitions.size$的`intersect`，每个`intersect`的时间复杂度为$O(v)$，join每次循环要执行j次，则join函数时间复杂度为$O(n^2vj)$
   - 由下述分析可以得出transferFunction的时间复杂度为$O(nj)$，每次循环共执行n次，则时间复杂度为$O(n^2j)$
     - valueExpr由于要遍历输入的`partitions`因此时间复杂度为$O(n)$
     - getVN的时间复杂度也为$O(n)$
     - valuePhiFunc由于要递归检查，且包含getVN，因此时间复杂度为$O(n)\times O(j)=O(nj)$

   综上最坏情况下，总共迭代n次，则算法时间复杂度为$O(n^3vj)$

3. `std::shared_ptr`如果存在环形引用，则无法正确释放内存，你的 Expression 类是否存在 circular reference?
   不存在，由于是静态单赋值形式，所以环形引用一般出现在phi指令上，但是我写的代码是成流水线形式的，即每次循环中，phi指令的操作数一定是上一次循环中的value_expr，因此不可能出现环形引用，而循环最开始的value_expr的如果是phi指令则其操作数是被临时置空的，因此引用到最初的循环便结束了

4. 尽管本次实验已经写了很多代码，但是在算法上和工程上仍然可以对 GVN 进行改进，请简述你的 GVN 实现可以改进的地方
   - GVN之所以还能够改进的地方主要是phi指令的处理还不够好，正常对于phi指令的处理应当是一个处于所有链节点都处于当前循环中的链表，但是我的偏向于流水线处理结果，虽然代码能全部通过线上测试案例，但是对于我自己给出的一个深度递归的vpf的案例是不能输出正确结果的。
     曾尝试使用更完美的phi指令连接，方法是在join函数中通过递归而不是用`Ci`和`Cj`的`value expression`,产生的新的PhiExpression。这样要付出的代价是需要自动终止可能产生的环形引用，更高的时间复杂度。同时这个还没有完全通过线上测试的案例，尚未找出原因
   - 在实际机器中最耗时的load指令并未进行优化，未优化的主要原因是无法确定同一个地址是否有新的数据填入。但是觉得是可以进行优化的，我提出一个猜想尝试，load的参数除了地址之外还要再多一个store的值表达式，这个store的是load前最近的操作同一地址的store指令，因为如果要去掉一个冗余的load指令，那么在这个load指令之前必然有一个操作同一地址的load指令，且中间没有操作此地址的store指令或操作此地址的store指令并不改变此地址存储的值。

## 实验总结

~~彻底明白了在一项工程中算法的伪代码和实际代码的差别是有多大~~

本次实验在最开始时只是对GVN算法伪代码的麻木转化，因此实验便极难进展下去了。

- 首先是缺少对`lightIR`更深刻的认识导致很难将GVN算法和`lightIR`这一具体场景联系起来。

- 然后便是对GVN算法的理解不够透彻，只是理解了GVN算法的肤浅的表层，并未抓住它的核心。不过现在明白了，其实GVN算法的核心就在于一个值表达式，不断迭代的过程只是将具有相同值表达式的变量归类，在此基础上靠着伪代码随便怎么写都行
- 最后便是对代码框架的不理解和被代码框架拘束了，在`伪代码->实际代码->符合代码框架的实际代码`这一过程中，自己有个实际代码的思路，但是在具体框架下便有些限死了思维的发挥~~也许这便是工程吧~~。但为了代码抽象化的层次结构，因此便需要尽量不脱离框架按照思路修改部分函数。现在也有些明白了，TODO只是提示，但并不是必须，只要符合抽象的层次结构，并在这一结构下发散思维。

在做实验时最大的bug是对算法实现理解上的偏差，其他的细节上的bug都可以花时间去解决掉，但是算法实现上一旦出了偏差，需要改动的便是整个的代码框架。如果存在算法详解的话，便尽量跟着算法走，任何自己随意的思维的发散和错误理解都可能导致一去不返（亲身体验，没按照算法走，添加了些额外内容，因此一直改的方向都是错的，导致最后bug越改越多）...

## 实验反馈（可选 不会评分）

- 个人感觉应当重点讲解instruction和valueExpr及等价类的转化问题，感觉一开始很难入门的原因主要就是不理解等价类为什么是这样构造的，这样构造有什么用(比如leader之后做了好久才明白)，也并不明白为什么x=e中x就是e
  - 感觉其实等价类可以更抽象化一点，比如用类构造，在刚开始时只提供`members_`，`value_expr`接口，这样可能会更方便入门，后续的接口可以等做着做着陆续接触(?
  - 可以给出一个具体的IR指令转化为expression的例子，比如`%op3 = add i32 %op1, %op2`
    x 就是`%op3`，e就是`add i32 %op1, %op2`，而`Instruction* instr`就是`%op3`，可以通过`instr->get_operand(0)`访问到`%op1`（不过这样举例可能又会造成过于简单以至学生缺少必要的思考？）
  然后通过这样一个`instr`和`pin`构建x的`ValueExpr`

- Expression类`operator==`的更多重载

  在实验时对Expression类及其子类的比较，大多数时候还是使用的是shared_ptr共享指针的比较，以为原先代码中给定的那个`operator==`的重载仅适用于双方都是`shared_ptr<Expression>`类型的，特别是两个等价类的`value_phi`进行比较时，则不会重载到代码中的`operator==`，相反会直接比较双方的地址，几乎必定返回0。

  而以我们现阶段的C++水平很难明白这一点，大部分时候都会出错却不知道错在哪里，这种debug类型对于整个实验的收益不大，因此觉得可以补充一下。我当时也是花了好久才找到这个问题。

  与这个问题相同的还有`shared_ptr<CongruenceClass>`之间的比较，因为这个也没有重载，重载的是一个等价类实例之间的比较