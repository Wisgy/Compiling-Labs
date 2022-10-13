#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Module.h"
#include "Type.h"

#include <iostream>
#include <memory>

#ifdef DEBUG
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl; // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

#define CONST_INT(num) ConstantInt::get(num, module)

#define CONST_FP(num) ConstantFP::get(num, module)

int main() {
    auto module = new Module("Cminus code");
    auto builder = new IRBuilder(nullptr, module);
    Type *Int32Type = Type::get_int32_type(module);

    //main function
    auto mainFun = Function::create(FunctionType::get(Int32Type, {}), "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);
    //begin
    builder->set_insert_point(bb);
    auto aAlloca = builder->create_alloca(Int32Type);
    auto iAlloca = builder->create_alloca(Int32Type);
    builder->create_store(CONST_INT(10), aAlloca);
    builder->create_store(CONST_INT(0),iAlloca);
    auto cmpBB = BasicBlock::create(module, "cmp", mainFun);
    auto whileBB = BasicBlock::create(module, "while", mainFun);
    auto retBB = BasicBlock::create(module, "ret", mainFun);
    builder->create_br(cmpBB);
    //compare i with 10
    builder->set_insert_point(cmpBB);
    auto iLoad = builder->create_load(iAlloca);
    auto cmp = builder->create_icmp_lt(iLoad, CONST_INT(10));
    auto br = builder->create_cond_br(cmp, whileBB, retBB);
    //while
    builder->set_insert_point(whileBB);
    iLoad = builder->create_load(iAlloca);
    auto add = builder->create_iadd(iLoad, CONST_INT(1));
    builder->create_store(add, iAlloca);
    iLoad = builder->create_load(iAlloca);
    auto aLoad = builder->create_load(aAlloca);
    add = builder->create_iadd(aLoad, iLoad);
    builder->create_store(add, aAlloca);
    builder->create_br(cmpBB);
    //ret
    builder->set_insert_point(retBB);
    auto retLoad = builder->create_load(aAlloca);
    builder->create_ret(retLoad);

    std::cout << module->print();
    delete module;
    return 0;
}