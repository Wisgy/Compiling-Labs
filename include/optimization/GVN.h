#pragma once
#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"
#include "PassManager.hpp"
#include "Value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GVNExpression {

// fold the constant value
class ConstFolder {
  public:
    ConstFolder(Module *m) : module_(m) {}
    Constant *compute(Instruction *instr, Constant *value1, Constant *value2);
    Constant *compute(Instruction *instr, Constant *value1);
  private:
    Module *module_;
};

/**
 * for constructor of class derived from `Expression`, we make it public
 * because `std::make_shared` needs the constructor to be publicly available,
 * but you should call the static factory method `create` instead the constructor itself to get the desired data
 */
class Expression {
  public:
    // TODO: you need to extend expression types according to testcases
    enum gvn_expr_t { e_constant, e_bin, e_phi, e_var, e_call, e_gep, e_cmp, e_fcmp, e_single};
    Expression(gvn_expr_t t) : expr_type(t) {}
    virtual ~Expression() = default;
    virtual std::string print() = 0;
    gvn_expr_t get_expr_type() const { return expr_type; }
    bool is_constant() const { return get_expr_type() == e_constant;}
    bool is_binary() const { return get_expr_type() == e_bin;}
    bool is_phi() const { return get_expr_type() == e_phi;}
    bool is_var() const { return get_expr_type() == e_var;}
    bool is_call() const { return get_expr_type() == e_call;}
    bool is_gep() const { return get_expr_type() == e_gep;}
    bool is_cmp() const { return get_expr_type() == e_cmp;}
    bool is_fcmp() const { return get_expr_type() == e_fcmp;}
    bool is_single() const { return get_expr_type() == e_single;}
  private:
    gvn_expr_t expr_type;
};

bool operator==(const std::shared_ptr<Expression> &lhs, const std::shared_ptr<Expression> &rhs);
bool operator==(const GVNExpression::Expression &lhs, const GVNExpression::Expression &rhs);

class SingleExpression : public Expression {
  public:
    static std::shared_ptr<SingleExpression> create(Instruction::OpID op, std::shared_ptr<Expression> oper) {
        return std::make_shared<SingleExpression>(op, oper);
    }
    virtual std::string print() { return "single_operand"; }
    bool equiv(const SingleExpression *other) const {
        if (op_ == other->op_ and *oper_ == *other->get_oper())
            return true;
        else
            return false;
    }
    Instruction::OpID get_op() const {return op_;}
    std::shared_ptr<Expression> get_oper() const {return oper_;}
    SingleExpression(Instruction::OpID op, std::shared_ptr<Expression> oper)
        : Expression(e_single), op_(op), oper_(oper) {}

  private:
    Instruction::OpID op_;
    std::shared_ptr<Expression> oper_;
};
class FCmpExpression : public Expression {
  public:
    static std::shared_ptr<FCmpExpression> create(FCmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs) {
        return std::make_shared<FCmpExpression>(op, lhs, rhs);
    }
    virtual std::string print() { return "FCmp"; }
    bool equiv(const FCmpExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    FCmpInst::CmpOp get_op() const {return op_;}
    std::shared_ptr<Expression> get_lhs() const {return lhs_;}
    std::shared_ptr<Expression> get_rhs() const {return rhs_;}
    FCmpExpression(FCmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_fcmp), op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    FCmpInst::CmpOp op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};
class CmpExpression : public Expression {
  public:
    static std::shared_ptr<CmpExpression> create(CmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs) {
        return std::make_shared<CmpExpression>(op, lhs, rhs);
    }
    virtual std::string print() { return "Cmp"; }
    bool equiv(const CmpExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    CmpInst::CmpOp get_op() const {return op_;}
    std::shared_ptr<Expression> get_lhs() const {return lhs_;}
    std::shared_ptr<Expression> get_rhs() const {return rhs_;}
    CmpExpression(CmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_cmp), op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    CmpInst::CmpOp op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};
class GepExpression : public Expression {
  public:
    static std::shared_ptr<GepExpression> create() { return std::make_shared<GepExpression>(); }
    virtual std::string print() { return "getelementptr"; }
    std::vector<std::shared_ptr<Expression>> get_args() const {return args;}
    void arg_insert(std::shared_ptr<Expression> arg) {args.push_back(arg);}
    bool equiv(const GepExpression *other) const {
        if(args.size()!=other->get_args().size())return false;
        auto lhs = args.begin();
        auto args2 = other->get_args();
        auto rhs = args2.begin();
        for(;lhs!=args.end();lhs++,rhs++){
          if(*lhs==*rhs)continue;
          else return false;
        }
        return true;
    }
    GepExpression() : Expression(e_gep) {}
  private:
    std::vector<std::shared_ptr<Expression>> args;
};

class CallExpression : public Expression {
  public:
    static std::shared_ptr<CallExpression> create(Function* call) { return std::make_shared<CallExpression>(call); }
    static std::shared_ptr<CallExpression> create(Instruction* ins) { return std::make_shared<CallExpression>(ins); }
    virtual std::string print() { return func==nullptr?"no_pure_func":func->get_name(); }
    Function* get_func() const {return func;}
    Instruction* get_instr() const {return instr;}
    std::vector<std::shared_ptr<Expression>> get_args() const {return args;}
    void arg_insert(std::shared_ptr<Expression> arg) {args.push_back(arg);};
    bool equiv(const CallExpression *other) const {
        if(instr!=nullptr)
            if(instr==other->get_instr())return true;
            else return false;
        else if (func==other->get_func()&&args.size()==other->get_args().size()){
            auto lhs = args.begin();
            auto args2 = other->get_args();
            auto rhs = args2.begin();
            for(;lhs!=args.end();lhs++,rhs++){
              if(*lhs==*rhs)continue;
              else return false;
            }
            return true;
        }
        else
            return false;
    }
    CallExpression(Function* call) : Expression(e_call), func(call), instr(nullptr) {}
    CallExpression(Instruction* ins) : Expression(e_call), func(nullptr), instr(ins) {}
    
  private:
    Function* func;
    Instruction* instr;
    std::vector<std::shared_ptr<Expression>> args;
};
class VarExpression : public Expression {
  public:
    static std::shared_ptr<VarExpression> create(Value* ins) { return std::make_shared<VarExpression>(ins); }
    virtual std::string print() { return "VarExpr"; }
    Value* get_instr() const { return instr;}
    bool equiv(const VarExpression *other) const {
        if (instr==other->get_instr()&&instr!=nullptr&&other->get_instr()!=nullptr)
        // if (instr==other->get_instr())
            return true;
        else
            return false;
    }
    VarExpression(Value* ins) : Expression(e_var), instr(ins) {}
    
  private:
    Value* instr;
};

class ConstantExpression : public Expression {
  public:
    static std::shared_ptr<ConstantExpression> create(Constant *c) { return std::make_shared<ConstantExpression>(c); }
    virtual std::string print() { return c_->print(); }
    // we leverage the fact that constants in lightIR have unique addresses
    Constant* get_cons() const { return c_; }
    bool equiv(const ConstantExpression *other) const { return c_ == other->c_; }
    ConstantExpression(Constant *c) : Expression(e_constant), c_(c) {}

  private:
    Constant *c_;
};

// arithmetic expression
class BinaryExpression : public Expression {
  public:
    static std::shared_ptr<BinaryExpression> create(Instruction::OpID op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<BinaryExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(" + Instruction::get_instr_op_name(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const BinaryExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    Instruction::OpID get_op() const {return op_;}
    std::shared_ptr<Expression> get_lhs() const {return lhs_;}
    std::shared_ptr<Expression> get_rhs() const {return rhs_;}
    BinaryExpression(Instruction::OpID op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_bin), op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    Instruction::OpID op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};

class PhiExpression : public Expression {
  public:
    static std::shared_ptr<PhiExpression> create(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs) {
        return std::make_shared<PhiExpression>(lhs, rhs);
    }
    virtual std::string print() { return "(phi " + lhs_->print() + " " + rhs_->print() + ")"; }
    bool equiv(const PhiExpression *other) const {
        if (*lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    std::shared_ptr<Expression> get_lhs() const {return lhs_;}
    std::shared_ptr<Expression> get_rhs() const {return rhs_;}
    PhiExpression(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_phi), lhs_(lhs), rhs_(rhs) {}

  private:
    std::shared_ptr<Expression> lhs_, rhs_;
};
bool operator==(const std::shared_ptr<SingleExpression> &lhs, const std::shared_ptr<SingleExpression> &rhs);
bool operator==(const std::shared_ptr<FCmpExpression> &lhs, const std::shared_ptr<FCmpExpression> &rhs);
bool operator==(const std::shared_ptr<CmpExpression> &lhs, const std::shared_ptr<CmpExpression> &rhs);
bool operator==(const std::shared_ptr<GepExpression> &lhs, const std::shared_ptr<GepExpression> &rhs);
bool operator==(const std::shared_ptr<CallExpression> &lhs, const std::shared_ptr<CallExpression> &rhs);
bool operator==(const std::shared_ptr<VarExpression> &lhs, const std::shared_ptr<VarExpression> &rhs);
bool operator==(const std::shared_ptr<ConstantExpression> &lhs, const std::shared_ptr<ConstantExpression> &rhs);
bool operator==(const std::shared_ptr<BinaryExpression> &lhs, const std::shared_ptr<BinaryExpression> &rhs);
bool operator==(const std::shared_ptr<PhiExpression> &lhs, const std::shared_ptr<PhiExpression> &rhs);
} // namespace GVNExpression

/**
 * Congruence class in each partitions
 * note: for constant propagation, you might need to add other fields
 * and for load/store redundancy detection, you most certainly need to modify the class
 */
struct CongruenceClass {
    size_t index_;
    // representative of the congruence class, used to replace all the members (except itself) when analysis is done
    Value *leader_;
    // value expression in congruence class
    std::shared_ptr<GVNExpression::Expression> value_expr_;
    // value φ-function is an annotation of the congruence class
    std::shared_ptr<GVNExpression::PhiExpression> value_phi_;
    // equivalent variables in one congruence class
    std::set<Value *> members_;

    CongruenceClass(size_t index) : index_(index), leader_{}, value_expr_{}, value_phi_{}, members_{} {}

    bool operator<(const CongruenceClass &other) const { return this->index_ < other.index_; }
    bool operator==(const CongruenceClass &other) const;
};
bool operator==(const std::shared_ptr<CongruenceClass> &lhs, const std::shared_ptr<CongruenceClass> &rhs);
namespace std {
template <>
// overload std::less for std::shared_ptr<CongruenceClass>, i.e. how to sort the congruence classes
struct less<std::shared_ptr<CongruenceClass>> {
    bool operator()(const std::shared_ptr<CongruenceClass> &a, const std::shared_ptr<CongruenceClass> &b) const {
        // nullptrs should never appear in partitions, so we just dereference it
        return *a < *b;
    }
};
} // namespace std

class GVN : public Pass {
  public:
    using partitions = std::set<std::shared_ptr<CongruenceClass>>;
    GVN(Module *m, bool dump_json) : Pass(m), dump_json_(dump_json) {}
    // pass start
    void run() override;
    // init for pass metadata;
    void initPerFunction();

    // fill the following functions according to Pseudocode, **you might need to add more arguments**
    void detectEquivalences();
    partitions join(const partitions &P1, const partitions &P2);
    std::shared_ptr<CongruenceClass> intersect(std::shared_ptr<CongruenceClass>, std::shared_ptr<CongruenceClass>);
    partitions transferFunction(Instruction *x, partitions pin);
    std::shared_ptr<GVNExpression::PhiExpression> valuePhiFunc(std::shared_ptr<GVNExpression::Expression>,
                                                               const partitions &,
                                                               Instruction*);
    std::shared_ptr<GVNExpression::Expression> valueExpr(Instruction *instr, partitions &pin);
    std::shared_ptr<GVNExpression::Expression> getVN(const partitions &pout,
                                                     std::shared_ptr<GVNExpression::Expression> ve,
                                                     Instruction* x);

    // replace cc members with leader
    void replace_cc_members();

    // note: be careful when to use copy constructor or clone
    partitions clone(const partitions &p);

    // create congruence class helper
    std::shared_ptr<CongruenceClass> createCongruenceClass(size_t index = 0) {
        return std::make_shared<CongruenceClass>(index);
    }

  private:
    bool dump_json_;
    std::uint64_t next_value_number_ = 1;
    Function *func_;
    std::map<BasicBlock *, partitions> pin_, pout_;
    std::unique_ptr<FuncInfo> func_info_;
    std::unique_ptr<GVNExpression::ConstFolder> folder_;
    std::unique_ptr<DeadCode> dce_;
};

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2);
