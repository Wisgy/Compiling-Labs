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
    Type *floatType = Type::get_float_type(module);

    //main function
    auto mainFun = Function::create(FunctionType::get(Int32Type, {}), "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);
    //begin
    builder->set_insert_point(bb);
    auto retAlloca = builder->create_alloca(Int32Type);
    auto aAlloca = builder->create_alloca(floatType);
    builder->create_store(CONST_FP(5.555), aAlloca);
    auto truebb = BasicBlock::create(module, "true", mainFun);
    auto falsebb = BasicBlock::create(module, "false", mainFun);
    auto retbb = BasicBlock::create(module, "ret", mainFun);
    auto aLoad = builder->create_load(aAlloca);
    auto comp = builder->create_fcmp_gt(aLoad, CONST_FP(1));
    auto br = builder->create_cond_br(comp, truebb, falsebb);
    //true
    builder->set_insert_point(truebb);
    builder->create_store(CONST_INT(233), retAlloca);
    builder->create_br(retbb);
    //false
    builder->set_insert_point(falsebb);
    builder->create_store(CONST_INT(0), retAlloca);
    builder->create_br(retbb);
    //ret
    builder->set_insert_point(retbb);
    builder->create_ret(retAlloca);

    std::cout << module->print();
    delete module;
    return 0;
}