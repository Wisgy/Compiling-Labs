# Lab5 实验文档

- [Lab5 实验文档](#lab5-实验文档)
  - [0. 前言](#0-前言)
    - [主要工作](#主要工作)
  - [1. 实验框架](#1-实验框架)
  - [2. 实验内容](#2-实验内容)
    - [阅读龙芯的文档](#阅读龙芯的文档)
    - [完成代码](#完成代码)
  - [3. 运行与调试](#3-运行与调试)
    - [龙芯服务器的登录](#龙芯服务器的登录)
    - [汇编代码的生成](#汇编代码的生成)
    - [调试生成的汇编](#调试生成的汇编)
    - [测试](#测试)
  - [4. 提交](#4-提交)
    - [目录结构](#目录结构)
    - [提交要求、评分标准](#提交要求评分标准)
  - [附录：栈分配策略](#附录栈分配策略)
    - [示例 1：计算加法并输出](#示例-1计算加法并输出)
    - [示例 2：调用函数访问数组](#示例-2调用函数访问数组)


## 0. 前言

经过前四个Lab，我们的cminus compiler已经可以生成有效的llvm IR。Lab5实验目的，需要你根据 IR 生成龙芯LA64架构下的可正常运行的汇编代码。

### 主要工作

1. 阅读龙芯LA64的ABI，熟悉了解龙芯loongArch64（简称LA64）架构。
2. 完成 `codegen`，由 IR 生成汇编代码。
3. 进行寄存器分配，优化性能（选做）。
4. 工作总结及功能汇报展示。


## 1. 实验框架

需要在现有框架的基础上，添加 `codegen` 模块，实现 IR 到汇编的转换；如果要优化性能，可以添加寄存器分配的模块。

## 2. 实验内容

### 阅读龙芯的文档

+ [龙芯官方文档](https://github.com/loongson/LoongArch-Documentation/releases/latest/download/LoongArch-Vol1-v1.02-CN.pdf)
+ [龙芯 ABI 文档](龙芯扩展实验ABI/龙芯扩展实验ABI.md)

可以思考以下问题：

1. 当函数参数个数大于 8 个时，如何传递参数？
2. 浮点常量、全局变量是如何存储的？

> 可以写一段 C 语言代码，将该文件命名为 `test.c`，在龙芯上用 `gcc -include io.h -S test.c` 生成汇编代码，观察汇编代码，思考如何实现。


### 完成代码

可以先使用[栈分配策略](#附录栈分配策略)存储变量，优先追求代码生成的完成度。
也可以在完成基本的代码生成后，使用专门的寄存器分配算法，优化性能。

> 寄存器分配部分可以参考 [寄存器分配.md](寄存器分配.md)

评测、答辩将会考虑以下两个方面：

1. 代码生成的完成度（允许自由选择和组合）：

   +  单个 main 函数的代码的汇编代码生成，主要包括算术指令、调用 `output` 函数，正常返回。
   +  单个 main 函数的代码的汇编代码生成，包括比较和跳转指令、局部数组访问。
   +  多个函数的代码的汇编代码生成，包括函数调用、返回。
   +  支持浮点数、全局变量等高级类型

   > 允许自由拓展

2. 是否完成寄存器分配：

   + 不进行专门的寄存器分配算法（图着色、线性分配等），所有变量都存储在栈上（详见[附录](#附录栈分配策略)）。
   + 完成专门的寄存器分配算法



## 3. 运行与调试

### 龙芯服务器的登录

根据助教提供的账号，密码，采用 ssh 登录龙芯机器。


### 汇编代码的生成

编译过程：

```bash
rm -rf build
mkdir build
cd build
cmake ..
make
```

编译后会产生 `cminusfc` 可执行文件，使用 `cminusfc -S test.cminus` 编译 `test.cminus`源文件，生成 `test.s`汇编文件。

> 可以选择开启 `cminusfc` 的 `-mem2reg` 选项优化中间代码

学生可以先本地运行生成汇编代码，然后将 `test.s` `io.c` `io.h` 传输到龙芯服务器，再在龙芯服务器上运行:

> 假设 test.c 在仓库根目录下，用户的用户名为 username

```bash
build/cminusfc -S test.cminus

scp src/io/io.c src/io/io.h test.s username@202.38.75.246:~/
ssh username@202.38.75.246

# 以下命令在 ssh 会话中执行
gcc test.s io.c -o test
./test
echo $?
```

### 调试生成的汇编

在龙芯服务器，loongnix系统上将源代码编译为汇编代码命令如下，

```bash
gcc test.s io.c -Wa,--gdwarf2 -o test       # 也可以利用静态链接库进行编译
gdb test
```

> gdb 常用命令：
> ```
> b 20    # 在第 20 行设置断点
> r       # 运行程序
> n       # 单步执行
> c       # 继续执行
> i r     # 查看寄存器
> ```

### 测试

测试分为两个部分：

- 功能测试：测试 `cminusfc` 是否能够正确生成汇编代码
- 性能测试：测试 `cminusfc` 生成的汇编代码的性能

测试样例、评测脚本见 `tests/5-bonus` 目录。

评测脚本的使用说明见 [评测脚本.md](评测脚本.md)


## 4. 提交

### 目录结构

与本次实验相关的目录结构如下：

```bash
.
├── CMakeLists.txt
├── Documentations
│   ├── ...
│   └── 5-bonus
│       ├── README.md                  # Lab5 实验文档说明（你在这里）
│       └── 寄存器分配引论17-19.pdf      # 寄存器分配相关资料
├── include
│   ├── ...
│   └── codegen.hpp                    # 代码生成相关头文件
├── Reports
│   ├── ...
│   └── 5-bonus
│       └── report.md                  # lab5 需提交的实验报告（需要上交）
├── src
│   ├── ...
│   └── codegen
│       ├── CMakeLists.txt
│       └── codegen.cpp                # 代码生成相关实现，包含简单的代码生成示例
└── tests
    ├── ...
    └── 5-bonus
        ├── test.py                    # 助教提供的测试脚本
        └── testcases                  # 助教提供的测试样例
```

如果需要，可以添加新的文件，或者修改已有文件。


### 提交要求、评分标准

- 提交要求：
  需要提交实验代码、实验报告。答辩时间待定

  - 提交方式：
    - 代码提交：在希冀课程平台上发布的作业中填写 GitLab 链接。
      如果遇到在平台上提交异常的问题，请通过邮件联系助教，助教将收取截止日期之前，学生在 gitlab 仓库最近一次 commit 内容进行评测。
    - 报告提交：`将 Reports/5/report.md` 导出成 pdf 文件
  - 截止时间：
    2023 年 3 月初，具体时间待定

- 评分标准：
  - 实验分数组成如下：
    测试用例的通过情况
    实验报告
    答辩情况
  - 禁止执行恶意代码，违者本次实验0分处理

- 关于抄袭和雷同
  经过助教和老师判定属于实验抄袭或雷同情况，所有参与方一律零分，不接受任何解释和反驳。

如有任何问题，可以通过邮件联系助教。



## 附录：栈分配策略

主要思想是：
把所有变量存储在内存中，仅当需要计算时，才将变量临时加载到寄存器上，完成后写回内存。

> 可以参考《寄存器分配引论》中的[相关章节](寄存器分配引论17-19.pdf)

### 示例 1：计算加法并输出

```c
int main(void) {
    int a;
    int b;
    int c;
    a = 17;
    b = 34;
    c = a + b;
    output(c);
    return 0;
}
```

以下是 gcc 生成的汇编代码（经过一定更改）：

```mips
    .text
    .globl  main
    .type   main, @function
main:
    addi.d  $sp, $sp, -32   # 更改栈指针
    st.d    $ra, $sp, 24    # 保存返回地址
    st.d    $fp, $sp, 16    # 保存帧指针
    addi.d  $fp, $sp, 32    # fp = sp + 32  当前函数栈帧底位置

    # a = 17
    addi.w  $t0, $zero, 17  # t0 = 0x11
    st.w    $t0, $fp, -20   # fp-20 = t0，将值保存在栈上

    # b = 34
    addi.w  $t0, $zero, 34  # t0 = 0x22
    st.w    $t0, $fp, -24   # fp-24 = t0

    # c = a + b
    ld.w    $t1, $fp, -20   # t1 = fp-20
    ld.w    $t0, $fp, -24   # t0 = fp-24
    add.w   $t0, $t1, $t0   # t0 = t1 + t0
    st.w    $t0, $fp, -28   # fp-28 = t0

    # output(c)
    ldptr.w $t0, $fp, -28   # t0 = fp-28
    or      $a0, $t0, $zero # a0 = t0
    bl      output          # 调用 output
    # return 0
    or  $t0, $zero, $zero   # t0 = 0
    or  $a0, $t0, $zero     # 设置返回值 a0 = t0    在此处设置断点

    ld.d    $ra, $sp, 24    # 恢复返回地址
    ld.d    $fp, $sp, 16    # 恢复帧指针
    addi.d  $sp, $sp, 32    # 恢复栈指针
    jr      $ra             # 跳转到返回地址
```

在程序设置返回值之后，恢复返回地址、帧指针、栈指针的寄存器前，设置断点，可以观察到栈中的变量值：


> 地址以 4 字节为单位

```
地址        内容         地址与寄存器的关系     描述
ffffff3190        0            fp
ffffff318c        ff
ffffff3188  f7e7c774    sp+24           存储 main 函数的返回地址 fff7e7c774，位于 __libc_start_main+228
ffffff3184         0
ffffff3180         0    sp+16           存储原来的 fp = 0
ffffff317c        11            fp-20   存储 a
ffffff3178        22            fp-24   存储 b
ffffff3174        33            fp-28   存储 c = a + b
ffffff3170  f7fd4598    sp
```

### 示例 2：调用函数访问数组

```c
void store(int arr[], int index, int value) {
    arr[index] = value;
}

int main(void) {
    int arr[10];
    store(arr, 5, 17);
}
```

```mips
    .text
    .globl  store
    .type   store, @function
store:
    addi.d  $sp, $sp, -32   # 更改栈指针
    st.d    $fp, $sp, 24    # 保存帧指针
    addi.d  $fp, $sp, 32    # fp = sp + 32

    st.d    $a0, $fp, -24   # fp-24 = a0    保存第一个参数 arr
    or      $t1, $a1, $zero # t1 = a1
    or      $t0, $a2, $zero # t0 = a2
    slli.w  $t1, $t1, 0     # t1 = t1 << 0  因为 int 是 32 位，而寄存器是 64 位，gcc 选择保留低 32 位，如果寄存器高 32 位为 0 可以忽略这一步
    st.w    $t1, $fp, -28   # fp-28 = t1    保存第二个参数 index
    slli.w  $t0, $t0, 0
    st.w    $t0, $fp, -32   # fp-32 = t0    保存第三个参数 value

    # arr[index] = value
    ldptr.w $t0, $fp, -28   # t0 = fp-28    取出 index
    slli.d  $t0, $t0, 2     # t0 = t0 << 2
    ld.d    $t1, $fp, -24   # t1 = fp-24    取出 arr
    add.d   $t0, $t1, $t0   # t0 = t1 + t0
    ld.w    $t1, $fp, -32   # t1 = fp-32    取出 value
    stptr.w $t1, $t0, 0     # t0[0] = t1    arr[index] = value  在此处设置断点

    ld.d    $fp, $sp, 24    # 恢复帧指针
    addi.d  $sp, $sp, 32    # 恢复栈指针
    jr      $ra             # 跳转到返回地址


    .globl  main
    .type   main, @function
main:
    addi.d  $sp, $sp, -64   # 更改栈指针
    st.d    $ra, $sp, 56    # 保存返回地址
    st.d    $fp, $sp, 48    # 保存帧指针
    addi.d  $fp, $sp, 64    # fp = sp + 64

    # store(arr, 5, 17);
    addi.d  $t0, $fp, -56   # t0 = fp-56    arr 的地址
    addi.w  $a2, $zero, 17  # a2 = 17       value = 0x11
    addi.w  $a1, $zero, 5   # a1 = 5        index = 5
    or      $a0, $t0, $zero # a0 = t0       arr
    bl      store           # 调用 store

    # return 0
    or  $t0, $zero, $zero   # t0 = 0
    or  $a0, $t0, $zero     # 设置返回值 a0 = t0

    ld.d    $ra, $sp, 56    # 恢复返回地址
    ld.d    $fp, $sp, 48    # 恢复帧指针
    addi.d  $sp, $sp, 64    # 恢复栈指针
    jr      $ra             # 跳转到返回地址
```


在执行完 store 函数中的 `stptr` 后，恢复帧指针和栈指针前，栈的情况如下：

> 地址以 8 字节为单位

```
地址        内容         地址与寄存器的关系     描述
ffffff3190          0          原fp
ffffff3188  fff7e7c774  原sp+56         存储 main 函数的返回地址，位于 __libc_start_main+228
ffffff3180           0  原sp+48         存储 main 函数保存的 fp
ffffff3178   120000920                  存储 arr[8], arr[9]
ffffff3170  fff7fd4598                  存储 arr[6], arr[7]
ffffff3168  11ffff32f8                  存储 arr[4], arr[5]，可以看到 arr[5] 变成了 0x11
ffffff3160  fff7fd45c0                  存储 arr[2], arr[3]
ffffff3158  fff7fd4618          原fp-56 存储 arr[0], arr[1]
ffffff3150         228  原sp    fp
ffffff3148  ffffff3190  sp+24           存储 main 函数的 fp，是下面原fp 的地址
ffffff3140  fff7fc0ea8
ffffff3138  ffffff3158          fp-24   存储 arr 的地址，即下面 arr 的地址
ffffff3130   500000011  sp      fp-32   存储 value 和 index     value=0x11 位于 fp-32，index=5 位于 fp-28
```

此时 RA=120000820，为 main 函数中 `call store` 的下一条指令的地址
