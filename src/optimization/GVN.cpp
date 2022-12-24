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
        json += "\"" + bb->get_name() + "\": " + dump_partition_json(p) + ",";
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
    if(P1==P2)std::cout<<"Really surprise your mother fucker!"<<std::endl;
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
    if(Ci==Cj)return Ci;
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

    if(Ck->value_phi_==nullptr&&Ck->members_.size()==0){//if Ck == {} 
        // delete Ck;
        return nullptr;
    }
    else if(Ck->value_expr_==nullptr){//Ck!={}, and Ck does not have value number
        Ck->value_phi_=PhiExpression::create(Ci->value_expr_, Cj->value_expr_);
        Ck->value_expr_=Ck->value_phi_;
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
    do {
        changed = false;
        partitions pout;
        // see the pseudo code in documentation
        for (auto &bb1 : func_->get_basic_blocks()) { // you might need to visit the blocks in depth-first order
            auto bb=&bb1;
            // get PIN of bb by predecessor(s)
            auto pre_bb = bb->get_pre_basic_blocks();
            // iterate through all instructions in the block
            // and the phi instruction in all the successors
            auto pre_bb1 = pre_bb.begin();
            auto pre_bb2 = pre_bb.begin();
            std::cout<<pre_bb.size()<<std::endl;
            if(pre_bb.size()==2){
                pre_bb2++;
                pout = join(pout_[*pre_bb1], pout_[*pre_bb2]);
            }
            else if(pre_bb.size()==1){
                pout = pout_[*pre_bb1];
                utils::print_partitions(pout);
            }
            else{
                pout = clone(top);
            }
            for(auto &instr : bb->get_instructions()){
                if(instr.is_phi()||instr.is_br()||instr.is_ret())continue;
                pout = transferFunction(&instr,pout);
            }
            //add copy statement
            auto succ_bb = bb->get_succ_basic_blocks();
            for(auto &bb : succ_bb){
                for(auto &ins : bb->get_instructions()){
                    auto instr = &ins;
                    if(instr->is_phi()){
                        for(auto &CC:pout){
                            if(CC->members_.count(instr->get_operand(0))||CC->members_.count(instr->get_operand(2))){
                                CC->members_.insert(instr);
                            }
                        }
                    }
                    else break;
                }
            }
            // check changes in pout
            if(!(pout_[bb] == pout)){
                pout_[bb] = pout;
                changed = true;
            }   
        }
    } while (changed);
}

shared_ptr<Expression> GVN::valueExpr(Instruction *instr, partitions& pin) {
    // TODO:
    // std::shared_ptr<Expression> expr = std::make_shared<Expression>();
    auto op=instr->get_instr_type();
    auto op_num=instr->get_num_operand();
    std::shared_ptr<Expression> lhs,rhs;
    bool flag=true;
    std::cout<<"op_num="<<op_num<<std::endl;
    if(instr->is_call()){
        if(func_info_->is_pure_function(static_cast<Function*>(instr->get_operand(0)))){
            for(auto &C : pin){
                for(int op=1;op<op_num;op++){
                    if(C->members_.count(instr->get_operand(op))){
                        // return C->value_expr_;
                        continue;
                    }
                }  
            }
        }
        else return VarExpression::create(next_value_number_);
    }
    else if(op_num==4&&instr->is_phi()){
        lhs=valueExpr(dynamic_cast<Instruction *>(instr->get_operand(0)), pin);
        rhs=valueExpr(dynamic_cast<Instruction *>(instr->get_operand(2)), pin);
        return PhiExpression::create(lhs, rhs);
    }
    else if(op_num==2){
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
                lhs=valueExpr(dynamic_cast<Instruction *>(instr->get_operand(0)), pin);
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
                rhs=valueExpr(dynamic_cast<Instruction *>(instr->get_operand(1)), pin);
        }
        if(instr->isBinary())
            return BinaryExpression::create(op, lhs, rhs);
        else 
            return BinaryExpression::create(op, lhs, rhs);
        
    }
    return nullptr;
}

// instruction of the form `x = e`, mostly x is just e (SSA), but for copy stmt x is a phi instruction in the
// successor. Phi values (not copy stmt) should be handled in detectEquiv
/// \param bb basic block in which the transfer function is called
GVN::partitions GVN::transferFunction(Instruction *x, partitions pin) {
    partitions pout = clone(pin);
    // TODO: get different ValueExpr by Instruction::OpID, modify pout
    //if x is in a class Ci in POUTs    
    for(auto &Ci : pout){
        if(Ci->members_.count(x)){
            Ci->members_.erase(x);
        }
    }
    //ve = valueExpr(e)
    auto ve = valueExpr(x, pin);
    //vpf = valuePhiFunc(ve,PINs)
    auto vpf = valuePhiFunc(ve, pin);
    //if ve or vpf is in a class Ci in POUTs
    bool flag = true;
    if((*pin.begin())->index_==0){
        pout.clear();
        std::shared_ptr<CongruenceClass> Ck = std::make_shared<CongruenceClass>(0);
        Ck->index_=next_value_number_++;
        Ck->leader_=x;
        Ck->value_expr_=ve;
        Ck->value_phi_=vpf;
        Ck->members_.insert(x);
        pout.insert(Ck);
    }
    else{
        for(auto &Ci : pout){
            
            if(Ci->value_expr_==ve || (vpf!=nullptr&&Ci->value_phi_==vpf)){
                Ci->members_.insert(x);
                // Ci->value_expr_=ve;
                std::cout<<"must"<<std::endl;
                flag=false;
                break;
            }
        }
        if(flag){
            std::shared_ptr<CongruenceClass> Ck = std::make_shared<CongruenceClass>(0);
            Ck->index_=next_value_number_++;
            Ck->leader_=x;
            Ck->value_expr_=ve;
            Ck->value_phi_=vpf;
            Ck->members_.insert(x);
            pout.insert(Ck);
        }
    }
    return pout;
}

shared_ptr<PhiExpression> GVN::valuePhiFunc(shared_ptr<Expression> ve, const partitions &P) {
    // TODO:
    if(ve==nullptr)return nullptr;
    bool flag=false;
    if(ve->is_binary()){
        bool lhs_is_phi=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_lhs()->is_phi();
        bool rhs_is_phi=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_rhs()->is_phi();
        if(lhs_is_phi&&rhs_is_phi)flag=true;
    }
    if(flag){
        auto ve_lhs=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_lhs();
        auto ve_rhs=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_rhs();
        auto ve_op=std::dynamic_pointer_cast<BinaryExpression>(ve)->get_op();
        auto ve_new_l=BinaryExpression::create(ve_op, std::dynamic_pointer_cast<PhiExpression>(ve_lhs)->get_lhs(), std::dynamic_pointer_cast<PhiExpression>(ve_rhs)->get_lhs());
        auto ve_new_r=BinaryExpression::create(ve_op, std::dynamic_pointer_cast<PhiExpression>(ve_lhs)->get_rhs(), std::dynamic_pointer_cast<PhiExpression>(ve_rhs)->get_rhs());
        auto vi=getVN(P, ve_new_l);
        if(vi==nullptr)
            vi=valuePhiFunc(ve_new_l, P);   
        auto vj=getVN(P, ve_new_r);
        if(vj==nullptr)
            vj=valuePhiFunc(ve_new_r, P);
        if(vi!=nullptr&&vj!=nullptr)
            return PhiExpression::create(vi, vj);
    }
    return nullptr;
}

shared_ptr<Expression> GVN::getVN(const partitions &pout, shared_ptr<Expression> ve) {
    // TODO: return what?
    for(auto &CC : pout){
        if(CC->value_phi_!=nullptr){
            if(CC->value_phi_->get_lhs()==ve)return CC->value_phi_->get_lhs();
            if(CC->value_phi_->get_rhs()==ve)return CC->value_phi_->get_rhs();
        }
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
    }
}

bool GVNExpression::operator==(const shared_ptr<Expression> &lhs, const shared_ptr<Expression> &rhs) {
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
            else return false;
        }
    }
    return true;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const {
    // TODO: which fields need to be compared?
    if(this->members_.size()!=other.members_.size())return false;
    if(this->value_phi_!=other.value_phi_)return false;
    for(auto &mem : other.members_){
        if(this->members_.count(mem))continue;
        else return false;
    }
    return true;
}