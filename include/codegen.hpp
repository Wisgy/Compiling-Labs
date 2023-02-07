#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "Module.h"
#include "logging.hpp"

using std::string;
using std::vector;
using std::set;

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
        assert(false);
    }
    bool operator<(Reg& other){
        return this->id < other.id;
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
        bool operator<(FReg& other){
            return this->id < other.id;
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
    // void add_assembly(Instruction* instr);
    // void sub_assembly(Instruction* instr);
    // void mul_assembly(Instruction* instr);
    // void sdiv_assembly(Instruction* instr);
    // void fadd_assembly(Instruction* instr);
    // void fsub_assembly(Instruction* instr);
    // void fmul_assembly(Instruction* instr);
    // void fdiv_assembly(Instruction* instr);
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
    Reg* GetReg(Value* value);

  private:
    Module *m;
    vector<string> output;
};

class AddrDesc{
    public:
        AddrDesc() = default;
        AddrDesc(Value* value) : val_(value) {}
        AddrDesc(Value* value, Reg* reg) : val_(value) {reg_ = reg;}
        // Reg* get_reg() { return reg_.size()?*reg_.begin():NULL;}
        Reg* get_reg() const { return reg_; }
        // Value* get_pos() { return pos_.size()?*pos_.begin():NULL;}
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
void SetReg(Reg*, Value* val);


#endif
