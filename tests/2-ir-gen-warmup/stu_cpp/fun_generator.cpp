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

    //callee function
    auto calleeFun = Function::create(FunctionType::get(Int32Type, {Int32Type}), "callee", module);
    auto bb = BasicBlock::create(module, "entry", calleeFun);
    builder->set_insert_point(bb);
    auto retAlloca = builder->create_alloca(Int32Type);
    auto aAlloca = builer->create_alloca(Int32Type);
    std::vector<Value *> args;  
    for (auto arg = calleeFun->arg_begin(); arg != calleeFun->arg_end(); arg++) {
        args.push_back(*arg);
    }
    builder->create_store(args[0], aAlloca);
    auto aLoad = builder->create_load(aAlloca);
    auto mul = builder->create_imul(CONST_INT(2), aLoad);
    builder->create_store(mul, retAlloca);
    builder->create_ret(retAlloca);

    //main function
    auto mainFun = Function::create(FunctionType::get(Int32Type, {}), "main", module);
    bb = BasicBlock::create(module,"entry", mainFun);
    builder->set_insert_point(bb);
    retAlloca = builer->create_alloca(Int32Type);
    auto call = builder->create_call(calleeFun, {CONST_INT(110)});
    builder->create_store(call, retAlloca);
    builder->create_ret(call);

    std::cout << module->print();
    delete module;
    return 0;
}