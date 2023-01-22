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

class Reg {
  public:
    Reg(int index) : id(index) {}

    int id;

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
};

void CodeGen::run() {
    // TODO: implement
    // 以下内容生成 int main() { return 0; } 的汇编代码

    output.push_back(".text");
    for (auto &func : m->get_functions())
        if (not func.is_declaration()) {
            output.push_back(".globl " + func.get_name());
            output.push_back(".type " + func.get_name() + ", @function");
            output.push_back(func.get_name() + ":");
            output.push_back("addi.d $sp, $sp, -16");
            output.push_back("st.d $ra, $sp, 8");
            output.push_back("addi.d $fp, $sp, 16");

            output.push_back("addi.w $a0, $zero, 0");
            output.push_back("ld.d $ra, $sp, 8");
            output.push_back("addi.d $sp, $sp, 16");
            output.push_back("jr $ra");
        }
}
