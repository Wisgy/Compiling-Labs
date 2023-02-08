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
std::map<Value*, int> LocalOff;
Function *cur_func = nullptr;
BasicBlock *cur_bb = nullptr;
Instruction *point = nullptr;
int offset;
int max_arg_size;
Reg R[32]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};// integer register
FReg FR[24]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};// float register
std::map<BasicBlock*, set<Value*>> OUT;// active vars at the exit of the basicblocks
std::map<Value*, set<Value*>> in;
std::map<Value*, set<Value*>> out;
inline std::set<Value*> clone(set<Value*> s){
    return s;
}
void ActiveVars(Function *func){// 根据函数返回对应OUT
    OUT.clear();
    // use[bb],def[bb]
    std::map<BasicBlock*, set<Value*>> use;
    std::map<BasicBlock*, set<Value*>> def;
    for(auto &arg : func->get_args())// arguments
        def[func->get_entry_block()].insert(arg);
    for(auto &bb : func->get_basic_blocks()){
        use[&bb].clear();
        def[&bb].clear();
        for(auto &instr : bb.get_instructions()){
            if(!instr.is_alloca()&&!instr.is_void())
                def[&bb].insert(&instr);
            if(instr.is_phi()){// 对于phi指令要单独处理，否则会导致其中一个前继块出现另一个前继块的的变量
                auto op1 = instr.get_operand(0);
                BasicBlock* bb1 = dynamic_cast<BasicBlock*>(instr.get_operand(1));
                if(!(dynamic_cast<Constant*>(op1)))OUT[bb1].insert(op1);
                if(instr.get_num_operand()==4){
                    auto op2 = instr.get_operand(2);
                    BasicBlock* bb2 = dynamic_cast<BasicBlock*>(instr.get_operand(3));
                    if(!(dynamic_cast<Constant*>(op2)))OUT[bb2].insert(op2);
                }
            }
            for(auto &op : instr.get_operands())
                if(!(dynamic_cast<GlobalVariable*>(op)||dynamic_cast<Constant*>(op)||dynamic_cast<AllocaInst*>(op)||dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)||instr.is_phi()))
                    use[&bb].insert(op);
        }
    }
    // IN,OUT
    std::map<BasicBlock*, set<Value*>> IN;
    for(auto &bb : func->get_basic_blocks())IN[&bb].clear();
    bool changed;
    do{
        changed = false;
        for(auto &bb : func->get_basic_blocks()){
            std::set<Value*> tmp = clone(IN[&bb]);
            for(auto &suc_bb : bb.get_succ_basic_blocks())OUT[&bb].merge(clone(IN[suc_bb]));
            IN[&bb].merge(clone(OUT[&bb]));
            IN[&bb].merge(clone(use[&bb]));
            for(auto inst : def[&bb])IN[&bb].erase(inst);
            if(tmp != IN[&bb]) changed = true;
        }
    }while(changed);
    // in, out
    Instruction* suc_inst, *cur_inst;
    for(auto& bb : func->get_basic_blocks()){
        auto iter = bb.get_instructions().end();
        iter--;
        cur_inst = &(*iter);
        out[cur_inst]=OUT[&bb];
        in[cur_inst]=out[cur_inst];
        in[cur_inst].insert(cur_inst);
        in[cur_inst].erase(suc_inst);
        for(auto op : cur_inst->get_operands())if(!(dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)))in[cur_inst].insert(op);
        suc_inst = cur_inst;
        for(;iter!=bb.get_instructions().begin();iter--){
            cur_inst = &(*iter);
            out[cur_inst] = in[suc_inst];
            in[cur_inst]=out[cur_inst];
            in[cur_inst].insert(cur_inst);
            in[cur_inst].erase(suc_inst);
            for(auto op : cur_inst->get_operands())if(!(dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)))in[cur_inst].insert(op);
            suc_inst = cur_inst;
        }
        cur_inst = &(*iter);
        out[cur_inst] = in[suc_inst];
        in[cur_inst] = out[cur_inst];
        in[cur_inst].insert(cur_inst);
        in[cur_inst].erase(suc_inst);
        for(auto op : cur_inst->get_operands())if(!(dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)))in[cur_inst].insert(op);
    }
    // for(auto &bb : func->get_basic_blocks()){
    //     std::cout<<bb.get_name()<<std::endl;
    //     for(auto &ins : bb.get_instructions()){
    //         auto cur_inst = &ins;
    //         std::cout<<cur_inst->get_name()<<":\n[";
    //         for(auto val : in[cur_inst])
    //         if(dynamic_cast<ConstantInt*>(val))std::cout<<dynamic_cast<ConstantInt*>(val)->get_value()<<",";
    //         else if(dynamic_cast<ConstantFP*>(val))std::cout<<dynamic_cast<ConstantFP*>(val)->get_value()<<",";
    //         else std::cout<<val->get_name()<<", ";
    //         std::cout<<"]"<<std::endl;
    //     }
    // }
    // std::cout<<std::endl;
}
bool is_active(Value* inst, BasicBlock* bb){
    return OUT[bb].find(inst)!=OUT[bb].end();
}
bool is_active(Value* inst, Value* point){// 当前程序点活跃
    return in[point].find(inst)!=in[point].end();
}
bool is_active_out(Value* inst, Value* point){// 当前程序点出口活跃
    return out[point].find(inst)!=out[point].end();
}
void CodeGen::run() {
    output.push_back(".text");
    // global_value
    for (auto& g_v : m->get_global_variable()){
        int size=g_v.get_type()->get_size();
        output.push_back(".comm " + g_v.get_name() + ", " + std::to_string(size));
    }
    for (auto &func : m->get_functions())
        if (not func.is_declaration()) {
            LocalOff.clear();
            ValPos.clear();
            ActiveVars(&func);
            cur_func = &func;
            offset = -16;
            max_arg_size = 0;
            // arguments
            int i,f,off;
            i=f=off=0;
            for(auto& arg : func.get_args()){
                if(arg->get_type()->is_integer_type()){
                    if(i<8)SetReg(&R[(i++)+4], arg);
                    else {
                        ValPos[arg].set_off(off);
                        off += arg->get_type()->get_size();
                    }
                }
                else if(arg->get_type()->is_float_type()){
                    if(f<8)SetReg(&FR[f++], arg);
                    else {
                        ValPos[arg].set_off(off);
                        off += arg->get_type()->get_size();
                    }
                }
                else if(arg->get_type()->is_pointer_type()){
                    if(i<8)SetReg(&R[(i++)+4], arg);
                    else {
                        ValPos[arg].set_off(off);
                        off += arg->get_type()->get_size();
                    }
                }
                else assert(false);
            }
            // entry
            output.push_back(".globl " + func.get_name());
            output.push_back(".type " + func.get_name() + ", @function");
            output.push_back(func.get_name() + ":");
            int pos = output.size();// record the location of the following instruction
            output.push_back("addi.d $sp, $sp, ");
            output.push_back("st.d $ra, $sp, ");
            output.push_back("st.d $fp, $sp, ");
            output.push_back("addi.d $fp, $sp, ");
            // body
            for(auto& bb : func.get_basic_blocks()){
                visit(&bb);
            }
            // calculate the memory used by the function
            offset -= max_arg_size;
            int bytes = -offset + 16 + offset%16;
            // revise the beginning instructions 
            output[pos] = output[pos] + std::to_string(-bytes);
            output[pos+1] = output[pos+1] + std::to_string(bytes-8);
            output[pos+2] = output[pos+2] + std::to_string(bytes-16);
            output[pos+3] = output[pos+3] + std::to_string(bytes);
            // return
            output.push_back("." + func.get_name() +"_return :");
            output.push_back("ld.d $ra, $sp, " + std::to_string(bytes-8));
            output.push_back("ld.d $fp, $sp, " + std::to_string(bytes-16));
            output.push_back("addi.d $sp, $sp, " + std::to_string(bytes));
            output.push_back("jr $ra");
        }
}

void CodeGen::visit(BasicBlock* bb){
    cur_bb = bb;
    output.push_back("." + cur_func->get_name() + "_" + bb->get_name() + ":");
    for(auto& instr : bb->get_instructions()){
        point = &instr;
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
            case Instruction::alloca: alloca_assembly(&instr); break;
            case Instruction::load: load_assembly(&instr); break;
            case Instruction::store: store_assembly(&instr); break;
            case Instruction::cmp: break;
            case Instruction::fcmp: break;
            case Instruction::phi: phi_assembly(&instr); break;
            case Instruction::call: call_assembly(&instr); break;
            case Instruction::getelementptr: getelementptr_assembly(&instr); break;
            case Instruction::zext: zext_assembly(&instr);break;
            case Instruction::fptosi: fptosi_assembly(&instr); break;
            case Instruction::sitofp: sitofp_assembly(&instr); break;
            default:
                assert(true);
        }
    }
    for(int i=4;i<=20;i++)SetReg(&R[i], nullptr);
    for(int i=0;i<=23;i++)SetReg(&FR[i], nullptr);
}
void CodeGen::bb_end_store(BasicBlock* bb){
    // phi
    for(auto& suc_bb : bb->get_succ_basic_blocks()){
        for(auto &inst : suc_bb->get_instructions()){
            if(inst.is_phi()){//op3 = phi(op2, op1)则令op12在出口处不活跃对其单独处理，将其值存储在op3的内存位置
                point = &inst;
                if(ValPos.find(&inst)==ValPos.end()){
                    offset -= inst.get_type()->get_size();
                    ValPos[&inst].set_off(offset);
                }
                assert(ValPos[&inst].get_off()!=-1);
                int off = ValPos[&inst].get_off();
                string head;
                if(inst.get_type()->is_float_type()) head = "fst.s ";
                else if(inst.get_type()->is_pointer_type()) head = "st.d ";
                else head = "st.w ";
                Reg* reg;
                if(cur_bb == inst.get_operand(1))
                    reg = GetReg(inst.get_operand(0));
                else if(inst.get_num_operand() == 4&&cur_bb == inst.get_operand(3))
                    reg = GetReg(inst.get_operand(2));
                else assert(false);
                output.push_back(head + reg->print() + ", $fp, " + std::to_string(off));
            }
            else break;
        }
    }
    // integer register
    for(int i=4;i<=20;i++){
        if(is_active(R[i].value, bb)){
            int off;
            if(ValPos[R[i].value].get_off()!=-1)
                off = ValPos[R[i].value].get_off();
            else{
                offset -= R[i].value->get_type()->get_size();
                off = offset;
            } 
            if(R[i].value->get_type()->is_integer_type())output.push_back("st.w " + R[i].print() + ", $fp, " + std::to_string(off));
            else output.push_back("st.d " + R[i].print() + ", $fp, " + std::to_string(off));
            ValPos[R[i].value].set_off(off);
        }
       SetReg(&R[i], nullptr);
    }
    // float register
    for(int i=0;i<=23;i++){
        if(is_active(FR[i].value, bb)){
            int off;
            if(ValPos[FR[i].value].get_off()!=-1)
                off = ValPos[FR[i].value].get_off();
            else{
                offset -= FR[i].value->get_type()->get_size();
                off = offset;
            } 
            output.push_back("fst.s " + FR[i].print() + ", $fp, " + std::to_string(off));
            ValPos[FR[i].value].set_off(off);
        }
        SetReg(&FR[i], nullptr);
    }
}
void CodeGen::ret_assembly(Instruction* instr){
    bool is_int = cur_func->get_return_type()->is_integer_type();
    bool is_float = cur_func->get_return_type()->is_float_type();
    if(is_int||is_float) {
        Value* v = instr->get_operand(0);
        Reg* Rv = GetReg(v);
        output.push_back((is_int?"or $a0, " + Rv->print() + ", $r0":"fmov.s $fa0, " + Rv->print()));
    }
    output.push_back("b ." + cur_func->get_name() +"_return");
}
void CodeGen::br_assembly(Instruction* instr){
    if(dynamic_cast<BranchInst*>(instr)->is_cond_br()){
        Instruction* cond = dynamic_cast<Instruction*>(instr->get_operand(0));
        assert(cond);
        if(cond->get_operand(0)->get_type()->is_float_type()){
            auto cur_point = point;
            point = cond;
            fcmp_assembly(cond, instr);
            point = cur_point;
        }
        else if(ValPos.find(cond)==ValPos.end()){
            auto cur_point = point;
            point = cond;
            cmp_assembly(cond, instr);
            point = cur_point;
        }
        else{
            auto reg = GetReg(cond);
            bb_end_store(cur_bb);
            string br1_name = "." + cur_func->get_name() + "_" + instr->get_operand(1)->get_name();
            string br2_name = "." + cur_func->get_name() + "_" + instr->get_operand(2)->get_name();
            output.push_back("bne " + reg->print() + ", $zero, " + br1_name);
            output.push_back("b " + br2_name);
        }
    }
    else {
        bb_end_store(cur_bb);
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
    //根据申请空间的大小计算偏移值
    int size = dynamic_cast<AllocaInst*>(instr)->get_alloca_type()->get_size();
    offset -= size;
    LocalOff[instr] = offset;
}
void CodeGen::load_assembly(Instruction* instr){
    Value* pt = instr->get_operand(0);
    Reg* reg;
    if(dynamic_cast<GlobalVariable*>(pt)){
        auto addr = GetReg(pt);
        reg = AllocaReg(instr);
        if(instr->get_type()->is_integer_type())
            output.push_back("ld.w " + reg->print() + ", " + addr->print() + ", 0");
        else if(instr->get_type()->is_float_type())
            output.push_back("fld.s " + reg->print() + ", " + addr->print() + ", 0");
        else assert(false);
    }
    else{
        string inst_tail;
        string inst_head;
        if(instr->get_type()->is_integer_type()) inst_head = "ld.w ";
        else if(instr->get_type()->is_float_type()) inst_head = "fld.s ";
        else assert(false);
        if(LocalOff.find(pt) != LocalOff.end()) inst_tail = ", $fp, " + std::to_string(LocalOff[pt]);
        else if(ValPos.find(pt) != ValPos.end()) {auto addr = GetReg(pt);inst_tail = ", " + addr->print() + ", 0";}
        else assert(false);
        reg = AllocaReg(instr);
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
        auto addr = GetReg(pt);
        if(type->is_integer_type())
            output.push_back("st.w " + reg->print() + ", " + addr->print() + ", 0");
        else if(type->is_float_type())
            output.push_back("fst.s " + reg->print() + ", " + addr->print() + ", 0");
        else assert(false);
    }
    else{
        string inst_tail;
        string inst_head;
        if(type->is_integer_type()) inst_head = "st.w ";
        else if(type->is_float_type()) inst_head = "fst.s ";
        else assert(false);
        if(LocalOff.find(pt) != LocalOff.end()) inst_tail = ", $fp, " + std::to_string(LocalOff[pt]);
        else if(ValPos.find(pt) != ValPos.end()) {auto addr = GetReg(pt);inst_tail = ", " + addr->print() + ", 0";}
        else assert(false);
        output.push_back(inst_head + reg->print() + inst_tail);
    }
}
void CodeGen::cmp_assembly(Instruction* instr, Instruction* refer){
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    auto l_reg = GetReg(lhs);
    auto r_reg = GetReg(rhs);
    if(refer->is_br()){
        bb_end_store(cur_bb);
        string br1_name = "." + cur_func->get_name() + "_" + refer->get_operand(1)->get_name();
        string br2_name = "." + cur_func->get_name() + "_" + refer->get_operand(2)->get_name();
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
            case CmpInst::EQ: output.push_back("beq " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("b " + br2_name);
                              break;
            case CmpInst::NE: output.push_back("bne " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("b " + br2_name);
                              break;
            case CmpInst::GT: output.push_back("bge " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("b " + br2_name);
                              break;
            case CmpInst::GE: output.push_back("bge " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("beq " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("b " + br2_name);
                              break;
            case CmpInst::LT: output.push_back("blt " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("b " + br2_name);
                              break;
            case CmpInst::LE: output.push_back("blt " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("beq " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              output.push_back("b " + br2_name);
                              break;
        }
    }
    else if(refer->is_zext()){
        auto res = AllocaReg(instr);
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
            case CmpInst::EQ: output.push_back("xor " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              output.push_back("sltui " + res->print() + ", " + res->print() + ", 1");
                              break;
            case CmpInst::NE: output.push_back("xor " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              output.push_back("sltu " + res->print() + ", " + "$zero, " + res->print());
                              break;
            case CmpInst::GT: output.push_back("slt " + res->print() + ", " + r_reg->print() + ", " + l_reg->print());
                              break;
            case CmpInst::GE: output.push_back("slt " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              output.push_back("xori " + res->print() + ", " + res->print() + ", 1");
                              break;
            case CmpInst::LT: output.push_back("slt " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              break;
            case CmpInst::LE: output.push_back("slt " + res->print() + ", " + r_reg->print() + ", " + l_reg->print());
                              output.push_back("xori " + res->print() + ", " + res->print() + ", 1");
                              break;
        }
        SetReg(res, instr);
    }
    else assert(false);
}
void CodeGen::fcmp_assembly(Instruction* instr, Instruction* refer){
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
    if(refer->is_br()){
        string br1_name = "." + cur_func->get_name() + "_" + refer->get_operand(1)->get_name();
        string br2_name = "." + cur_func->get_name() + "_" + refer->get_operand(2)->get_name();
        bb_end_store(cur_bb);
        output.push_back("bceqz $fcc0, " + br2_name);
        output.push_back("b " + br1_name);
    }
    else if(refer->is_zext()){
        string br = "." + cur_func->get_name() + "_" + instr->get_name();
        auto reg = AllocaReg(refer);
        output.push_back("addi.w " + reg->print() + ", $zero" + ", 1");
        output.push_back("bceqz $fcc0, " + br);
        output.push_back("addi.w " + reg->print() + ", $zero" + ", 0");
        output.push_back(br + ":");
        SetReg(reg, instr);
    }
    else assert(false);
}
void CodeGen::phi_assembly(Instruction* instr){
    auto reg = AllocaReg(instr);
    int off = ValPos[instr].get_off();
    assert(off!=-1);
    // 判断整型还是浮点
    if(instr->get_type()->is_integer_type())
        output.push_back("ld.w " + reg->print() + ", $fp, " + std::to_string(off));
    else if(instr->get_type()->is_float_type())
        output.push_back("fld.s " + reg->print() + ", $fp, " + std::to_string(off));
    else assert(false);
    SetReg(reg, instr);
}
void CodeGen::call_assembly(Instruction* instr){
    // first store the value of temporary registers and arguments registers
    for(int i=4;i<=20;i++){
        if(is_active_out(R[i].value, instr)&&ValPos[R[i].value].get_off()==-1){
            offset -= R[i].value->get_type()->get_size();
            ValPos[R[i].value].set_off(offset);
            output.push_back("st.w " + R[i].print() + ", $fp, " + std::to_string(offset));
        }
    }
    for(int i=0;i<=23;i++){
        if(is_active_out(FR[i].value, instr)&&ValPos[FR[i].value].get_off()==-1){
            offset -= FR[i].value->get_type()->get_size();
            ValPos[FR[i].value].set_off(offset);
            output.push_back("fst.s " + FR[i].print() + ", $fp, " + std::to_string(offset));
        }
    }
    // second load the arguments (if the number of arguments is more than 8)store the extra arguments in the memory
    int i,j,off=0;
    for(i=0,j=0;(i+j+1)<instr->get_num_operand();){
        auto op = instr->get_operand(i+j+1);
        if(op->get_type()->is_float_type())
            if(j++<8){// 寄存器传参
                auto s_reg = GetReg(op);
                if(&FR[j-1]!=s_reg)
                    output.push_back("fmov.s " + FR[j-1].print() + ", " + s_reg->print());
                SetReg(&FR[j-1], op);
            }
            else{//  堆栈传参
                output.push_back("fst.s " + GetReg(op)->print() + ", $sp, " + std::to_string(off));
                off += op->get_type()->get_size();
            }   
        else if(i++<8){// 寄存器传参
                auto s_reg = GetReg(op);
                if(&R[i+3]!=s_reg)
                    output.push_back("or " + R[i+3].print() + ", " + s_reg->print() + ", $zero");
                SetReg(&R[i+3], op);
            }
        else{   //  堆栈传参
            output.push_back("st.w " + GetReg(op)->print() + ", $sp, " + std::to_string(off));
            off += op->get_type()->get_size();
        }
    }
    if(off>max_arg_size)max_arg_size=off;
    // third "bl " function_name
    output.push_back("bl " + instr->get_operand(0)->get_name());
    // fourth mark the value of registers as null
    for(int i=4;i<=20;i++)SetReg(&R[i], nullptr);
    for(int i=0;i<=23;i++)SetReg(&FR[i], nullptr);
    if(instr->get_type()->is_integer_type())SetReg(&R[4], instr);
    else if(instr->get_type()->is_float_type())SetReg(&FR[0], instr);
}
void CodeGen::getelementptr_assembly(Instruction* instr){
    Value* pt = instr->get_operand(0);
    auto type = pt->get_type()->get_pointer_element_type();
    Reg* addr;
    if(dynamic_cast<GlobalVariable*>(pt)){
        if(dynamic_cast<ConstantInt*>(instr->get_operand(2))){
            addr = AllocaReg(instr);
            int elem_off = dynamic_cast<ConstantInt*>(instr->get_operand(2))->get_value();
            int elem_size = type->get_array_element_type()->get_size();
            output.push_back("la.local " + addr->print() + ", " + pt->get_name());
            if(elem_off*elem_size)output.push_back("addi.d " + addr->print() + ", " + addr->print() + ", " + std::to_string(elem_off*elem_size));
        }
        else {
            auto index_reg = GetReg(instr->get_operand(2));
            addr = AllocaReg(instr);
            SetReg(index_reg, nullptr);
            int elem_size = type->get_array_element_type()->get_size();
            output.push_back("la.local " + addr->print() + ", " + pt->get_name());
            if(elem_size){
                output.push_back("slli.w " + index_reg->print() + ", " + index_reg->print() + ", " + std::to_string((int)(log2(elem_size))));
                output.push_back("add.d " + addr->print() + ", " + addr->print() + ", " + index_reg->print());
            }
        }
        SetReg(addr, instr);
    }
    else{
        assert(LocalOff.find(pt) != LocalOff.end() || ValPos.find(pt) != ValPos.end());
        if(type->is_array_type()){
            if(dynamic_cast<ConstantInt*>(instr->get_operand(2))){
                int elem_off = dynamic_cast<ConstantInt*>(instr->get_operand(2))->get_value();
                int elem_size = pt->get_type()->get_pointer_element_type()->get_size();
                int off = LocalOff[pt] + elem_off*elem_size;
                LocalOff[instr] = off;
            }
            else{
                auto index_reg = GetReg(instr->get_operand(2));
                addr = AllocaReg(instr);
                int elem_size = type->get_array_element_type()->get_size();
                output.push_back("slli.d " + index_reg->print() + ", " + index_reg->print() + ", " + std::to_string((int)(log2(elem_size))));
                output.push_back("addi.d " + addr->print() + ", $fp, " + std::to_string(LocalOff[pt]));
                output.push_back("add.d " + addr->print() + ", " + addr->print() + ", " + index_reg->print());
                SetReg(index_reg, nullptr);
                SetReg(addr, instr);
            }
        }
        else{
            auto reg = GetReg(pt);
            if(dynamic_cast<ConstantInt*>(instr->get_operand(1))){
                addr = AllocaReg(instr);
                int elem_off = dynamic_cast<ConstantInt*>(instr->get_operand(1))->get_value();
                int elem_size = pt->get_type()->get_pointer_element_type()->get_size();
                output.push_back("addi.d " + addr->print() + ", " + reg->print() + ", " + std::to_string(elem_off*elem_size));
            }
            else{
                auto index_reg = GetReg(instr->get_operand(1));
                addr = AllocaReg(instr);
                SetReg(index_reg, nullptr);
                int elem_size = type->get_size();
                output.push_back("slli.w " + index_reg->print() + ", " + index_reg->print() + ", " + std::to_string((int)(log2(elem_size))));
                output.push_back("add.d " + addr->print() + ", " + reg->print() + ", " + index_reg->print());
            }
            SetReg(addr, instr);
        }
    }
}
void CodeGen::zext_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    if(ValPos.find(val)==ValPos.end()){
        auto cur_point = point;
        point = dynamic_cast<Instruction*>(val);
        if(dynamic_cast<Instruction*>(val)->is_cmp())
            cmp_assembly(dynamic_cast<Instruction*>(val), instr);
        else if(dynamic_cast<Instruction*>(val)->is_fcmp())
            fcmp_assembly(dynamic_cast<Instruction*>(val), instr);
        else assert(false);
        point = cur_point;
    }
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    output.push_back("or " + res->print() + ", " + reg->print() + ", $zero");
    SetReg(res, instr);
}
void CodeGen::fptosi_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto tmp_fr = AllocaTmpReg();
    auto res = AllocaReg(instr);
    output.push_back("ftint.w.s " + reg->print() + ", " + reg->print());
    output.push_back("movfr2gr.s " + res->print() + ", " + reg->print());
    SetReg(res, instr);
}
void CodeGen::sitofp_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    output.push_back("movgr2fr.w " + res->print() + ", " + reg->print());
    output.push_back("ffint.s.w " + res->print() + ", " + res->print());
    SetReg(res, instr);
}
Reg* CodeGen::GetReg(Value* v) {// 一定会返回存有此值的寄存器
    if(ValPos.find(v)!=ValPos.end()&&ValPos[v].get_reg()!=nullptr) {// 如果寄存器存在此变量的值
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
                    output.push_back("lu12i.w " + reg->print() + ", " + std::to_string(val-val%4096) + ">>12");
                    if(val%4096)output.push_back("ori " + reg->print() + ", " + reg->print() + ", " + std::to_string(val%4096));
                    SetReg(reg, v);
                    return reg;
                }
            }
            else{//浮点型常量
                auto tmp_r = AllocaTmpReg();
                auto reg = AllocaReg(v);
                float f_val = dynamic_cast<ConstantFP*>(v)->get_value();
                int i_val = *(int*)&f_val;
                output.push_back("lu12i.w " + tmp_r->print() + ", " + std::to_string(i_val-i_val%4096) + ">>12");
                if(i_val%4096)output.push_back("ori " + tmp_r->print() + ", " + tmp_r->print() + ", " + std::to_string(i_val%4096));
                output.push_back("movgr2fr.w " + reg->print() + ", " + tmp_r->print());
                tmp_r->locked = false;
                SetReg(reg, v);
                return reg;
            }
        }
        else if(dynamic_cast<GlobalVariable*>(v)) {// 全局变量
            auto reg = AllocaReg(v);
            output.push_back("la.local " + reg->print() + "," + v->get_name());
            SetReg(reg, v);
            return reg;
        }
        else { //局部变量
            assert(ValPos.find(v)!=ValPos.end()||LocalOff.find(v)!=LocalOff.end());
            auto reg = AllocaReg(v);
            if(ValPos.find(v)!=ValPos.end()){// 局部int或float型变量值或参数指针值
                int off = ValPos[v].get_off();
                assert(off!=-1);
                if(v->get_type()->is_float_type())
                    output.push_back("fld.s " + reg->print() + ", $fp, " + std::to_string(off));
                else if(v->get_type()->is_pointer_type())
                    output.push_back("ld.d " + reg->print() + ", $fp, " + std::to_string(off));
                else
                    output.push_back("ld.w " + reg->print() + ", $fp, " + std::to_string(off));
            }
            else {//局部指针值
                int off = LocalOff[v];
                output.push_back("addi.d " + reg->print() + ", $fp, " + std::to_string(off));
            }
            SetReg(reg, v);
            return reg;
        }
    }
    return RandomReg();

}
Reg* CodeGen::AllocaReg(Value* v){
    if(v->get_type()->is_float_type())return LinearScanFR();
    else return LinearScanR();
}
Reg* CodeGen::AllocaTmpReg(){
    for(int i=4;i<=20;i++)
        if(!is_active(R[i].value, point)&&R[i].locked!=true){
            R[i].locked = true;
            return &R[i];
        }
} 
Reg* CodeGen::AllocaTmpFReg(){
    for(int i=0;i<=23;i++)
        if(!is_active(FR[i].value, point)&&FR[i].locked!=true){
            FR[i].locked = true;
            return &FR[i];
        }
}
Reg* LinearScanR(){
    for(int i=4;i<=20;i++)
        if(!is_active(R[i].value, point)&&R[i].locked!=true)return &R[i];
}
Reg* LinearScanFR(){
    for(int i=0;i<=23;i++)
        if(!is_active(FR[i].value, point)&&FR[i].locked!=true)return &FR[i];
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
    if (ValPos.find(val)!=ValPos.end()&&ValPos[val].get_reg()!=nullptr)ValPos[val].get_reg()->value = nullptr;
    if (r->value!=nullptr) ValPos[r->value].set_reg(nullptr);
    r->value = val;
    if (val) ValPos[val].set_reg(r);
}