#include "GVN.h"

#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

using namespace GVNExpression;
using std::string_literals::operator""s;
using std::shared_ptr;
// std::set<Value>
static auto get_const_int_value = [](Value *v) { return dynamic_cast<ConstantInt *>(v)->get_value(); };
static auto get_const_fp_value = [](Value *v) { return dynamic_cast<ConstantFP *>(v)->get_value(); };
// Constant Propagation helper, folders are done for you
Constant *ConstFolder::compute(Instruction *instr, Constant *value1, Constant *value2) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::add: return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub: return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul: return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv: return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd: return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub: return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul: return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv: return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);

    case Instruction::cmp:
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
        case CmpInst::EQ: return ConstantInt::get(get_const_int_value(value1) == get_const_int_value(value2), module_);
        case CmpInst::NE: return ConstantInt::get(get_const_int_value(value1) != get_const_int_value(value2), module_);
        case CmpInst::GT: return ConstantInt::get(get_const_int_value(value1) > get_const_int_value(value2), module_);
        case CmpInst::GE: return ConstantInt::get(get_const_int_value(value1) >= get_const_int_value(value2), module_);
        case CmpInst::LT: return ConstantInt::get(get_const_int_value(value1) < get_const_int_value(value2), module_);
        case CmpInst::LE: return ConstantInt::get(get_const_int_value(value1) <= get_const_int_value(value2), module_);
        }
    case Instruction::fcmp:
        switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
        case FCmpInst::EQ: return ConstantInt::get(get_const_fp_value(value1) == get_const_fp_value(value2), module_);
        case FCmpInst::NE: return ConstantInt::get(get_const_fp_value(value1) != get_const_fp_value(value2), module_);
        case FCmpInst::GT: return ConstantInt::get(get_const_fp_value(value1) > get_const_fp_value(value2), module_);
        case FCmpInst::GE: return ConstantInt::get(get_const_fp_value(value1) >= get_const_fp_value(value2), module_);
        case FCmpInst::LT: return ConstantInt::get(get_const_fp_value(value1) < get_const_fp_value(value2), module_);
        case FCmpInst::LE: return ConstantInt::get(get_const_fp_value(value1) <= get_const_fp_value(value2), module_);
        }
    default: return nullptr;
    }
}

Constant *ConstFolder::compute(Instruction *instr, Constant *value1) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::sitofp: return ConstantFP::get((float)get_const_int_value(value1), module_);
    case Instruction::fptosi: return ConstantInt::get((int)get_const_fp_value(value1), module_);
    case Instruction::zext: return ConstantInt::get((int)get_const_int_value(value1), module_);
    default: return nullptr;
    }
}

Constant *ConstFolder::compute(Instruction::OpID op, Constant *value1, Constant *value2) {
    switch (op) {
    case Instruction::add: return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub: return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul: return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv: return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd: return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub: return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul: return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv: return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);
    default: return nullptr;
    }
}

namespace utils {
static std::string print_congruence_class(const CongruenceClass &cc) {
    std::stringstream ss;
    if (cc.index_ == 0) {
        ss << "top class\n";
        return ss.str();
    }
    ss << "\nindex: " << cc.index_ << "\nleader: " << cc.leader_->print()
       << "\nvalue phi: " << (cc.value_phi_ ? cc.value_phi_->print() : "nullptr"s)
       << "\nvalue expr: " << (cc.value_expr_ ? cc.value_expr_->print() : "nullptr"s) << "\nmembers: {";
    for (auto &member : cc.members_)
        ss << member->print() << "; ";
    ss << "}\n";
    return ss.str();
}

static std::string dump_cc_json(const CongruenceClass &cc) {
    std::string json;
    json += "[";
    for (auto member : cc.members_) {
        if (auto c = dynamic_cast<Constant *>(member))
            json += member->print() + ", ";
        else
            json += "\"%" + member->get_name() + "\", ";
    }
    json += "]";
    return json;
}

static std::string dump_partition_json(const GVN::partitions &p) {
    std::string json;
    json += "[";
    for (auto cc : p)
        json += dump_cc_json(*cc) + ", ";
    json += "]";
    return json;
}

static std::string dump_bb2partition(const std::map<BasicBlock *, GVN::partitions> &map) {
    std::string json;
    json += "{";
    for (auto [bb, p] : map)
        json += "\"" + bb->get_name() + "\": " + dump_partition_json(p) + ",\n";
    json += "}";
    return json;
}

// logging utility for you
static void print_partitions(const GVN::partitions &p) {
    if (p.empty()) {
        LOG_DEBUG << "empty partitions\n";
        return;
    }
    std::string log;
    for (auto &cc : p)
        log += print_congruence_class(*cc);
    LOG_DEBUG << log; // please don't use std::cout
}
} // namespace utils

GVN::partitions GVN::join(const partitions &P1, const partitions &P2) {
    // TODO: do intersection pair-wise
    if((*P1.begin())->index_==0)
        return P2;
    if((*P2.begin())->index_==0)
        return P1;
    partitions P;
    for(auto &Ci : P1)
        for(auto &Cj : P2){
            auto Ck = intersect(Ci, Cj);
            if(Ck!=nullptr)
                P.insert(Ck);
        }
    return P;
}

std::shared_ptr<CongruenceClass> GVN::intersect(std::shared_ptr<CongruenceClass> Ci,
                                                std::shared_ptr<CongruenceClass> Cj) {
    // TODO:
    std::shared_ptr<CongruenceClass> Ck = createCongruenceClass();
    size_t index;
    //Ck = Ci ∩ Cj
    if(Ci->value_expr_==Cj->value_expr_)
        Ck->value_expr_=Ci->value_expr_;
    else 
        Ck->value_expr_=nullptr;
    if(Ci->value_phi_==Cj->value_phi_)
        Ck->value_phi_=Ci->value_phi_;
    else
        Ck->value_phi_=nullptr;
    for(auto &mem : Ci->members_){
        if(Cj->members_.count(mem))
            Ck->members_.insert(mem);
    }
    if(Ci->leader_==Cj->leader_)
        Ck->leader_=Ci->leader_;
    else{
        Ck->leader_=*Ck->members_.begin();
    } 
    if(Ck->members_.size()==0){//if Ck == {} delete Ck;
        return nullptr;
    }
    else if(Ck->value_expr_==nullptr){//Ck!={}, and Ck does not have value number
        Ck->value_expr_=valueExpr(dynamic_cast<Instruction*>(Ck->leader_), pout_[func_->get_entry_block()]);
        Ck->value_phi_=std::dynamic_pointer_cast<PhiExpression>(Ck->value_expr_);
    }
    Ck->index_=next_value_number_++;
    return Ck;
}

void GVN::detectEquivalences() {
    bool changed = false;
    // initialize pout with top
    partitions top;
    auto top_c=createCongruenceClass(0);
    top.insert(top_c);
    for (auto &bb : func_->get_basic_blocks()){
        pout_[&bb] = top;
    }
    // iterate until converge
    // std::cout<<"func:"<<(func_->get_name())<<std::endl;
    int times=0;
    do {
        changed = false;
        // see the pseudo code in documentation
        for (auto &bb1 : func_->get_basic_blocks()) { // you might need to visit the blocks in depth-first order
            partitions pout;
            auto bb=&bb1;
            std::cout<<(bb->get_name())<<std::endl;
            // get PIN of bb by predecessor(s)
            auto pre_bb = bb->get_pre_basic_blocks();
            // iterate through all instructions in the block
            // and the phi instruction in all the successors
            auto pre_bb1 = pre_bb.begin();
            auto pre_bb2 = pre_bb.begin();
            // std::cout<<pre_bb.size()<<std::endl;
            if(pre_bb.size()==2){
                pre_bb2++;
                pout = join(pout_[*pre_bb1], pout_[*pre_bb2]);
            }
            else if(pre_bb.size()==1){
                // for(auto &C : pout_[*pre_bb1]){
                //     std::shared_ptr<CongruenceClass> Ck = createCongruenceClass(C->index_);
                //     Ck->leader_=C->leader_;
                //     Ck->value_expr_=C->value_expr_;
                //     Ck->value_phi_=C->value_phi_;
                //     Ck->members_=C->members_;
                //     pout.insert(Ck);
                // }
                pout = clone(pout_[*pre_bb1]);
            }
            else{
                partitions entry;
                for(auto &arg : func_->get_args()){//arguments
                    auto C = createCongruenceClass(next_value_number_++);
                    C->leader_=arg;
                    C->members_.insert(arg);
                    C->value_expr_=VarExpression::create(arg);
                    entry.insert(C);
                }
                for(auto &g_v : m_->get_global_variable()){//global_value
                    auto g_V=&g_v;
                    auto C = createCongruenceClass(next_value_number_++);
                    C->leader_=g_V;
                    C->members_.insert(g_V);
                    C->value_expr_=VarExpression::create(g_V);
                    entry.insert(C);
                }
                if(entry.size()==0)pout=clone(top);
                else pout = entry;
            }
            for(auto &instr : bb->get_instructions()){
                // std::cout<<"trans:"<<(instr.get_name())<<std::endl;
                if(instr.is_br()||instr.is_phi()||instr.is_ret()||instr.is_store()||instr.is_call()&&instr.get_name()=="")continue;
                pout = transferFunction(&instr,pout);
            }
            //add copy statement
            auto succ_bb = bb->get_succ_basic_blocks();
            for(auto &suc_bb : succ_bb){
                for(auto &ins : suc_bb->get_instructions()){
                    auto instr = &ins;
                    if(instr->is_phi()){
                        std::cout<<"copy:"<<(instr->get_name())<<std::endl;
                        for(auto &Ci : pout){
                            if(Ci->members_.count(instr)){
                                Ci->members_.erase(instr);
                                if(Ci->members_.size()==0)pout.erase(Ci);
                                break;
                            }
                        }
                        Value* oper;
                        if(instr->get_operand(1)==bb)oper = instr->get_operand(0);
                        else if(instr->get_operand(3)==bb)oper = instr->get_operand(2);
                        else {
                            auto C = std::make_shared<CongruenceClass>(next_value_number_++);
                            C->members_.insert(instr);
                            C->leader_=instr;
                            C->value_expr_=VarExpression::create(nullptr);
                            pout.insert(C);
                            continue;
                        }
                        bool flag=true;
                        for(auto &CC:pout){
                            if(CC->members_.count(oper)||CC->leader_==oper){
                                CC->members_.insert(instr);
                                flag=false;
                                break;
                            }
                        }
                        if(flag){
                            std::shared_ptr<CongruenceClass> C= std::make_shared<CongruenceClass>(next_value_number_++);
                            if(dynamic_cast<Constant *>(oper)){//create C
                                C->leader_=dynamic_cast<Constant *>(oper);
                                C->value_expr_=ConstantExpression::create(dynamic_cast<Constant *>(oper));
                            }
                            else{
                                C->leader_=instr;
                                C->value_expr_=VarExpression::create(instr);
                            }
                            C->members_.insert(instr);
                            if((*pout.begin())->index_==0)pout.clear();
                            pout.insert(C);
                        }
                    }
                    else break;
                }
            }
            
            // check changes in pout
            if(!(pout_[bb] == pout)){
                changed = true;
                std::cout<<"before"<<std::endl;
                for(auto & C : pout_[bb]){
                    std::cout<<"[";
                    for(auto &mem : C->members_)
                        std::cout<<(mem->get_name())<<",";
                    std::cout<<"]"<<std::endl;
                }
                std::cout<<"after"<<std::endl;
                for(auto & C : pout){
                    std::cout<<"[";
                    for(auto &mem : C->members_)
                        std::cout<<(mem->get_name())<<",";
                    std::cout<<"]"<<std::endl;
                }
                std::cout<<""<<std::endl;
            }
            pout_[bb] = clone(pout);
        }
        times++;
        std::cout<<"times:"<<times<<std::endl;
        if(times==1)
            std::cout<<"pause"<<std::endl;
        if(times==2)
            std::cout<<"pause"<<std::endl;
        if(times==3)
            std::cout<<"pause"<<std::endl;
        if(times==7)
            std::cout<<"pause"<<std::endl;
    } while (changed);
    
}

shared_ptr<Expression> GVN::valueExpr(Instruction *instr, partitions& pin) {
    // TODO:
    if(instr==nullptr)return nullptr;
    // std::cout<<(instr->get_name())<<std::endl;
    auto op_num=instr->get_num_operand();
    std::shared_ptr<Expression> lhs,rhs;
    bool flag=true;
    if(instr->is_phi()){
        lhs = nullptr;
        rhs = nullptr;
        BasicBlock* left_bb = dynamic_cast<BasicBlock*>(instr->get_operand(1));
        BasicBlock* right_bb = op_num==4?dynamic_cast<BasicBlock*>(instr->get_operand(3)):NULL;
        if(left_bb){
            if(dynamic_cast<Constant*>(instr->get_operand(0)))
                lhs = ConstantExpression::create(dynamic_cast<Constant*>(instr->get_operand(0)));
            else{
                // for(auto &Ci : pout_[left_bb]){
                //     if(Ci->members_.count(instr->get_operand(0))){
                //         lhs = Ci->value_expr_;
                //         flag = false;
                //         break;
                //     }
                // }
                // if(lhs==nullptr)
                lhs=valueExpr(static_cast<Instruction *>(instr->get_operand(0)), pout_[left_bb]);
            }
        }
        if(right_bb){
            if(dynamic_cast<Constant*>(instr->get_operand(2)))
                rhs = ConstantExpression::create(dynamic_cast<Constant*>(instr->get_operand(0)));
            else{
                // for(auto &Cj : pout_[right_bb]){
                //     if(Cj->members_.count(instr->get_operand(2))){
                //         rhs = Cj->value_expr_;
                //         flag = false;
                //         break;
                //     }
                // }
                // if(rhs==nullptr)
                rhs=valueExpr(static_cast<Instruction *>(instr->get_operand(2)), pout_[right_bb]);
            }
        }
        if(lhs==rhs)return lhs;
        else return PhiExpression::create(lhs, rhs);
    }
    else if(instr->is_call()){//CallExpression
        if(func_info_->is_pure_function(static_cast<Function*>(instr->get_operand(0)))){//pure_function
            auto call_exp = CallExpression::create(static_cast<Function*>(instr->get_operand(0)));
            int op=1;
            for(;op<op_num;op++){
                flag = true;
                for(auto &C : pin){
                    if(C->members_.count(instr->get_operand(op))){
                        call_exp->arg_insert(C->value_expr_);
                        flag = false;
                        break;
                    }
                }
                if(flag)
                    call_exp->arg_insert(VarExpression::create(nullptr)); 
            }
            return call_exp;
        }
        else return CallExpression::create(instr);//not pure_function
    }
    else if(instr->is_gep()){//GepExpression
        auto expr = GepExpression::create();
        for(int op=0;op<op_num;op++){
            if(dynamic_cast<Constant *>(instr->get_operand(op)))
                expr->arg_insert(ConstantExpression::create(dynamic_cast<Constant *>(instr->get_operand(op))));
            else {
                flag = true;
                for(auto &C : pin){
                    if(C->members_.count(instr->get_operand(op))){
                        expr->arg_insert(C->value_expr_);
                        flag = false;
                        break;
                    }
                }
                if(flag)
                    expr->arg_insert(VarExpression::create(nullptr));
            }
        }
        return expr;
    }
    else if(instr->isBinary()||instr->is_cmp()||instr->is_fcmp()){
        bool lhs_is_cons = dynamic_cast<Constant *>(instr->get_operand(0));
        bool rhs_is_cons = dynamic_cast<Constant *>(instr->get_operand(1));
        if(lhs_is_cons)
            lhs=ConstantExpression::create(dynamic_cast<Constant *>(instr->get_operand(0)));
        else{
            for(auto &C : pin){
                if(C->members_.count(instr->get_operand(0))){
                    lhs=C->value_expr_;
                    flag=false;
                    break;
                }
            }
            if(flag)
                lhs=VarExpression::create(nullptr);
        }
        flag=true;
        if(rhs_is_cons)
            rhs=ConstantExpression::create(dynamic_cast<Constant *>(instr->get_operand(1)));
        else{
            for(auto &C : pin){
                if(C->members_.count(instr->get_operand(1))){
                    rhs=C->value_expr_;
                    flag=false;
                    break;
                }
            }
            if(flag)
                rhs=VarExpression::create(nullptr);
        }
        lhs_is_cons = lhs->is_constant();
        rhs_is_cons = rhs->is_constant();
        if(lhs_is_cons&&rhs_is_cons){
            auto cons = folder_->compute(instr, std::dynamic_pointer_cast<ConstantExpression>(lhs)->get_cons(), 
                                        std::dynamic_pointer_cast<ConstantExpression>(rhs)->get_cons());
            return ConstantExpression::create(cons);
        }
        else if(instr->isBinary())
            return BinaryExpression::create(instr->get_instr_type(), lhs, rhs);
        else if(instr->is_cmp())
            return CmpExpression::create(dynamic_cast<CmpInst *>(instr)->get_cmp_op(), lhs, rhs);
        else if(instr->is_fcmp())
            return FCmpExpression::create(dynamic_cast<FCmpInst *>(instr)->get_cmp_op(), lhs, rhs);
        
    }
    else if(instr->is_zext()||instr->is_fp2si()||instr->is_si2fp()){
        if(dynamic_cast<Constant *>(instr->get_operand(0)))
            return ConstantExpression::create(folder_->compute(instr, dynamic_cast<Constant *>(instr->get_operand(0))));
        for(auto &C : pin){
            if(C->members_.count(instr->get_operand(0))){
                lhs=C->value_expr_;
                flag=false;
                break;
            }
        }
        if(flag){
            lhs = VarExpression::create(nullptr);
            return SingleExpression::create(instr->get_instr_type(), lhs);
        }
        else if(lhs->is_constant())
            return ConstantExpression::create(folder_->compute(instr, std::dynamic_pointer_cast<ConstantExpression>(lhs)->get_cons()));
        else
            return SingleExpression::create(instr->get_instr_type(), lhs);
    } 
    else if(instr->is_alloca()||instr->is_load())
        return VarExpression::create(instr);
    else return nullptr;
}

// instruction of the form `x = e`, mostly x is just e (SSA), but for copy stmt x is a phi instruction in the
// successor. Phi values (not copy stmt) should be handled in detectEquiv
/// \param bb basic block in which the transfer function is called
GVN::partitions GVN::transferFunction(Instruction *x, partitions pin) {
    partitions pout = clone(pin);
    // TODO: get different ValueExpr by Instruction::OpID, modify pout
    //if x is in a class Ci in POUTs (I don't think this is needed)
    for(auto &Ci : pin){
        if(Ci->members_.count(x)){
            Ci->members_.erase(x);
            if(Ci->members_.size()==0)pout.erase(Ci);
        }
    }
    //ve = valueExpr(e)
    auto ve = valueExpr(x, pout);
    //vpf = valuePhiFunc(ve,PINs)
    Instruction* l_oper=NULL;
    Instruction* r_oper=NULL;
    if(x->isBinary()){
        l_oper=static_cast<Instruction*>(x->get_operand(0));
        r_oper=static_cast<Instruction*>(x->get_operand(1));
    }
    auto vpf = valuePhiFunc(ve, pin, l_oper, r_oper);
    //if ve or vpf is in a class Ci in POUTs
    bool flag = true;
    if(pin.size()!=0&&(*pin.begin())->index_==0)
        pout.clear();
    for(auto &Ci : pout){
        if(Ci->value_expr_==ve || (vpf!=nullptr&&Ci->value_phi_==vpf)){
            Ci->members_.insert(x);
            flag=false;
            break;
        }
    }
    if(flag&&ve!=nullptr){
        std::shared_ptr<CongruenceClass> Ck = std::make_shared<CongruenceClass>(next_value_number_++);
        Ck->value_expr_=ve;
        if(Ck->value_expr_->is_constant())Ck->leader_=std::dynamic_pointer_cast<ConstantExpression>(Ck->value_expr_)->get_cons();
        else Ck->leader_=x;
        if(vpf==nullptr&&ve->is_phi())Ck->value_phi_=std::dynamic_pointer_cast<PhiExpression>(ve);
        else Ck->value_phi_=vpf;
        Ck->members_.insert(x);
        pout.insert(Ck);
    }
    return pout;
}

shared_ptr<PhiExpression> GVN::valuePhiFunc(shared_ptr<Expression> ve, const partitions &P, Instruction *l_oper, Instruction *r_oper) {
    // TODO:
    if(ve==nullptr)return nullptr;
    if(!ve->is_binary())return nullptr;
    bool flag=false;
    auto lhs=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_lhs();
    auto rhs=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_rhs();
    if(lhs->is_phi()&&rhs->is_phi())
    // if(l_oper->is_phi()&&r_oper->is_phi())
        flag=true;
    if(flag){
        BasicBlock* lhs_left_bb=dynamic_cast<BasicBlock*>(l_oper->get_operand(1));
        BasicBlock* rhs_left_bb=dynamic_cast<BasicBlock*>(r_oper->get_operand(1));
        BasicBlock* lhs_right_bb=l_oper->get_num_operand()==4?dynamic_cast<BasicBlock*>(l_oper->get_operand(3)):NULL;
        BasicBlock* rhs_right_bb=r_oper->get_num_operand()==4?dynamic_cast<BasicBlock*>(r_oper->get_operand(3)):NULL;
        if(!(lhs_left_bb==rhs_left_bb&&lhs_right_bb==rhs_right_bb))return nullptr;
        auto op=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_op();
        auto left_bb = dynamic_cast<BasicBlock*>(lhs_left_bb);
        auto lhs_new=BinaryExpression::create(op, std::dynamic_pointer_cast<PhiExpression>(lhs)->get_lhs(), std::dynamic_pointer_cast<PhiExpression>(rhs)->get_lhs());
        Instruction* l_l_op=static_cast<Instruction*>(l_oper->get_operand(0));
        Instruction* r_l_op=static_cast<Instruction*>(r_oper->get_operand(0));
        auto vi=getVN(pout_[left_bb], lhs_new);
        if(vi==nullptr){
            // std::cout<<(lhs_left_bb->get_name())<<std::endl;
            vi=valuePhiFunc(lhs_new, pout_[left_bb], l_l_op, r_l_op);
        }
        if(lhs_right_bb!=NULL){
            auto right_bb = dynamic_cast<BasicBlock*>(lhs_right_bb);
            auto rhs_new=BinaryExpression::create(op, std::dynamic_pointer_cast<PhiExpression>(lhs)->get_rhs(), std::dynamic_pointer_cast<PhiExpression>(rhs)->get_rhs());        
            Instruction* l_r_op=static_cast<Instruction*>(l_oper->get_operand(2));
            Instruction* r_r_op=static_cast<Instruction*>(r_oper->get_operand(2));
            // std::cout<<(l_oper->get_name())<<"+"<<(r_oper->get_name())<<std::endl;
            auto vj=getVN(pout_[right_bb], rhs_new);
            if(vj==nullptr){
                // std::cout<<(lhs_right_bb->get_name())<<std::endl;
                vj=valuePhiFunc(rhs_new, pout_[right_bb], l_r_op, r_r_op);
            }
            if(vi!=nullptr&&vj!=nullptr)
            return PhiExpression::create(vi, vj);
        }
        else if(vi!=nullptr)
            return PhiExpression::create(vi, nullptr);
        else return nullptr;
    }
    return nullptr;
}

shared_ptr<Expression> GVN::getVN(const partitions &pout, shared_ptr<Expression> ve) {
    // TODO: return what?
    auto lhs = std::dynamic_pointer_cast<BinaryExpression>(ve)->get_lhs();
    auto rhs = std::dynamic_pointer_cast<BinaryExpression>(ve)->get_rhs();
    auto op = std::dynamic_pointer_cast<BinaryExpression>(ve)->get_op();
    bool lhs_is_cons = lhs->is_constant();
    bool rhs_is_cons = rhs->is_constant();
    if(lhs_is_cons&&rhs_is_cons){
        auto cons = folder_->compute(op, 
                                    std::dynamic_pointer_cast<ConstantExpression>(lhs)->get_cons(), 
                                    std::dynamic_pointer_cast<ConstantExpression>(rhs)->get_cons());
        return ConstantExpression::create(cons);
    }
    for(auto &CC : pout){
        if(CC->value_expr_==ve)return CC->value_expr_;
    }
    return nullptr;
}

void GVN::initPerFunction() {
    next_value_number_ = 1;
    pin_.clear();
    pout_.clear();
}

void GVN::replace_cc_members() {
    for (auto &[_bb, part] : pout_) {
        auto bb = _bb; // workaround: structured bindings can't be captured in C++17
        for (auto &cc : part) {
            if (cc->index_ == 0)
                continue;
            // if you are planning to do constant propagation, leaders should be set to constant at some point
            for (auto &member : cc->members_) {
                bool member_is_phi = dynamic_cast<PhiInst *>(member);
                bool value_phi = cc->value_phi_ != nullptr;
                if (member != cc->leader_ and (value_phi or !member_is_phi)) {
                    // only replace the members if users are in the same block as bb
                    member->replace_use_with_when(cc->leader_, [bb](User *user) {
                        if (auto instr = dynamic_cast<Instruction *>(user)) {
                            auto parent = instr->get_parent();
                            auto &bb_pre = parent->get_pre_basic_blocks();
                            if (instr->is_phi()) // as copy stmt, the phi belongs to this block
                                return std::find(bb_pre.begin(), bb_pre.end(), bb) != bb_pre.end();
                            else
                                return parent == bb;
                        }
                        return false;
                    });
                }
            }
        }
    }
    return;
}

// top-level function, done for you
void GVN::run() {
    std::ofstream gvn_json;
    if (dump_json_) {
        gvn_json.open("gvn.json", std::ios::out);
        gvn_json << "[";
    }

    folder_ = std::make_unique<ConstFolder>(m_);
    func_info_ = std::make_unique<FuncInfo>(m_);
    func_info_->run();
    dce_ = std::make_unique<DeadCode>(m_);
    dce_->run(); // let dce take care of some dead phis with undef

    for (auto &f : m_->get_functions()) {
        if (f.get_basic_blocks().empty())
            continue;
        func_ = &f;
        initPerFunction();
        LOG_INFO << "Processing " << f.get_name();
        detectEquivalences();
        LOG_INFO << "===============pin=========================\n";
        for (auto &[bb, part] : pin_) {
            LOG_INFO << "\n===============bb: " << bb->get_name() << "=========================\npartitionIn: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        LOG_INFO << "\n===============pout=========================\n";
        for (auto &[bb, part] : pout_) {
            LOG_INFO << "\n=====bb: " << bb->get_name() << "=====\npartitionOut: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        if (dump_json_) {
            gvn_json << "{\n\"function\": ";
            gvn_json << "\"" << f.get_name() << "\", ";
            gvn_json << "\n\"pout\": " << utils::dump_bb2partition(pout_);
            gvn_json << "},";
        }
        replace_cc_members(); // don't delete instructions, just replace them
    }
    dce_->run(); // let dce do that for us
    if (dump_json_)
        gvn_json << "]";
}

template <typename T>
static bool equiv_as(const Expression &lhs, const Expression &rhs) {
    // we use static_cast because we are very sure that both operands are actually T, not other types.
    return static_cast<const T *>(&lhs)->equiv(static_cast<const T *>(&rhs));
}

bool GVNExpression::operator==(const Expression &lhs, const Expression &rhs) {
    if (lhs.get_expr_type() != rhs.get_expr_type())
        return false;
    switch (lhs.get_expr_type()) {
    case Expression::e_constant: return equiv_as<ConstantExpression>(lhs, rhs);
    case Expression::e_bin: return equiv_as<BinaryExpression>(lhs, rhs);
    case Expression::e_phi: return equiv_as<PhiExpression>(lhs, rhs);
    case Expression::e_var: return equiv_as<VarExpression>(lhs, rhs);
    case Expression::e_call: return equiv_as<CallExpression>(lhs, rhs);
    case Expression::e_gep: return equiv_as<GepExpression>(lhs, rhs);
    case Expression::e_cmp: return equiv_as<CmpExpression>(lhs, rhs);
    case Expression::e_fcmp: return equiv_as<FCmpExpression>(lhs, rhs);
    case Expression::e_single: return equiv_as<SingleExpression>(lhs, rhs);
    }
}

bool GVNExpression::operator==(const shared_ptr<Expression> &lhs, const shared_ptr<Expression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<SingleExpression> &lhs, const std::shared_ptr<SingleExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<FCmpExpression> &lhs, const std::shared_ptr<FCmpExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<CmpExpression> &lhs, const std::shared_ptr<CmpExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<GepExpression> &lhs, const std::shared_ptr<GepExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<CallExpression> &lhs, const std::shared_ptr<CallExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<VarExpression> &lhs, const std::shared_ptr<VarExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<ConstantExpression> &lhs, const std::shared_ptr<ConstantExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<BinaryExpression> &lhs, const std::shared_ptr<BinaryExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
bool GVNExpression::operator==(const std::shared_ptr<PhiExpression> &lhs, const std::shared_ptr<PhiExpression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}
GVN::partitions GVN::clone(const partitions &p) {
    partitions data;
    for (auto &cc : p) {
        data.insert(std::make_shared<CongruenceClass>(*cc));
    }
    return data;
}

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2) {
    // TODO: how to compare partitions?
    if(p1.size()!=p2.size())return false;
    else{
        std::set<std::shared_ptr<CongruenceClass>>::iterator i=p1.begin();
        std::set<std::shared_ptr<CongruenceClass>>::iterator j=p2.begin();
        for(; i!=p1.end(); i++,j++){
            if(**i==**j)continue;
            else if(**i==**j) return false;
            else return false;
        }
    }
    return true;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const {
    // TODO: which fields need to be compared?
    if(this->members_.size()!=other.members_.size())return false;
    if(!(this->leader_==other.leader_))return false;
    for(auto &mem : other.members_){
        if(this->members_.count(mem))continue;
        else return false;
    }
    return true;
}
