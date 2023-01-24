#include "codegen.hpp"

// $r0          $zero       constant 0
// $r1          $ra         return address
// $r2          $tp         thread pointer
// $r3          $sp         stack pointer
// $r4 - $r5    $a0 - $a1   argument, return value
// $r6 - $r11   $a2 - $a7   argument
// $r12 - $r20  $t0 - $t8   temporary
// $r21                     saved
// $r22         $fp         frame pointer
// $r23 - $r31  $s0 - $s8   static

int next_reg=0;
std::map<Instruction*, AddrDesc> InstrPos;
Reg R[32]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
void CodeGen::run() {
    // TODO: implement
    // 以下内容生成 int main() { return 0; } 的汇编代码

    output.push_back(".text");
    // global_value
    for (auto& g_v : m->get_global_variable()){
        // TODO: global_value
        output.push_back(".comm " + g_v.get_name() + ", 4");
    }
    for (auto &func : m->get_functions())
        if (not func.is_declaration()) {
            // entry
            output.push_back(".globl " + func.get_name());
            output.push_back(".type " + func.get_name() + ", @function");
            output.push_back(func.get_name() + ":");
            output.push_back("addi.d $sp, $sp, -16");
            output.push_back("st.d $ra, $sp, 8");
            output.push_back("addi.d $fp, $sp, 16");
            // body
            for(auto& bb : func.get_basic_blocks()){
                visit(&bb);
            }
            // return
            output.push_back("ld.d $ra, $sp, 8");
            output.push_back("addi.d $sp, $sp, 16");
            output.push_back("jr $ra");
        }
}

void CodeGen::visit(BasicBlock* bb){
    output.push_back(bb->get_name() + ":");
    for(auto& instr : bb->get_instructions()){
        switch (instr.get_instr_type()){
            case Instruction::ret: ret_assembly(&instr); break;
            // case Instruction::br: return "br"; break;
            // case Instruction::add: return "add"; break;
            // case Instruction::sub: return "sub"; break;
            // case Instruction::mul: return "mul"; break;
            // case Instruction::sdiv: return "sdiv"; break;
            // case Instruction::fadd: return "fadd"; break;
            // case Instruction::fsub: return "fsub"; break;
            // case Instruction::fmul: return "fmul"; break;
            // case Instruction::fdiv: return "fdiv"; break;
            // case Instruction::alloca: return "alloca"; break;
            // case Instruction::load: return "load"; break;
            // case Instruction::store: return "store"; break;
            // case Instruction::cmp: return "cmp"; break;
            // case Instruction::fcmp: return "fcmp"; break;
            // case Instruction::phi: return "phi"; break;
            // case Instruction::call: return "call"; break;
            // case Instruction::getelementptr: return "getelementptr"; break;
            // case Instruction::zext: return "zext"; break;
            // case Instruction::fptosi: return "fptosi"; break;
            // case Instruction::sitofp: return "sitofp"; break;
            default:
                assert(false);
        }
    }
}
Reg* CodeGen::GetReg(Instruction* instr){
    // if(InstrPos.find(instr)!=InstrPos.end())
    //     if(InstrPos[instr].get_reg()!=NULL)//存在寄存器含有此变量值
    //         return InstrPos[instr].get_reg();
    //     else if(InstrPos[instr].get_pos()!=NULL){//变量值存储在memory中
    //         auto addr_reg = GetReg(static_cast<Instruction*> (InstrPos[instr].get_pos()));
    //         // output.push_back("ld.w " + )
    //     }
    return RandomReg();

}
Reg* RandomReg(){
    next_reg = (next_reg+1)%9;
    return &R[next_reg+12];
}
void CodeGen::ret_assembly(Instruction* instr){
    auto reg = GetReg(instr);
    output.push_back("addi.w $a0, " + reg->print() + ", 0");
}
