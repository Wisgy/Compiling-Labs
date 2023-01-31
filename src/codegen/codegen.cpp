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
int next_freg=0;

std::map<Value*, AddrDesc> ValPos;
std::map<Value*, int> PtOff;
Function *cur_func = nullptr;
BasicBlock *cur_bb = nullptr;
int offset;
Reg R[32]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
FReg FR[24]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
void CodeGen::run() {
    output.push_back(".text");
    // global_value
    for (auto& g_v : m->get_global_variable()){
        int size=g_v.get_type()->get_size();
        output.push_back(".comm " + g_v.get_name() + ", " + std::to_string(size));
    }
    for (auto &func : m->get_functions())
        if (not func.is_declaration()) {
            cur_func = &func;
            offset = -16;
            // entry
            output.push_back(".globl " + func.get_name());
            output.push_back(".type " + func.get_name() + ", @function");
            output.push_back(func.get_name() + ":");
            int pos = output.size();// record the location of the following instruction
            output.push_back("addi.d $sp, $sp, ");
            output.push_back("st.d $ra, $sp, ");// 假设有st.d $fp, $sp, 0
            output.push_back("addi.d $fp, $sp, ");
            // argument
            // body
            for(auto& bb : func.get_basic_blocks()){
                visit(&bb);
            }
            // calculate the memory used by the function
            int bytes = -offset + 16 + offset%16;
            // revise the beginning instructions 
            output[pos] = output[pos] + std::to_string(-bytes);
            output[pos+1] = output[pos+1] + std::to_string(bytes-8);
            output[pos+2] = output[pos+2] + std::to_string(bytes);
            // return
            output.push_back("." + func.get_name() +"_return :");
            output.push_back("ld.d $ra, $sp, " + std::to_string(bytes-8));
            output.push_back("addi.d $sp, $sp, " + std::to_string(bytes));
            output.push_back("jr $ra");
        }
}

void CodeGen::visit(BasicBlock* bb){
    cur_bb = bb;
    output.push_back("." + cur_func->get_name() + "_" + bb->get_name() + ":");
    for(auto& instr : bb->get_instructions()){
        // std::cout<<instr.get_name()<<std::endl;
        switch (instr.get_instr_type()){
            case Instruction::ret: ret_assembly(&instr); break;
            case Instruction::br: br_assembly(&instr); break;
            case Instruction::add: binary_assembly(&instr); break;
            case Instruction::sub: binary_assembly(&instr); break;
            case Instruction::mul: binary_assembly(&instr); break;
            case Instruction::sdiv: binary_assembly(&instr); break;
            case Instruction::fadd: binary_assembly(&instr); break;
            case Instruction::fsub: binary_assembly(&instr); break;
            case Instruction::fmul: binary_assembly(&instr); break;
            case Instruction::fdiv: binary_assembly(&instr); break;
            // case Instruction::add: add_assembly(&instr); break;
            // case Instruction::sub: sub_assembly(&instr); break;
            // case Instruction::mul: mul_assembly(&instr); break;
            // case Instruction::sdiv: sdiv_assembly(&instr); break;
            // case Instruction::fadd: fadd_assembly(&instr); break;
            // case Instruction::fsub: fsub_assembly(&instr); break;
            // case Instruction::fmul: fmul_assembly(&instr); break;
            // case Instruction::fdiv: fdiv_assembly(&instr); break;
            case Instruction::alloca: alloca_assembly(&instr); break;
            case Instruction::load: load_assembly(&instr); break;
            case Instruction::store: store_assembly(&instr); break;
            case Instruction::cmp: cmp_assembly(&instr); break;
            case Instruction::fcmp: fcmp_assembly(&instr); break;
            case Instruction::phi: break;//活跃变量分析
            case Instruction::call: call_assembly(&instr); break;
            case Instruction::getelementptr: getelementptr_assembly(&instr); break;
            case Instruction::zext: zext_assembly(&instr);break;
            case Instruction::fptosi: fptosi_assembly(&instr); break;
            case Instruction::sitofp: sitofp_assembly(&instr); break;
            default:
                assert(true);
        }
    }
}
void CodeGen::ret_assembly(Instruction* instr){
    bool is_int = cur_func->get_return_type()->is_integer_type();
    bool is_float = cur_func->get_return_type()->is_float_type();
    if(is_int||is_float) {
        Value* v = instr->get_operand(0);
        Reg* Rv = GetReg(v);
        output.push_back((is_int?"or $a0, ":"or $fa0, ") + Rv->print() + ", $r0");
        
    }
    output.push_back("b ." + cur_func->get_name() +"_return");
}
void CodeGen::br_assembly(Instruction* instr){
    // TODO: 
    if(dynamic_cast<BranchInst*>(instr)->is_cond_br()){
        string br1_name = "." + cur_func->get_name() + "_" + instr->get_operand(1)->get_name();
        string br2_name = "." + cur_func->get_name() + "_" + instr->get_operand(2)->get_name();
        output[output.size()-1] = output[output.size()-1] + br1_name;
        auto op = dynamic_cast<CmpInst *>(instr->get_operand(0))?
                    dynamic_cast<CmpInst *>(instr->get_operand(0))->get_cmp_op() : 
                    dynamic_cast<FCmpInst *>(instr->get_operand(0))->get_cmp_op();
        if(op == CmpInst::GE || op == CmpInst::LE || op == FCmpInst::GE || op == FCmpInst::LE)
            output[output.size()-2] = output[output.size()-2] + br1_name;
        output.push_back("b " + br2_name);
    }
    else {
        string br_name = "." + cur_func->get_name() + "_" + instr->get_operand(0)->get_name();
        output.push_back("b " + br_name);
    }
}
void CodeGen::binary_assembly(Instruction* instr){
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    auto l_reg = GetReg(lhs);
    auto r_reg = GetReg(rhs);
    string assem_inst;
    switch (instr->get_instr_type()){
        case Instruction::add: assem_inst = "add.w"; break;
        case Instruction::sub: assem_inst = "sub.w"; break;
        case Instruction::mul: assem_inst = "mul.w"; break;
        case Instruction::sdiv: assem_inst = "div.w"; break;
        case Instruction::fadd: assem_inst = "fadd.s"; break;
        case Instruction::fsub: assem_inst = "fsub.s"; break;
        case Instruction::fmul: assem_inst = "fmul.s"; break;
        case Instruction::fdiv: assem_inst = "fdiv.s"; break;
        default:
            assert(true);
    }
    auto res = AllocaReg(instr);
    output.push_back(assem_inst + " " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
    SetReg(res, instr);
}
void CodeGen::alloca_assembly(Instruction* instr){
    //TODO:根据申请空间的大小计算偏移值
    int size = dynamic_cast<AllocaInst*>(instr)->get_alloca_type()->get_size();
    offset -= size;
    PtOff[instr] = offset;
}
void CodeGen::load_assembly(Instruction* instr){
    auto reg = AllocaReg(instr);// 此处必须是个整型寄存器
    Value* pt = instr->get_operand(0);
    if(dynamic_cast<GlobalVariable*>(pt)){
        auto addr = RandomReg();
        output.push_back("la.local " + addr->print() + ", " + pt->get_name());
        SetReg(addr, nullptr);
        if(instr->get_type()->is_integer_type())
            output.push_back("ld.w " + reg->print() + addr->print() + ", 0");
        else if(instr->get_type()->is_float_type())
            output.push_back("fld.s " + reg->print() + addr->print() + ", 0");
        else assert(false);
    }
    else{
        int off = PtOff[pt];
        string inst_tail;
        string inst_head;
        if(instr->get_type()->is_integer_type()) inst_head = "ld.w ";
        else if(instr->get_type()->is_float_type()) inst_head = "fld.s ";
        else assert(false);
        if(PtOff.find(pt) != PtOff.end()) inst_tail = ", $fp, " + std::to_string(off);
        else if(ValPos.find(pt) != ValPos.end()) {auto addr = GetReg(pt);inst_tail = ", " + addr->print() + ", 0";}
        else assert(false);
        output.push_back(inst_head + reg->print() + inst_tail);
    }
    SetReg(reg, instr);
}
void CodeGen::store_assembly(Instruction* instr){
    Value* data = instr->get_operand(0);
    Value* pt = instr->get_operand(1);
    auto reg = GetReg(data);
    auto type = pt->get_type()->get_pointer_element_type();
    if(dynamic_cast<GlobalVariable*>(pt)){
        auto addr = RandomReg();
        output.push_back("la.local " + addr->print() + ", " + pt->get_name());
        SetReg(addr, nullptr);
        if(type->is_integer_type())
            output.push_back("st.w " + reg->print() + ", " + addr->print() + ", 0");
        else if(type->is_float_type())
            output.push_back("fst.s " + reg->print() + ", " + addr->print() + ", 0");
        else assert(false);
    }
    else{
        int off = PtOff[pt];
        string inst_tail;
        string inst_head;
        if(type->is_integer_type()) inst_head = "st.w ";
        else if(type->is_float_type()) inst_head = "fst.s ";
        else assert(false);
        if(PtOff.find(pt) != PtOff.end()) inst_tail = ", $fp, " + std::to_string(off);
        else if(ValPos.find(pt) != ValPos.end()) {auto addr = GetReg(pt);inst_tail = ", " + addr->print() + ", 0";}
        else assert(false);
        output.push_back(inst_head + reg->print() + inst_tail);
    }
}
void CodeGen::cmp_assembly(Instruction* instr){// 暂时无法分别br_cond和op=cmp可能需要活跃变量分析
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    auto l_reg = GetReg(lhs);
    auto r_reg = GetReg(rhs);
    switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
        case CmpInst::EQ: output.push_back("beq " + l_reg->print() + ", " + r_reg->print() + ", ");break;
        case CmpInst::NE: output.push_back("bne " + l_reg->print() + ", " + r_reg->print() + ", ");break;
        case CmpInst::GT: output.push_back("bge " + l_reg->print() + ", " + r_reg->print() + ", ");break;
        case CmpInst::GE: output.push_back("bge " + l_reg->print() + ", " + r_reg->print() + ", ");
                          output.push_back("beq " + l_reg->print() + ", " + r_reg->print() + ", ");break;
        case CmpInst::LT: output.push_back("blt " + l_reg->print() + ", " + r_reg->print() + ", ");break;
        case CmpInst::LE: output.push_back("blt " + l_reg->print() + ", " + r_reg->print() + ", ");
                          output.push_back("beq " + l_reg->print() + ", " + r_reg->print() + ", ");break;
    }
}
void CodeGen::fcmp_assembly(Instruction* instr){
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    auto l_reg = GetReg(lhs);
    auto r_reg = GetReg(rhs);
    switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
        case FCmpInst::EQ: output.push_back("fcmp.ceq.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
        case FCmpInst::NE: output.push_back("fcmp.cne.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
        case FCmpInst::GT: output.push_back("fcmp.clt.s $fcc0, " + r_reg->print() + ", " + l_reg->print());break;
        case FCmpInst::GE: output.push_back("fcmp.cle.s $fcc0, " + r_reg->print() + ", " + l_reg->print());break;
        case FCmpInst::LT: output.push_back("fcmp.clt.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
        case FCmpInst::LE: output.push_back("fcmp.cle.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
    }
}
void CodeGen::phi_assembly(Instruction* instr){
    auto reg = AllocaReg(instr);
    auto lhs = instr->get_operand(0);
    int off = ValPos[lhs].get_off();
    // 判断整型还是浮点
    if(lhs->get_type()->is_integer_type())
        output.push_back("ld.w " + reg->print() + ", $fp, " + std::to_string(off));
    else if(lhs->get_type()->is_float_type())
        output.push_back("fld.s " + reg->print() + ", $fp, " + std::to_string(off));
    else assert(false);
    SetReg(reg, instr);
}
void CodeGen::call_assembly(Instruction* instr){
    // first store the value of registers
    // second load the arguments (if the number of arguments is more than 8)store the extra arguments in the memory
    // third "bl " function_name
    // fourth restore the value of registers
    assert(false);
}
void CodeGen::getelementptr_assembly(Instruction* instr){
    Value* pt = instr->get_operand(0);
    int arr_off = dynamic_cast<ConstantInt*>(instr->get_operand(1))->get_value();
    int elem_off = dynamic_cast<ConstantInt*>(instr->get_operand(2))->get_value();
    int arr_size = pt->get_type()->get_size();
    int elem_size = pt->get_type()->get_pointer_element_type()->get_size();
    int total_off = arr_off*arr_size + elem_off*elem_size;
    auto type = pt->get_type()->get_pointer_element_type();
    auto addr = AllocaReg(instr);
    if(dynamic_cast<GlobalVariable*>(pt)){
        auto addr = RandomReg();
        output.push_back("la.local " + addr->print() + ", " + pt->get_name());
        output.push_back("addi.w " + addr->print() + ", " + addr->print() + ", " + std::to_string(total_off));
        SetReg(addr, instr);
    }
    else{
        assert(PtOff.find(pt) != PtOff.end());
        int off = PtOff[pt] + total_off;
        PtOff[instr] = off;
    }
}
void CodeGen::zext_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    output.push_back("or " + res->print() + ", " + reg->print() + "$zero");
    SetReg(res, instr);
}
void CodeGen::fptosi_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    output.push_back("movfr2gr.s " + res->print() + ", " + reg->print());
}
void CodeGen::sitofp_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    output.push_back("movgr2fr.w " + res->print() + ", " + reg->print());
}
Reg* CodeGen::GetReg(Value* v) {// 一定会返回存有此值的寄存器
    if(ValPos.find(v)!=ValPos.end()&&ValPos[v].get_reg()!=nullptr) {// TODO:如果寄存器存在此变量的值
        return ValPos[v].get_reg();
    }
    else{
        if(dynamic_cast<Constant*>(v)) {// 常量
            if(dynamic_cast<ConstantInt*>(v)){// 整型常量
                int val = dynamic_cast<ConstantInt*>(v)->get_value();
                if(val==0)return &R[0];
                else if(val>0&&val<=4095){// 可直接加载的小立即数
                    auto reg = AllocaReg(v);
                    output.push_back("ori " + reg->print() + ", $zero, " + std::to_string(val));
                    SetReg(reg, v);
                    return reg;
                }
                else if(val>=-2048&&val<0){
                    auto reg = AllocaReg(v);
                    output.push_back("addi.w " + reg->print() + ", $zero, " + std::to_string(val));
                    SetReg(reg, v);
                    return reg;
                }
                else{ // 不可直接加载的大数
                    auto reg = AllocaReg(v);
                    output.push_back("lu12i.w " + reg->print() + ", " + std::to_string(val) + ">>12");
                    output.push_back("ori" + reg->print() + ", " + reg->print() + ", " + std::to_string(val));
                    SetReg(reg, v);
                    return reg;
                }
            }
            else{//TODO:浮点型常量
                assert(false);
            }
        }
        else if(dynamic_cast<GlobalVariable*>(v)) {// 全局变量
            auto reg = AllocaReg(v);
            output.push_back("la.local " + reg->print() + "," + v->get_name());
            SetReg(reg, v);
        }
        else { //TODO:局部变量
            assert(false);
        }
    }
    return RandomReg();

}
Reg* CodeGen::AllocaReg(Value* v){
    if(v->get_type()->is_float_type())return RandomFReg();
    else return RandomReg();
}
Reg* RandomReg(){
    next_reg = (next_reg+1)%9;
    return &R[next_reg+12];
}
Reg* RandomFReg(){
    next_freg = (next_freg+1)%16;
    return &FR[next_freg+8];
}
void SetReg(Reg* r, Value* val){
    if (r->value!=nullptr) ValPos[r->value].set_reg(nullptr);
    r->value = val;
    if (val) ValPos[val].set_reg(r);
}