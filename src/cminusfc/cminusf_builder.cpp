/*
 * 声明：本代码为 2020 秋 中国科大编译原理（李诚）课程实验参考实现。
 * 请不要以任何方式，将本代码上传到可以公开访问的站点或仓库
 */

#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())


// TODO: Global Variable Declarations 
// You can define global variables here
// to store state. You can expand these
// definitions if you need to.
#define CONST_ZERO(type) ConstantZero::get(type, module.get())
Value *value;
Type *type;

// function that is being built
Function *cur_fun = nullptr;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

void CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());

    for (auto decl : node.declarations) {
        decl->accept(*this);
        // cout<<"ok"<<endl;
    }
}

void CminusfBuilder::visit(ASTNum &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(node.type == TYPE_INT){
        value = CONST_INT(node.i_val);
        type = INT32_T;
    }
    else {
        value = CONST_FP(node.f_val);
        type = FLOAT_T;
    }
}

void CminusfBuilder::visit(ASTVarDeclaration &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(scope.in_global()){//global
        if(node.num){
            value = GlobalVariable::create(node.id, module.get(), ArrayType::get(type, node.num->i_val), false, CONST_ZERO(type));
        }
        else{
            value = GlobalVariable::create(node.id, module.get(), type, false, CONST_ZERO(type));
        }
    }
    else{
        if(node.num){//array
            node.num->accept(*this);
            builder->create_alloca(ArrayType::get(type, node.num->i_val));
        }
        else{//number
            value = builder->create_alloca(INT32_T);
        }
    }
    scope.push(node.id, value);
}

void CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        //!TOiDO: Please accomplish param_types.
        if(param->type == TYPE_INT){
            if(param->isarray)param_types.push_back(INT32PTR_T);
            else param_types.push_back(INT32_T);
        }
        else{
            if(param->isarray)param_types.push_back(FLOATPTR_T);
            else param_types.push_back(FLOAT_T);
        }
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto fun = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, fun);
    cur_fun = fun;
    auto funBB = BasicBlock::create(module.get(), "entry", fun);
    builder->set_insert_point(funBB);
    scope.enter();
    std::vector<Value *> args;
    for (auto arg = fun->arg_begin(); arg != fun->arg_end(); arg++) {
        args.push_back(*arg);
    }
    for (long unsigned int i = 0; i < node.params.size(); ++i) {
        //!TOiDO: You need to deal with params
        // and store them in the scope.
        type = param_types[i];
        node.params[i]->accept(*this);
        builder->create_store(args[i], value);
        scope.push(node.params[i]->id, value);
    }
    node.compound_stmt->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr) 
    {
        if (cur_fun->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (cur_fun->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
}

void CminusfBuilder::visit(ASTParam &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    value = builder->create_alloca(type);
}

void CminusfBuilder::visit(ASTCompoundStmt &node) {
    //!TOiDO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements. 
    scope.enter();

    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }
    scope.exit();
}

void CminusfBuilder::visit(ASTExpressionStmt &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(node.expression){
        node.expression->accept(*this);
    }
}

void CminusfBuilder::visit(ASTSelectionStmt &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    scope.enter();
    node.expression->accept(*this);
    auto exitbb = BasicBlock::create(module.get(), "next", cur_fun);
    auto truebb = BasicBlock::create(module.get(), "true", cur_fun);
    BasicBlock * falsebb;
    if(node.else_statement){
        falsebb = BasicBlock::create(module.get(), "false", cur_fun);
        builder->create_cond_br(value, truebb, falsebb);
    }
    else {
        builder->create_cond_br(value, truebb, exitbb);
    }
    //true TODO:ifelse return to where?
    builder->set_insert_point(truebb);
    node.if_statement->accept(*this);
    // if(builder->get_insert_block()->get_terminator()!=nullptr)builder->create_br(retbb);
    builder->create_br(exitbb);
    if(node.else_statement){//false
        builder->set_insert_point(falsebb);
        node.else_statement->accept(*this);
        // if(builder->get_insert_block()->get_terminator()!=nullptr)builder->create_br(retbb);
        builder->create_br(exitbb);
    }
    scope.exit();
    //next
    builder->set_insert_point(exitbb);
}

void CminusfBuilder::visit(ASTIterationStmt &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    //true TODO:return to where?
    scope.enter();
    auto cmpBB = BasicBlock::create(module.get(), "cmp", cur_fun);
    auto whileBB = BasicBlock::create(module.get(), "while", cur_fun);
    auto exitBB = BasicBlock::create(module.get(), "exit", cur_fun);
    builder->create_br(cmpBB);
    builder->set_insert_point(cmpBB);
    node.expression->accept(*this);
    builder->create_cond_br(value, whileBB, exitBB);
    builder->set_insert_point(whileBB);
    node.statement->accept(*this);
    // if(builder->get_insert_block()->get_terminator()!=nullptr)builder->create_br(retbb);
    builder->create_br(cmpBB);
    builder->set_insert_point(exitBB);
    scope.exit();
}

void CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
    } else {
        //!TOiDO: The given code is incomplete.
        // You need to solve other return cases (e.g. return an integer).
        node.expression->accept(*this);
        auto return_type = cur_fun->get_return_type();
        if(return_type != type){//type conversion
                if(return_type == FLOAT_T) value = builder->create_sitofp(value, type);
                else if(type == return_type) value = builder->create_fptosi(value, return_type);
            }
        builder->create_ret(value);
    }
}

void CminusfBuilder::visit(ASTVar &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(node.expression){//array
        node.expression->accept(*this);
        if(type == FLOAT_T){//type conversion
            value = builder->create_fptosi(value, INT32_T);
            type = INT32_T;
        }
        // if(){ TODO:how to compare data of Value type with zero of int type
        //     value = scope.find("neg_idx_except");
        //     builder->create_call(value, {});
        // }
        auto base_addr = scope.find(node.id);
        value = builder->create_gep(base_addr, {CONST_INT(0), value});
        type = value->get_type();
    }
    else{//number
        value = scope.find(node.id);
        type = value->get_type();
    }
}

void CminusfBuilder::visit(ASTAssignExpression &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    node.expression->accept(*this);
    auto return_value = value;
    auto return_type = type;
    node.var->accept(*this);
    auto target_addr = value;
    auto target_type = type;
    if(target_type != return_type){//type conversion
        if(target_type == FLOATPTR_T){
            return_value = builder->create_sitofp(return_value, return_type);
            type = target_type;
        }
        else{
            return_value = builder->create_fptosi(return_value, target_type);
            type = target_type;
        }
    }
    value = return_value;
    builder->create_store(return_value, target_addr);
}

void CminusfBuilder::visit(ASTSimpleExpression &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(node.additive_expression_r){//additive-expression relop additive-expression
        node.additive_expression_l->accept(*this);
        auto l_value = value;
        auto l_type = type;
        node.additive_expression_r->accept(*this);
        auto r_value = value;
        auto r_type = type;
        if(l_type == FLOAT_T || r_type == FLOAT_T){//type conversion
            if(l_type != FLOAT_T) l_value = builder->create_sitofp(l_value, FLOAT_T);
            else if(r_type != FLOAT_T)r_value = builder->create_sitofp(r_value, FLOAT_T);
            type = FLOAT_T;
        }
        else type = INT32_T;
        switch (node.op)
        {
        case OP_LE:
            if(type == FLOAT_T)value = builder->create_fcmp_le(l_value, r_value);
            else value= builder->create_icmp_le(l_value, r_value);
            break;
        case OP_LT:
            if(type == FLOAT_T)value = builder->create_fcmp_lt(l_value, r_value);
            else value= builder->create_icmp_lt(l_value, r_value);
            break;
        case OP_GT:
            if(type == FLOAT_T)value = builder->create_fcmp_gt(l_value, r_value);
            else value= builder->create_icmp_gt(l_value, r_value);
            break;
        case OP_GE:
            if(type == FLOAT_T)value = builder->create_fcmp_ge(l_value, r_value);
            else value= builder->create_icmp_ge(l_value, r_value);
            break;
        case OP_EQ:
            if(type == FLOAT_T)value = builder->create_fcmp_eq(l_value, r_value);
            else value= builder->create_icmp_eq(l_value, r_value);
            break;
        case OP_NEQ:
            if(type == FLOAT_T)value = builder->create_fcmp_ne(l_value, r_value);
            else value= builder->create_icmp_ne(l_value, r_value);
            break;
        default:
            break;
        }
        type = INT1_T;
    }
    else{//additive-expression
        node.additive_expression_l->accept(*this);
    }
}

void CminusfBuilder::visit(ASTAdditiveExpression &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(node.additive_expression){//additive-expression addop term
        node.additive_expression->accept(*this);
        auto l_value = value;
        auto l_type = type;
        node.term->accept(*this);
        auto r_value = value;
        auto r_type = type;
        if(l_type == FLOAT_T || r_type == FLOAT_T){//type conversion
            if(l_type != FLOAT_T) l_value = builder->create_sitofp(l_value, FLOAT_T);
            else if(r_type != FLOAT_T)r_value = builder->create_sitofp(r_value, FLOAT_T);
        }
        if(node.op == OP_PLUS){// add
            if(l_type == FLOAT_T || r_type == FLOAT_T){
                value = builder->create_fadd(l_value, r_value);
                type = FLOAT_T;
            }
            else{
                value = builder->create_iadd(l_value, r_value);
                type = INT32_T;
            }
        }
        else{// sub
            if(l_type == FLOAT_T || r_type == FLOAT_T){
                value = builder->create_fsub(l_value, r_value);
                type = FLOAT_T;
            }
            else{
                value = builder->create_isub(l_value, r_value);
                type = INT32_T;
            }
        }
    }
    else{//term
        node.term->accept(*this);
    }
    
}

void CminusfBuilder::visit(ASTTerm &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    if(node.term){//term mulop factor
        node.term->accept(*this);
        auto l_value = value;
        auto l_type = type;
        node.factor->accept(*this);
        auto r_value = value;
        auto r_type = type;
        if(l_type == FLOAT_T || r_type == FLOAT_T){//type conversion
            if(l_type != FLOAT_T) l_value = builder->create_sitofp(l_value, FLOAT_T);
            else if(r_type != FLOAT_T)r_value = builder->create_sitofp(r_value, FLOAT_T);
        }
        if(node.op == OP_MUL){// mul
            if(l_type == FLOAT_T || r_type == FLOAT_T){
                value = builder->create_fmul(l_value, r_value);
                type = FLOAT_T;
            }
            else{
                value = builder->create_imul(l_value, r_value);
                type = INT32_T;
            }
        }
        else{// div
            if(l_type == FLOAT_T || r_type == FLOAT_T){
                value = builder->create_fdiv(l_value, r_value);
                type = FLOAT_T;
            }
            else{
                value = builder->create_isdiv(l_value, r_value);
                type = INT32_T;
            }
        }
    }
    else{//factor
        node.factor->accept(*this);
    }

}

void CminusfBuilder::visit(ASTCall &node) {
    //!TOiDO: This function is empty now.
    // Add some code here.
    Function* func = (Function*)scope.find(node.id);
    auto arg_types = func->get_function_type();
    std::vector<Value*> args;
    if(node.args.size() != 0){//exist args
        for(int i = node.args.size()-1;i >= 0;i--){
            node.args[i]->accept(*this);
            auto arg_type = arg_types->get_param_type(i);
            if(arg_type != type){//type conversion
                if(arg_type == FLOAT_T) value = builder->create_sitofp(value, arg_type);
                else if(type == FLOAT_T) value = builder->create_fptosi(value, arg_type);
            }
            args.push_back(value);
        }
    }
    //void
    value = builder->create_call(func, args);
    type = func->get_return_type();
}
