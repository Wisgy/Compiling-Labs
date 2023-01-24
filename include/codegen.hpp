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

    int id;
    set<Value*> value;
    string print() {
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
    void ret_assembly(Instruction* instr);
    Reg* GetReg(Instruction* instr);

  private:
    Module *m;
    vector<string> output;
};

class AddrDesc{
    public:
        AddrDesc() = default;
        AddrDesc(Value* value) : val_(value) {}
        AddrDesc(Value* value, Reg* reg) : val_(value) {reg_.insert(reg);}
        set<Reg*>& get_regs() { return reg_;}
        Reg* get_reg() { return reg_.size()?*reg_.begin():NULL;}
        Value* get_pos() { return pos_.size()?*pos_.begin():NULL;}
        void push_reg(Reg* reg) {reg_.insert(reg);}
        void push_addr(Value* addr) {pos_.insert(addr);}

    private:
        Value* val_;
        set<Reg*> reg_ ;
        set<Value*> pos_;
};
Reg* RandomReg();


#endif
