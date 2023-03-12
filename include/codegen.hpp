#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "Module.h"
#include "logging.hpp"

using std::string;
using std::vector;
using std::set;


// register
class Reg {
  public:
    Reg(int index) : id(index) {}
    virtual ~Reg() = default;

    int id;
    Value* value = NULL;
    virtual string print() {
        if (id == 0)
            return "$zero";
        if (id == 1)
            return "$ra";
        if (id == 2)
            return "$tp";
        if (id == 3)
            return "$sp";
        if (4 <= id and id <= 11)
            return "$a" + std::to_string(id - 4);
        if (12 <= id and id <= 20)
            return "$t" + std::to_string(id - 12);
        if (id == 22)
            return "$fp";
        if (23 <= id and id <= 31)
            return "$s" + std::to_string(id - 23);
        assert(false);
    }
};
class FReg : public Reg{
    public:
        FReg(int index) : Reg(index) {}
        string print() override{
            if (0 <= id and id <= 7)
                return "$fa" + std::to_string(id);
            if (8 <= id and id <= 23)
                return "$ft" + std::to_string(id-8);
            assert(false);
        }
};

class CodeGen {
  public:
    CodeGen(Module *module) : m(module) {}

    string print() {
        string result;
        for (auto line : output) {
            if (line.find(":") == string::npos and line != "")
                result += "\t"; // 添加缩进
            result += line + "\n";
        }
        return result;
    }

    void run();
    void visit(BasicBlock* bb);
    void bb_end_store(BasicBlock* bb);
    void ret_assembly(Instruction* instr);
    void br_assembly(Instruction* instr);
    void binary_assembly(Instruction* instr);
    void alloca_assembly(Instruction* instr);
    void load_assembly(Instruction* instr);
    void store_assembly(Instruction* instr);
    void cmp_assembly(Instruction* instr, Instruction* refer);
    void fcmp_assembly(Instruction* instr, Instruction* refer);
    void phi_assembly(Instruction* instr);
    void call_assembly(Instruction* instr);
    void getelementptr_assembly(Instruction* instr);
    void zext_assembly(Instruction* instr);
    void fptosi_assembly(Instruction* instr);
    void sitofp_assembly(Instruction* instr);
    Reg* AllocaReg(Value* v);
    Reg* AllocaTmpReg();
    Reg* AllocaTmpFReg();
    Reg* GetReg(Value* value);
    int LoadArgs(Function* func);

    void CFopt(Function* func);
    void gen_code(string assem);
    void gen_code(vector<string>& assem);

    void join(BasicBlock*,BasicBlock*, BasicBlock*);
    void RegDescUpdate(BasicBlock*);
    void RegFlowAnalysis(Function*);
    bool is_inerited(Value* val, BasicBlock* bb);
    Reg* get_inerited_reg(Value* val, BasicBlock* bb);
    void DefineStore(Value*, Reg*);

  private:
    Module *m;
    vector<string> output;
};

class AddrDesc{
    public:
        AddrDesc() = default;
        AddrDesc(Value* value) : val_(value) {}
        AddrDesc(Value* value, Reg* reg) : val_(value) {reg_ = reg;}
        Reg* get_reg() const { return reg_; }
        int get_off() const { return off_; }
        void set_reg(Reg* reg) {reg_ = reg;}
        void set_off(int offset) {off_ = offset;}

    private:
        Value* val_ ;
        Reg* reg_ ;
        int off_=-1;
};
Reg* LinearScanR();
Reg* LinearScanFR();
Reg* RandomReg();
Reg* RandomFReg();
void UpdateReg(Reg* r, Value* val, bool set_null = true);



#endif
