# lab2 实验报告
学号:PB20081594 姓名:孙晨阳

## 问题1: getelementptr
请给出 `IR.md` 中提到的两种 getelementptr 用法的区别,并稍加解释:
  - `%2 = getelementptr [10 x i32], [10 x i32]* %1, i32 0, i32 %0`
    将`%1` 视为指向大小为10的一维数组的指针，因此`%1` 就类似`a[m][n]` 中的a，存储的是二维数组(即一维数组)的地址。在获取形如`a[0][i]` 的数组元素的地址时，首先需要确定指向一维数组的偏移值，即先需要确定指向的是第一个一维数组还是指向的第二个一维数组；然后确定好一维数组之后，再利用第二个索引值`%0` 获取一维数组中`%0` 位置的元素地址。
  - `%2 = getelementptr i32, i32* %1 i32 %0`
    将`%1` 视为指向`i32` 类型数值的指针,  因此`%1` 就类似一维数组`a[n]` 的a,  存储的是数组的地址,  指向的是数组元素。因此只需要一个索引值`%0` 以确定指向的是数组中的哪个元素, 再计算出其地址即可

## 问题2: cpp 与 .ll 的对应
请说明你的 cpp 代码片段和 .ll 的每个 BasicBlock 的对应关系。
### assign_generator
#### main函数的第一个块
```C++
builder->set_insert_point(bb);
auto aAlloca = builder->create_alloca(ArrayType::get(Int32Type, 10));
auto a0GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(0)});
builder->create_store(CONST_INT(10), a0GEP);
a0GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(0)});
auto a0Load = builder->create_load(a0GEP);
auto mul = builder->create_imul(a0Load, CONST_INT(2));
auto a1GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(1)});
builder->create_store(mul, a1GEP);
a1GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(1)});
auto a1Load = builder->create_load(a1GEP);
builder->create_ret(a1Load);//引用ret返回,第一个块到此结束
```
以上的CPP代码片段对应以下.ll的BasicBlock
``` llvm
0:;BB入口
  %1 = alloca i32, align 4
  %2 = alloca [10 x i32], align 16
  store i32 0, i32* %1, align 4
  %3 = getelementptr inbounds [10 x i32], [10 x i32]* %2, i64 0, i64 0
  store i32 10, i32* %3, align 16
  %4 = getelementptr inbounds [10 x i32], [10 x i32]* %2, i64 0, i64 0
  %5 = load i32, i32* %4, align 16
  %6 = mul nsw i32 %5, 2
  %7 = getelementptr inbounds [10 x i32], [10 x i32]* %2, i64 0, i64 1
  store i32 %6, i32* %7, align 4
  %8 = getelementptr inbounds [10 x i32], [10 x i32]* %2, i64 0, i64 1
  %9 = load i32, i32* %8, align 4
  ret i32 %9;BB出口
```
### fun_generator
#### callee函数的第一个块
```C++
builder->set_insert_point(bb);
auto retAlloca = builder->create_alloca(Int32Type);
auto aAlloca = builder->create_alloca(Int32Type);
std::vector<Value *> args;  
for (auto arg = calleeFun->arg_begin(); arg != calleeFun->arg_end(); arg++) {args.push_back(*arg);}
builder->create_store(args[0], aAlloca);
auto aLoad = builder->create_load(aAlloca);
auto mul = builder->create_imul(CONST_INT(2), aLoad);
builder->create_store(mul, retAlloca);
auto retLoad = builder->create_load(retAlloca);
builder->create_ret(retLoad);//引用ret返回,第一个块到此结束
```
以上的CPP代码片段对应以下.ll的BasicBlock
``` llvm
1:;BB入口
  %2 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = mul nsw i32 2, %3
  ret i32 %4;BB出口
```
#### main函数的第一个块
```C++
builder->set_insert_point(bb);
retAlloca = builder->create_alloca(Int32Type);
auto call = builder->create_call(calleeFun, {CONST_INT(110)});
builder->create_store(call, retAlloca);
builder->create_ret(call);//引用ret返回,第一个块到此结束
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
0:;BB出口
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call i32 @callee(i32 110)
  ret i32 %2;BB入口
```
### if_generator
#### main函数的第一个块
```C++
builder->set_insert_point(bb);
auto retAlloca = builder->create_alloca(Int32Type);
auto aAlloca = builder->create_alloca(floatType);
builder->create_store(CONST_FP(5.555), aAlloca);
auto truebb = BasicBlock::create(module, "true", mainFun);
auto falsebb = BasicBlock::create(module, "false", mainFun);
auto retbb = BasicBlock::create(module, "ret", mainFun);
auto aLoad = builder->create_load(aAlloca);
auto comp = builder->create_fcmp_gt(aLoad, CONST_FP(1));
auto br = builder->create_cond_br(comp, truebb, falsebb);//br跳转,此块结束
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
0:
  %1 = alloca i32, align 4
  %2 = alloca float, align 4
  store i32 0, i32* %1, align 4
  store float 0x40163851E0000000, float* %2, align 4
  %3 = load float, float* %2, align 4
  %4 = fcmp ogt float %3, 1.000000e+00
  br i1 %4, label %5, label %6;根据条件%4选择跳转至块%5或块%6
```
#### true条件块
```C++
builder->set_insert_point(truebb);
builder->create_store(CONST_INT(233), retAlloca);
builder->create_br(retbb);//true执行完毕,跳转至ret块
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
5:                                                ; preds = %0
  store i32 233, i32* %1, align 4
  br label %7;跳转至块%7
```
#### false条件块
```C++
builder->set_insert_point(falsebb);
builder->create_store(CONST_INT(0), retAlloca);
builder->create_br(retbb);//false执行完毕,跳转至ret块
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
6:                                                ; preds = %0
  store i32 0, i32* %1, align 4
  br label %7;跳转至块%7
```
#### 返回块
```C++
builder->set_insert_point(retbb);
auto retLoad = builder->create_load(retAlloca);
builder->create_ret(retLoad);//返回值加载完毕,返回数值,块结束
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
7:                                                ; preds = %6, %5
  %8 = load i32, i32* %1, align 4
  ret i32 %8;返回对应值,块结束
```
### while_generator
#### main函数的第一个块
```C++
builder->set_insert_point(bb);
auto aAlloca = builder->create_alloca(Int32Type);
auto iAlloca = builder->create_alloca(Int32Type);
builder->create_store(CONST_INT(10), aAlloca);
builder->create_store(CONST_INT(0),iAlloca);
auto cmpBB = BasicBlock::create(module, "cmp", mainFun);
auto whileBB = BasicBlock::create(module, "while", mainFun);
auto retBB = BasicBlock::create(module, "ret", mainFun);
builder->create_br(cmpBB);//跳转至判断块
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
0:
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  store i32 0, i32* %3, align 4
  br label %4;跳转至块%4
```
#### 判断块
```C++
builder->set_insert_point(cmpBB);
auto iLoad = builder->create_load(iAlloca);
auto cmp = builder->create_icmp_lt(iLoad, CONST_INT(10));
auto br = builder->create_cond_br(cmp, whileBB, retBB);//根据cmp,选择跳转至循环块或是返回块
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
4:                                                ; preds = %7, %0
  %5 = load i32, i32* %3, align 4
  %6 = icmp slt i32 %5, 10
  br i1 %6, label %7, label %13;选择跳转至块%7或块%13
```
#### 循环块
```C++
builder->set_insert_point(whileBB);
iLoad = builder->create_load(iAlloca);
auto add = builder->create_iadd(iLoad, CONST_INT(1));
builder->create_store(add, iAlloca);
iLoad = builder->create_load(iAlloca);
auto aLoad = builder->create_load(aAlloca);
add = builder->create_iadd(aLoad, iLoad);
builder->create_store(add, aAlloca);
builder->create_br(cmpBB);//跳转至判断块
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
7:                                                ; preds = %4
  %8 = load i32, i32* %3, align 4
  %9 = add nsw i32 %8, 1
  store i32 %9, i32* %3, align 4
  %10 = load i32, i32* %2, align 4
  %11 = load i32, i32* %3, align 4
  %12 = add nsw i32 %10, %11
  store i32 %12, i32* %2, align 4
  br label %4;跳转至块%4
```
#### 返回块
```C++
builder->set_insert_point(retBB);
auto retLoad = builder->create_load(aAlloca);
builder->create_ret(retLoad);//返回值,块结束
```
以上的CPP代码片段对应以下.ll的BasicBlock
```llvm
13:                                               ; preds = %4
  %14 = load i32, i32* %2, align 4
  ret i32 %14
```
### 针对gcd_array_generator.cpp中的问题在此解答
- `auto x0GEP = builder->create_gep(x, {CONST_INT(0), CONST_INT(0)});  // GEP: 这里为什么是{0, 0}呢? (实验报告相关)`
- `auto y0GEP = builder->create_gep(y, {CONST_INT(0), CONST_INT(0)});  // GEP: 这里为什么是{0, 0}呢? (实验报告相关)`
  传进去的x,y为指向一维数组的指针, 因此需要两个索引(原因详见[问题1](#问题1: getelementptr)), 第一个索引`0` 确定在当前指向的一维数组中, 第二个索引`0` 则在先前确认的一维数组中索引至指向数组中的第一个元素
- `call = builder->create_call(funArrayFun, {x0GEP, y0GEP});           // 为什么这里传的是{x0GEP, y0GEP}呢?`
  函数调用的参数是以vector传递的, 在代码中传递的是两个一维数组首元素的地址。因此传递的是x0的地址和y0的地址
## 问题3: Visitor Pattern
分析 `calc` 程序在输入为 `4 * (8 + 4 - 1) / 2` 时的行为：
1. 请画出该表达式对应的抽象语法树（使用 `calc_ast.hpp` 中的 `CalcAST*` 类型和在该类型中存储的值来表示），并给节点使用数字编号。
   以下为该表达式对应的抽象语法树, 每个节点内容为`数字编号:终结符`
   ![AST|200](AST.png)
2. 请指出示例代码在用访问者模式遍历该语法树时的遍历顺序。

序列请按如下格式指明（序号为问题 2.1 中的编号）：  
3->2->5->1

## 实验难点
描述在实验中遇到的问题、分析和解决方案。

## 实验反馈
吐槽?建议?
