#include "codegen.hpp"
#include<memory>
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
Reg R[32]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};// integer register
FReg FR[24]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};// float register
//Program point
Function *cur_func;
BasicBlock *cur_bb;
Instruction *point;
// register allocation
std::set<Reg*> locked_regs;
int next_reg=0;
int next_freg=0;
// preprocess regs alloca
bool preprocessing;
//Address Descriptor
std::map<BasicBlock*, std::map<Value*, Reg*>> RegDesc;
std::map<Value*, int> OffsetDesc;
//Offset Optimization
std::map<Value*, int> LocalOff;
//GlobalValue Optimization
namespace gv_opt{
    std::map<Value*, int> mem_int;
    std::map<Value*, float> mem_float;
}
//Constant Optimization
const bool const_optim_flag=true;
namespace const_opt{
    std::map<Value*, int>inst2int;
    std::map<Value*, float>inst2float;
    inline bool is_const(Value* inst){
        if(inst2int.find(inst)!=inst2int.end() || inst2float.find(inst)!=inst2float.end())return true;
        if(dynamic_cast<ConstantInt*>(inst)){inst2int[inst]=dynamic_cast<ConstantInt*>(inst)->get_value();return true;}
        if(dynamic_cast<ConstantFP*>(inst)){inst2float[inst]=dynamic_cast<ConstantFP*>(inst)->get_value();return true;}
        return false;
    }
    inline bool is_integer(Value * inst){
        return inst2int.find(inst) != inst2int.end();
    }
    inline bool is_float(Value* inst){
        return inst2float.find(inst) != inst2float.end();
    }
    inline int get_ival(Value* inst){
        assert(inst2int.find(inst)!=inst2int.end());
        return inst2int[inst];
    }
    inline float get_fval(Value* inst){
        assert(inst2float.find(inst)!=inst2float.end());
        return inst2float[inst];
    }
    inline void set_ival(Value* inst, int val){
        inst2int[inst]=val;
    }
    inline void set_fval(Value* inst, float val){
        inst2float[inst]=val;
    }
}
using namespace const_opt;
//Memory Calculate
int offset;
int max_arg_size;
//Live Variable Analysis
std::map<BasicBlock*, set<Value*>> OUT;// active vars at the exit of the basicblocks
std::map<Value*, set<Value*>> in;
std::map<Value*, set<Value*>> out;
namespace ActiveVarsFunc{//Wrapping functions for program point update and discrimination
    inline std::set<Value*> clone(set<Value*> s){
        return s;
    }
    inline bool is_active(Value* inst, BasicBlock* bb){
        return OUT[bb].find(inst)!=OUT[bb].end();
    }
    inline bool is_active(Value* inst, Value* point){// 当前程序点活跃
        return in[point].find(inst)!=in[point].end();
    }
    inline bool is_active_out(Value* inst, Value* point){// 当前程序点出口活跃
        return out[point].find(inst)!=out[point].end();
    }
    inline int CurOff(Value* inst){// acquire the memory offset of inst
        assert(OffsetDesc.find(inst)!=OffsetDesc.end());
        return OffsetDesc[inst];
    }
    inline Reg* CurReg(Value* inst, BasicBlock* bb=cur_bb){// acquire the current reg of inst
        assert(RegDesc[bb].find(inst)!=RegDesc[bb].end());
        return RegDesc[bb][inst];
    }
    inline void SetOff(Value* inst, int off){// set the memory offset of inst
        OffsetDesc[inst]=off;
    }
    inline void SetReg(Value* inst, Reg* reg, BasicBlock* bb=cur_bb){// update the position of inst's reg
        if(reg)RegDesc[bb][inst]=reg;
        else RegDesc[bb].erase(inst);
    }
    inline bool is_in_reg(Value* inst, BasicBlock* bb=cur_bb){// determine whether inst has its own address descriptor
        return RegDesc[bb].find(inst)!=RegDesc[bb].end()&&RegDesc[bb][inst]!=nullptr;
    }
    inline bool is_stored(Value* inst){// determine whether inst has been allocated memory space for 
        return OffsetDesc.find(inst)!=OffsetDesc.end();
    }
    inline bool is_used(Value* inst){
        return is_in_reg(inst) || is_stored(inst) || is_const(inst);
    }
}
using namespace ActiveVarsFunc;
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
        for(auto op : cur_inst->get_operands())if(!(dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)||dynamic_cast<Constant*>(op)))in[cur_inst].insert(op);
        suc_inst = cur_inst;
        for(;iter!=bb.get_instructions().begin();iter--){
            cur_inst = &(*iter);
            out[cur_inst] = in[suc_inst];
            in[cur_inst]=out[cur_inst];
            in[cur_inst].insert(cur_inst);
            in[cur_inst].erase(suc_inst);
            for(auto op : cur_inst->get_operands())if(!(dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)||dynamic_cast<Constant*>(op)))in[cur_inst].insert(op);
            suc_inst = cur_inst;
        }
        cur_inst = &(*iter);
        out[cur_inst] = in[suc_inst];
        in[cur_inst] = out[cur_inst];
        in[cur_inst].insert(cur_inst);
        in[cur_inst].erase(suc_inst);
        for(auto op : cur_inst->get_operands())if(!(dynamic_cast<BasicBlock*>(op)||dynamic_cast<Function*>(op)||dynamic_cast<Constant*>(op)))in[cur_inst].insert(op);
    }
    // delete the active phi var at the exit of bb blocks
    // for(auto &bb : func->get_basic_blocks()){
    //     for(auto &suc_bb : bb.get_succ_basic_blocks()){
    //         for(auto &inst : suc_bb->get_instructions()){
    //             if(inst.is_phi())
    //                 for(auto &op : inst.get_operands())
    //                     OUT[&bb].erase(op);
    //             else break;
    //         }
    //     }
    // }
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

// Short Function Optimization
class InlineFunc{
    public:
        InlineFunc(Function* fun) : func(fun) {}
        void set_ival(int val, Value* inst = nullptr){ if(inst)int_map[inst] = val;else int_map[instr] = val; }
        void set_fval(float val, Value* inst = nullptr) { if(inst)float_map[inst] = val; else float_map[instr] = val; }
        int get_const_int_value(Value* ins){
            if(dynamic_cast<ConstantInt*>(ins)) return dynamic_cast<ConstantInt*>(ins)->get_value();
            else if(int_map.find(ins)==int_map.end()){able_opt=false;return 0;}
            return int_map[ins];
        }
        float get_const_fp_value(Value* ins){
            if(dynamic_cast<ConstantFP*>(ins)) return dynamic_cast<ConstantFP*>(ins)->get_value();
            else if(float_map.find(ins)==float_map.end()){able_opt=false;return 0;}
            return float_map[ins];
        }
        vector<string>& gen_inline_code(){return inline_code;}
        bool run(vector<Value*>& real_args){
            // initialize
            inline_code.clear();
            int_map.clear();
            float_map.clear();
            able_opt = true;
            // insert real_args into mapping table
            int index_arg = 0;
            for(auto formal_arg : func->get_args()){
                if (formal_arg->get_type()->is_integer_type()&&const_opt::is_const(real_args[index_arg])){
                    set_ival(const_opt::get_ival(real_args[index_arg]), formal_arg);
                }
                else if (formal_arg->get_type()->is_float_type()&&const_opt::is_const(real_args[index_arg])){
                    set_fval(const_opt::get_fval(real_args[index_arg]), formal_arg);
                }
                else no_map.insert(formal_arg);
                index_arg++;
            }
            // traverse instructions
            BasicBlock* bb = func->get_entry_block();
            while(able_opt){
                for(auto& inst : bb->get_instructions()){
                    instr = &inst;
                    auto op = instr->get_instr_type();
                    int num = instr->get_num_operand();
                    auto lhs = instr->get_operand(0);
                    auto rhs = num==2?instr->get_operand(1):nullptr;
                    switch (op) {
                    case Instruction::add: set_ival(get_const_int_value(lhs) + get_const_int_value(rhs));break;
                    case Instruction::sub: set_ival(get_const_int_value(lhs) - get_const_int_value(rhs));break;
                    case Instruction::mul: set_ival(get_const_int_value(lhs) * get_const_int_value(rhs));break;
                    case Instruction::sdiv: set_ival(get_const_int_value(lhs) / get_const_int_value(rhs));break;
                    case Instruction::fadd: set_fval(get_const_fp_value(lhs) + get_const_fp_value(rhs));break;
                    case Instruction::fsub: set_fval(get_const_fp_value(lhs) - get_const_fp_value(rhs));break;
                    case Instruction::fmul: set_fval(get_const_fp_value(lhs) * get_const_fp_value(rhs));break;
                    case Instruction::fdiv: set_fval(get_const_fp_value(lhs) / get_const_fp_value(rhs));break;
                    case Instruction::cmp:
                        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
                        case CmpInst::EQ: set_ival(get_const_int_value(lhs) == get_const_int_value(rhs));break;
                        case CmpInst::NE: set_ival(get_const_int_value(lhs) != get_const_int_value(rhs));break;
                        case CmpInst::GT: set_ival(get_const_int_value(lhs) > get_const_int_value(rhs));break;
                        case CmpInst::GE: set_ival(get_const_int_value(lhs) >= get_const_int_value(rhs));break;
                        case CmpInst::LT: set_ival(get_const_int_value(lhs) < get_const_int_value(rhs));break;
                        case CmpInst::LE: set_ival(get_const_int_value(lhs) <= get_const_int_value(rhs));break;
                        }
                        break;
                    case Instruction::fcmp:
                        switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
                        case FCmpInst::EQ: set_ival(get_const_fp_value(lhs) == get_const_fp_value(rhs));break;
                        case FCmpInst::NE: set_ival(get_const_fp_value(lhs) != get_const_fp_value(rhs));break;
                        case FCmpInst::GT: set_ival(get_const_fp_value(lhs) > get_const_fp_value(rhs));break;
                        case FCmpInst::GE: set_ival(get_const_fp_value(lhs) >= get_const_fp_value(rhs));break;
                        case FCmpInst::LT: set_ival(get_const_fp_value(lhs) < get_const_fp_value(rhs));break;
                        case FCmpInst::LE: set_ival(get_const_fp_value(lhs) <= get_const_fp_value(rhs));break;
                        }
                        break;
                    case Instruction::sitofp: set_fval((float)get_const_int_value(lhs));break;
                    case Instruction::fptosi: set_ival((int)get_const_fp_value(lhs));break;
                    case Instruction::zext: set_ival((int)get_const_int_value(lhs));break;
                    case Instruction::br:   
                        if(lhs==nullptr) 
                            bb = dynamic_cast<BasicBlock*>(instr->get_operand(0));
                        else if(get_const_int_value(lhs)){
                            bb = dynamic_cast<BasicBlock*>(instr->get_operand(1));
                        }
                        else bb = dynamic_cast<BasicBlock*>(instr->get_operand(2));
                        break;
                    case Instruction::ret: 
                        if(func->get_return_type()->is_integer_type()){
                            int val = get_const_int_value(lhs);
                            if(val>0&&val<=4095)
                                inline_code.push_back("ori $r4, $zero, " + std::to_string(val));
                            else if(val>=-2048&&val<0)
                                inline_code.push_back("addi.w $r4, $zero, " + std::to_string(val));
                            else{ 
                                inline_code.push_back("lu12i.w $r4, " + std::to_string(val>>12));
                                if(val%4096)inline_code.push_back("ori $r4, $r4, " + std::to_string(val%4096));
                            }
                        }
                        else if(func->get_return_type()->is_float_type()){
                            float f_val = get_const_fp_value(lhs);
                            int i_val = *(int*)&f_val;
                            inline_code.push_back("lu12i.w $r4, " + std::to_string(i_val>>12));
                            if(i_val%4096)inline_code.push_back("ori $r4, $r4, " + std::to_string(i_val%4096));
                            inline_code.push_back("movgr2fr.w $fa0, $r4");
                        }
                        else return false;
                        return true;
                    default: return false;
                    }
                    if(!able_opt)return false;
                }
            }
        }
    
    private:
        bool able_opt;
        Instruction* instr;
        vector<string> inline_code;
        std::set<Value*> no_map;
        std::map<Value*, int> int_map;
        std::map<Value*, float> float_map; 
        Function* func;
};
bool memory_used;
bool call_used;
vector<string> func_entry;
vector<string> func_exit;
std::map<Function*, std::shared_ptr<vector<string>>>short_func;
std::map<Value*, std::shared_ptr<InlineFunc>>inline_func;
// Control Flow Optimization
class CFG{
    public:
        CFG() = default;
        BasicBlock* jump_end_bb(BasicBlock* bb){
            if(bb2code[bb].size()!=2)return bb;
            else if(jump_bb[bb])return jump_end_bb(jump_bb[bb]);
            else return bb;
        }
        void push_back(string code, BasicBlock* bb) { bb2code[bb].push_back(code); }
        void insert(vector<string>& codes, BasicBlock* bb){ bb2code[bb].insert(bb2code[bb].end(), codes.begin(), codes.end());}
        BasicBlock* get_jump_bb(BasicBlock* bb) { return jump_bb.find(bb)!=jump_bb.end()?jump_bb[bb]:nullptr;}
        BasicBlock* get_cj_bb(BasicBlock* bb) { return cj_bb.find(bb)!=cj_bb.end()?cj_bb[bb]:nullptr;}
        void clear() {bb2code.clear();cj_bb.clear();jump_bb.clear();}
        void set_jump_bb(BasicBlock* next_bb, BasicBlock* bb = cur_bb){
            if(preprocessing)return;
            assert(next_bb&&jump_bb.find(bb)==jump_bb.end());
            jump_bb[bb] = next_bb;
        }
        void new_jump_bb(BasicBlock* next_bb, BasicBlock* bb) { jump_bb[bb] = next_bb; }
        void set_cj_bb(BasicBlock* next_bb, BasicBlock* bb = cur_bb){
            if(preprocessing)return;
            assert(next_bb&&cj_bb.find(bb)==cj_bb.end());
            cj_bb[bb] = next_bb;
        }
        void new_cj_bb(BasicBlock* next_bb, BasicBlock* bb) { cj_bb[bb] = next_bb; }
        vector<string>& operator[] (BasicBlock* bb) { return bb2code[bb]; }
    private:
        std::map<BasicBlock*, vector<string>> bb2code;
        std::map<BasicBlock*, BasicBlock*> cj_bb;
        std::map<BasicBlock*, BasicBlock*> jump_bb;
};
CFG cfg;
void CodeGen::CFopt(Function* func){
    std::map<BasicBlock*, bool> visited;
    vector<BasicBlock*> stack;
    stack.push_back(func->get_entry_block());
    while(stack.size()){// for blocks with consecutive jumps, jump to the end block
        auto bb = stack.back();
        stack.pop_back();
        if(visited[bb])continue;
        visited[bb]=true;
        if(cfg.get_cj_bb(bb)){
            if(cfg[cfg.get_cj_bb(bb)].size()==2){
                BasicBlock* new_branch = cfg.jump_end_bb(cfg.get_cj_bb(bb));
                int index = cfg[bb].size()-2;
                cfg[bb][index] = cfg[bb][index].substr(0, cfg[bb][index].find('.')) + "." + cur_func->get_name() + "_" + new_branch->get_name(); 
                cfg.new_cj_bb(new_branch, bb);
                stack.push_back(new_branch);
            }
            else stack.push_back(cfg.get_cj_bb(bb));
        }
        if(cfg.get_jump_bb(bb)){
            if(cfg[cfg.get_jump_bb(bb)].size()==2){
                BasicBlock* new_branch = cfg.jump_end_bb(cfg.get_jump_bb(bb));
                int index = cfg[bb].size()-1;
                cfg[bb][index] = cfg[bb][index].substr(0, cfg[bb][index].find('.')) + "." + cur_func->get_name() + "_" + new_branch->get_name(); 
                cfg.new_jump_bb(new_branch, bb);
                stack.push_back(new_branch);
            }
            else stack.push_back(cfg.get_jump_bb(bb));
        }
    }
    stack.push_back(func->get_entry_block());
    visited.clear();
    std::shared_ptr<vector<string>> func_body = std::make_shared<vector<string>>();
    while(stack.size()){// insert codes of the basicblock in a depth-first manner
        auto bb = stack.back();
        stack.pop_back();
        if(visited[bb])continue;
        visited[bb]=true;
        if(cfg.get_cj_bb(bb))stack.push_back(cfg.get_cj_bb(bb));
        if(cfg.get_jump_bb(bb)&&!visited[cfg.get_jump_bb(bb)]){// remove the redundant br instruction
            cfg[bb].pop_back();
            if(cfg[bb].size()==1)cfg[bb].pop_back();
            stack.push_back(cfg.get_jump_bb(bb));
        }
        func_body->insert(func_body->end(), cfg[bb].begin(),cfg[bb].end());
    }
    if(!(memory_used||call_used)&&func_body->size()<=10)short_func[cur_func]=func_body;
    if(!(memory_used||call_used))inline_func[cur_func]=std::make_shared<InlineFunc>(cur_func);
    if(memory_used||call_used)output.insert(output.end(), func_entry.begin(), func_entry.end());
    output.insert(output.end(), func_body->begin(), func_body->end());
    if(memory_used||call_used)output.insert(output.end(), func_exit.begin(), func_exit.end());
}
// Inter-block spanning optimization
std::map<BasicBlock*, std::map<Value*, Reg*>> InitRegDesc;
void CodeGen::join(BasicBlock* dest_bb,BasicBlock* pre_bb1, BasicBlock* pre_bb2){
    for(auto [key, val] : RegDesc[pre_bb1]){
        if(RegDesc[pre_bb2].find(key)!=RegDesc[pre_bb2].end()&&RegDesc[pre_bb2][key]==val)
            InitRegDesc[dest_bb][key]=val;
    }
}
void CodeGen::RegDescUpdate(Function* func){
    preprocessing = true;
    for(auto& bb : func->get_basic_blocks())visit(&bb);
    preprocessing = false;
}
void CodeGen::RegFlowAnalysis(Function* func){
    InitRegDesc.clear();
    ActiveVars(func);
    bool changed = true;
    while(changed){
        changed = false;
        //initialize
        OffsetDesc.clear();
        RegDesc.clear();
        offset = -16;
        max_arg_size = 0;
        LoadArgs(func);
        cur_func = func;
        // update RegDesc
        RegDescUpdate(func);
        for(auto& BB:func->get_basic_blocks()){
            BasicBlock* bb = &BB;
            std::map<Value*, Reg*> tmpRegDesc = InitRegDesc[bb];
            if(bb!=func->get_entry_block())InitRegDesc[bb].clear();
            size_t pre_bb_num = bb->get_pre_basic_blocks().size();
            if(pre_bb_num == 0) continue;
            else if(pre_bb_num == 1){
                auto suc_bb = *bb->get_pre_basic_blocks().begin();
                InitRegDesc[bb] = RegDesc[suc_bb];
            }
            else if(pre_bb_num == 2){
                auto iter = bb->get_pre_basic_blocks().begin();
                auto pre_bb1 = *iter;
                iter++;
                auto pre_bb2 = *iter;
                join(bb, pre_bb1, pre_bb2);
            }
            else assert(false);
            if(tmpRegDesc != InitRegDesc[bb])changed = true;
        }
    }
}
bool CodeGen::is_inerited(Value* val, BasicBlock* bb){
    for(auto suc_bb : bb->get_succ_basic_blocks()){
        if(InitRegDesc[suc_bb].find(val)==InitRegDesc[suc_bb].end())return false;
    }
    return true;
}
inline void CodeGen::DefineStore(Value* val, Reg* reg){
    if(OffsetDesc.find(val)!=OffsetDesc.end())
        if(val->get_type()->is_float_type())
            gen_code("fst.s " + reg->print() + ", $fp, " + std::to_string(OffsetDesc[val]));
        else
            gen_code("st.w " + reg->print() + ", $fp, " + std::to_string(OffsetDesc[val]));
}
int CodeGen::LoadArgs(Function* func){
    int i, f, off;
    i=f=off=0;
    for(auto& arg : func->get_args()){
        cur_bb=func->get_entry_block();
        if(arg->get_type()->is_integer_type()){
            if(i<8){
                DefineStore(arg, &R[i+4]);
                InitRegDesc[cur_bb][arg] = &R[i+4];
                R[(i++)+4].value = arg;
            }
            else {
                SetOff(arg, off);
                off += arg->get_type()->get_size();
            }
        }
        else if(arg->get_type()->is_float_type()){
            if(f<8){
                DefineStore(arg, &FR[f]);
                InitRegDesc[cur_bb][arg] = &FR[f];
                FR[f++].value = arg;
            }
            else {
                SetOff(arg, off);
                off += arg->get_type()->get_size();
            }
        }
        else if(arg->get_type()->is_pointer_type()){
            if(i<8){
                DefineStore(arg, &R[i+4]);
                InitRegDesc[cur_bb][arg] = &R[i+4];
                R[(i++)+4].value = arg;
            }
            else {
                SetOff(arg, off);
                off += arg->get_type()->get_size();
            }
        }
        else assert(false);
    }
}
inline void CodeGen::gen_code(string assem){
    if(!preprocessing)cfg.push_back(assem, cur_bb);
}
inline void CodeGen::gen_code(vector<string>& assem){
    if(!preprocessing)cfg.insert(assem, cur_bb);
}
inline void func_init(Function* func){
    // OffsetDesc.clear();
    RegDesc.clear();
    cfg.clear();
    // ActiveVars(func);
    cur_func = func;
    offset = -16;
    max_arg_size = 0;
    memory_used = false;
    call_used = false;
    func_entry.clear();
    func_exit.clear();
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
            // preanalysis of register using and memory using
            RegFlowAnalysis(&func);
            // calculate the memory used by the function
            offset -= max_arg_size;
            int bytes = -offset + (offset%16?16+offset%16:0);
            if(bytes!=16)memory_used = true;
            // initialize
            func_init(&func);
            // load arguments
            int off = LoadArgs(&func);
            // entry
            output.push_back(".globl " + func.get_name());
            output.push_back(".type " + func.get_name() + ", @function");
            output.push_back(func.get_name() + ":");
            // revise the beginning instructions 
            // func_entry[0] = func_entry[0] + std::to_string(-bytes);
            // func_entry[1] = func_entry[1] + std::to_string(bytes-8);
            // func_entry[2] = func_entry[2] + std::to_string(bytes-16);
            // func_entry[3] = func_entry[3] + std::to_string(bytes);
            func_entry.push_back("addi.d $sp, $sp, " + std::to_string(-bytes));
            func_entry.push_back("st.d $ra, $sp, " + std::to_string(bytes-8));
            func_entry.push_back("st.d $fp, $sp, " + std::to_string(bytes-16));
            func_entry.push_back("addi.d $fp, $sp, " + std::to_string(bytes));
            // body
            for(auto& bb : func.get_basic_blocks()){
                visit(&bb);
            }
            // return
            func_exit.push_back("." + func.get_name() +"_return :");
            func_exit.push_back("ld.d $ra, $sp, " + std::to_string(bytes-8));
            func_exit.push_back("ld.d $fp, $sp, " + std::to_string(bytes-16));
            func_exit.push_back("addi.d $sp, $sp, " + std::to_string(bytes));
            // Control Flow opt
            CFopt(cur_func);
            if(!(memory_used||call_used))output.push_back("." + func.get_name() +"_return :");
            output.push_back("jr $ra");
        }
}

void CodeGen::visit(BasicBlock* bb){
    cur_bb = bb;
    //load RegDesc
    for(int i=0;i<32;i++)R[i].value = nullptr;
    for(int i=0;i<24;i++)FR[i].value = nullptr;
    for(auto [key, val] : InitRegDesc[bb])val->value = key;
    RegDesc[bb]=InitRegDesc[bb];
    gen_code("." + cur_func->get_name() + "_" + bb->get_name() + ":");
    for(auto& instr : bb->get_instructions()){
        point = &instr;
        locked_regs.clear();
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
}
void CodeGen::bb_end_store(BasicBlock* bb){
    // phi
    for(auto& suc_bb : bb->get_succ_basic_blocks()){
        for(auto &inst : suc_bb->get_instructions()){
            if(inst.is_phi()){//op3 = phi(op2, op1)则令op12在出口处不活跃对其单独处理，将其值存储在op3的内存位置
                point = &inst;
                if(!is_stored(&inst)){
                    offset -= inst.get_type()->get_size()+offset%inst.get_type()->get_size();
                    SetOff(&inst, offset);
                }
                int off = CurOff(&inst);
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
                gen_code(head + reg->print() + ", $fp, " + std::to_string(off));
            }
            else break;
        }
    }
    // integer register
    for(int i=4;i<=20;i++){
        if(is_active(R[i].value, bb)&&!is_inerited(R[i].value, bb)){
            int off;
            if(is_stored(R[i].value))
                off = CurOff(R[i].value);
            else{
                offset -= R[i].value->get_type()->get_size()+offset%R[i].value->get_type()->get_size();
                off = offset;
            } 
            if(R[i].value->get_type()->is_integer_type())gen_code("st.w " + R[i].print() + ", $fp, " + std::to_string(off));
            else gen_code("st.d " + R[i].print() + ", $fp, " + std::to_string(off));
            SetOff(R[i].value, off);
        }
    }
    // float register
    for(int i=0;i<=23;i++){
        if(is_active(FR[i].value, bb)&&!is_inerited(FR[i].value, bb)){
            int off;
            if(is_stored(FR[i].value))
                off = CurOff(FR[i].value);
            else{
                offset -= FR[i].value->get_type()->get_size()+offset%FR[i].value->get_type()->get_size();
                off = offset;
            } 
            gen_code("fst.s " + FR[i].print() + ", $fp, " + std::to_string(off));
            SetOff(FR[i].value, off);
        }
    }
}
void CodeGen::ret_assembly(Instruction* instr){
    bool is_int = cur_func->get_return_type()->is_integer_type();
    bool is_float = cur_func->get_return_type()->is_float_type();
    if(is_int||is_float) {
        Value* v = instr->get_operand(0);
        Reg* Rv = GetReg(v);
        if(Rv!=&R[4]&&Rv!=&FR[0])gen_code((is_int?"or $a0, " + Rv->print() + ", $r0":"fmov.s $fa0, " + Rv->print()));
    }
    gen_code("b ." + cur_func->get_name() +"_return");
}
void CodeGen::br_assembly(Instruction* instr){
    if(dynamic_cast<BranchInst*>(instr)->is_cond_br()){
        if(dynamic_cast<Constant*>(instr->get_operand(0))){
            int flag = dynamic_cast<ConstantInt*>(instr->get_operand(0))->get_value();
            string br1_name = "." + cur_func->get_name() + "_" + instr->get_operand(1)->get_name();
            string br2_name = "." + cur_func->get_name() + "_" + instr->get_operand(2)->get_name();
            if(flag)gen_code("b " + br1_name);
            else gen_code("b " + br2_name);
            if(flag)cfg.set_jump_bb(dynamic_cast<BasicBlock*>(instr->get_operand(1)));
            else cfg.set_jump_bb(dynamic_cast<BasicBlock*>(instr->get_operand(2)));
            return;
        }
        auto cond = dynamic_cast<Instruction*>(instr->get_operand(0));
        assert(cond);
        if(cond->get_operand(0)->get_type()->is_float_type()){
            auto cur_point = point;
            point = cond;
            fcmp_assembly(cond, instr);
            point = cur_point;
        }
        else if(!is_used(cond)){
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
            gen_code("bne " + reg->print() + ", $zero, " + br1_name);
            gen_code("b " + br2_name);
            cfg.set_cj_bb(dynamic_cast<BasicBlock*>(instr->get_operand(1)));
            cfg.set_jump_bb(dynamic_cast<BasicBlock*>(instr->get_operand(2)));
        }
    }
    else {
        bb_end_store(cur_bb);
        string br_name = "." + cur_func->get_name() + "_" + instr->get_operand(0)->get_name();
        gen_code("b " + br_name);
        cfg.set_jump_bb(dynamic_cast<BasicBlock*>(instr->get_operand(0)));
    }
}
void CodeGen::binary_assembly(Instruction* instr){
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    if(const_optim_flag&&is_const(lhs)&&is_const(rhs)){
        switch(instr->get_instr_type()){
            case Instruction::add: set_ival(instr, get_ival(lhs)+get_ival(rhs)); break;
            case Instruction::sub: set_ival(instr, get_ival(lhs)-get_ival(rhs)); break;
            case Instruction::mul: set_ival(instr, get_ival(lhs)*get_ival(rhs)); break;
            case Instruction::sdiv: set_ival(instr, get_ival(lhs)/get_ival(rhs)); break;
            case Instruction::fadd: set_fval(instr, get_fval(lhs)+get_fval(rhs)); break;
            case Instruction::fsub: set_fval(instr, get_fval(lhs)-get_fval(rhs)); break;
            case Instruction::fmul: set_fval(instr, get_fval(lhs)*get_fval(rhs)); break;
            case Instruction::fdiv: set_fval(instr, get_fval(lhs)/get_fval(rhs)); break;
            default:
                assert(false);
        }
        return;
    }
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
            assert(false);
    }
    auto res = AllocaReg(instr);
    gen_code(assem_inst + " " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
    UpdateReg(res, instr);
    DefineStore(instr, res);
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
            gen_code("ld.w " + reg->print() + ", " + addr->print() + ", 0");
        else if(instr->get_type()->is_float_type())
            gen_code("fld.s " + reg->print() + ", " + addr->print() + ", 0");
        else assert(false);
    }
    else{
        string inst_tail;
        string inst_head;
        if(instr->get_type()->is_integer_type()) inst_head = "ld.w ";
        else if(instr->get_type()->is_float_type()) inst_head = "fld.s ";
        else assert(false);
        if(LocalOff.find(pt) != LocalOff.end()) inst_tail = ", $fp, " + std::to_string(LocalOff[pt]);
        else if(is_used(pt)) {auto addr = GetReg(pt);inst_tail = ", " + addr->print() + ", 0";}
        else assert(false);
        reg = AllocaReg(instr);
        gen_code(inst_head + reg->print() + inst_tail);
    }
    UpdateReg(reg, instr);
    DefineStore(instr, reg);
}
void CodeGen::store_assembly(Instruction* instr){
    Value* data = instr->get_operand(0);
    Value* pt = instr->get_operand(1);
    auto reg = GetReg(data);
    auto type = pt->get_type()->get_pointer_element_type();
    if(dynamic_cast<GlobalVariable*>(pt)){
        auto addr = GetReg(pt);
        if(type->is_integer_type())
            gen_code("st.w " + reg->print() + ", " + addr->print() + ", 0");
        else if(type->is_float_type())
            gen_code("fst.s " + reg->print() + ", " + addr->print() + ", 0");
        else assert(false);
    }
    else{
        string inst_tail;
        string inst_head;
        if(type->is_integer_type()) inst_head = "st.w ";
        else if(type->is_float_type()) inst_head = "fst.s ";
        else assert(false);
        if(LocalOff.find(pt) != LocalOff.end()) inst_tail = ", $fp, " + std::to_string(LocalOff[pt]);
        else if(is_used(pt)) {auto addr = GetReg(pt);inst_tail = ", " + addr->print() + ", 0";}
        else assert(false);
        gen_code(inst_head + reg->print() + inst_tail);
    }
}
void CodeGen::cmp_assembly(Instruction* instr, Instruction* refer){
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    if(const_optim_flag&&is_const(lhs)&&is_const(rhs)){
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
            case CmpInst::EQ: set_ival(instr, get_ival(lhs)==get_ival(rhs));break;
            case CmpInst::NE: set_ival(instr, get_ival(lhs)!=get_ival(rhs));break;
            case CmpInst::GT: set_ival(instr, get_ival(lhs)>get_ival(rhs));break;
            case CmpInst::GE: set_ival(instr, get_ival(lhs)>=get_ival(rhs));break;
            case CmpInst::LT: set_ival(instr, get_ival(lhs)<get_ival(rhs));break;
            case CmpInst::LE: set_ival(instr, get_ival(lhs)<=get_ival(rhs));break;
        }
        if(refer->is_br()){
            bb_end_store(cur_bb);
            string br1_name = "." + cur_func->get_name() + "_" + refer->get_operand(1)->get_name();
            string br2_name = "." + cur_func->get_name() + "_" + refer->get_operand(2)->get_name();
            if(get_ival(instr)!=0)gen_code("b " + br1_name);
            else gen_code("b " + br2_name);
            if(get_ival(instr)!=0)cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(1)));
            else cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(2)));
        }
        else if(refer->is_zext()){
            set_ival(refer, get_ival(instr));
        }
        else assert(false);
        return;
    }
    auto l_reg = GetReg(lhs);
    auto r_reg = GetReg(rhs);
    if(refer->is_br()){
        bb_end_store(cur_bb);
        string br1_name = "." + cur_func->get_name() + "_" + refer->get_operand(1)->get_name();
        string br2_name = "." + cur_func->get_name() + "_" + refer->get_operand(2)->get_name();
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
            case CmpInst::EQ: gen_code("beq " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              gen_code("b " + br2_name);
                              break;
            case CmpInst::NE: gen_code("bne " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              gen_code("b " + br2_name);
                              break;
            case CmpInst::GT: gen_code("bge " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              gen_code("b " + br2_name);
                              break;
            case CmpInst::GE: gen_code("blt " + l_reg->print() + ", " + r_reg->print() + ", " + br2_name);
                              gen_code("b " + br1_name);
                              break;
            case CmpInst::LT: gen_code("blt " + l_reg->print() + ", " + r_reg->print() + ", " + br1_name);
                              gen_code("b " + br2_name);
                              break;
            case CmpInst::LE: gen_code("bge " + l_reg->print() + ", " + r_reg->print() + ", " + br2_name);
                              gen_code("b " + br1_name);
                              break;
        }
        switch(dynamic_cast<CmpInst *>(instr)->get_cmp_op()){
            case CmpInst::GE:
            case CmpInst::LE:cfg.set_cj_bb(dynamic_cast<BasicBlock*>(refer->get_operand(2)));
                             cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(1)));
                             break;
            default:cfg.set_cj_bb(dynamic_cast<BasicBlock*>(refer->get_operand(1)));
                    cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(2)));
                    break; 
        }
    }
    else if(refer->is_zext()){
        auto res = AllocaReg(instr);
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
            case CmpInst::EQ: gen_code("xor " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              gen_code("sltui " + res->print() + ", " + res->print() + ", 1");
                              break;
            case CmpInst::NE: gen_code("xor " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              gen_code("sltu " + res->print() + ", " + "$zero, " + res->print());
                              break;
            case CmpInst::GT: gen_code("slt " + res->print() + ", " + r_reg->print() + ", " + l_reg->print());
                              break;
            case CmpInst::GE: gen_code("slt " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              gen_code("xori " + res->print() + ", " + res->print() + ", 1");
                              break;
            case CmpInst::LT: gen_code("slt " + res->print() + ", " + l_reg->print() + ", " + r_reg->print());
                              break;
            case CmpInst::LE: gen_code("slt " + res->print() + ", " + r_reg->print() + ", " + l_reg->print());
                              gen_code("xori " + res->print() + ", " + res->print() + ", 1");
                              break;
        }
        UpdateReg(res, instr);
        DefineStore(instr, res);
    }
    else assert(false);
}
void CodeGen::fcmp_assembly(Instruction* instr, Instruction* refer){
    auto lhs = instr->get_operand(0);
    auto rhs = instr->get_operand(1);
    if(const_optim_flag&&is_const(lhs)&&is_const(rhs)){
        switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
            case CmpInst::EQ: set_ival(instr, get_fval(lhs)==get_fval(rhs));break;
            case CmpInst::NE: set_ival(instr, get_fval(lhs)!=get_fval(rhs));break;
            case CmpInst::GT: set_ival(instr, get_fval(lhs)>get_fval(rhs));break;
            case CmpInst::GE: set_ival(instr, get_fval(lhs)>=get_fval(rhs));break;
            case CmpInst::LT: set_ival(instr, get_fval(lhs)<get_fval(rhs));break;
            case CmpInst::LE: set_ival(instr, get_fval(lhs)<=get_fval(rhs));break;
        }
        if(refer->is_br()){
            bb_end_store(cur_bb);
            string br1_name = "." + cur_func->get_name() + "_" + refer->get_operand(1)->get_name();
            string br2_name = "." + cur_func->get_name() + "_" + refer->get_operand(2)->get_name();
            if(get_ival(instr)!=0)gen_code("b " + br1_name);
            else gen_code("b " + br2_name);
            if(get_ival(instr)!=0)cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(1)));
            else cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(2)));
        }
        else if(refer->is_zext()){
            set_ival(refer, get_ival(instr));
        }
        else assert(false);
        return;
    }
    auto l_reg = GetReg(lhs);
    auto r_reg = GetReg(rhs);
    switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
        case FCmpInst::EQ: gen_code("fcmp.ceq.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
        case FCmpInst::NE: gen_code("fcmp.cne.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
        case FCmpInst::GT: gen_code("fcmp.clt.s $fcc0, " + r_reg->print() + ", " + l_reg->print());break;
        case FCmpInst::GE: gen_code("fcmp.cle.s $fcc0, " + r_reg->print() + ", " + l_reg->print());break;
        case FCmpInst::LT: gen_code("fcmp.clt.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
        case FCmpInst::LE: gen_code("fcmp.cle.s $fcc0, " + l_reg->print() + ", " + r_reg->print());break;
    }
    if(refer->is_br()){
        string br1_name = "." + cur_func->get_name() + "_" + refer->get_operand(1)->get_name();
        string br2_name = "." + cur_func->get_name() + "_" + refer->get_operand(2)->get_name();
        bb_end_store(cur_bb);
        gen_code("bceqz $fcc0, " + br2_name);
        gen_code("b " + br1_name);
        cfg.set_cj_bb(dynamic_cast<BasicBlock*>(refer->get_operand(2)));
        cfg.set_jump_bb(dynamic_cast<BasicBlock*>(refer->get_operand(1)));
    }
    else if(refer->is_zext()){
        string br = "." + cur_func->get_name() + "_" + instr->get_name();
        auto reg = AllocaReg(refer);
        gen_code("addi.w " + reg->print() + ", $zero" + ", 1");
        gen_code("bceqz $fcc0, " + br);
        gen_code("addi.w " + reg->print() + ", $zero" + ", 0");
        gen_code(br + ":");
        UpdateReg(reg, instr);
        DefineStore(instr, reg);
    }
    else assert(false);
}
void CodeGen::phi_assembly(Instruction* instr){
    auto reg = AllocaReg(instr);
    int off = CurOff(instr);
    assert(off!=-1);
    // 判断整型还是浮点
    if(instr->get_type()->is_integer_type())
        gen_code("ld.w " + reg->print() + ", $fp, " + std::to_string(off));
    else if(instr->get_type()->is_float_type())
        gen_code("fld.s " + reg->print() + ", $fp, " + std::to_string(off));
    else assert(false);
    UpdateReg(reg, instr);
}
void CodeGen::call_assembly(Instruction* instr){
    // first store the value of temporary registers and arguments registers
    call_used = true;
    if(inline_func.find(instr->get_operand(0))!=inline_func.end()){
        vector<Value*> real_args;
        for(unsigned int i=1;i<instr->get_num_operand();i++)real_args.push_back(instr->get_operand(i));
        std::shared_ptr<InlineFunc> in_func = inline_func[instr->get_operand(0)];
        if(in_func->run(real_args)){
            gen_code(in_func->gen_inline_code());
            if(instr->get_type()->is_integer_type())
                UpdateReg(&R[4], instr);
            else {UpdateReg(&FR[0], instr);UpdateReg(&R[4], nullptr);}
            return;
        }
    }
    for(int i=4;i<=20;i++){
        if(is_active_out(R[i].value, instr)&&!is_stored(R[i].value)){
            offset -= R[i].value->get_type()->get_size()+offset%R[i].value->get_type()->get_size();
            SetOff(R[i].value, offset);
            gen_code("st.w " + R[i].print() + ", $fp, " + std::to_string(offset));
        }
    }
    for(int i=0;i<=23;i++){
        if(is_active_out(FR[i].value, instr)&&!is_stored(FR[i].value)){
            offset -= FR[i].value->get_type()->get_size()+offset%FR[i].value->get_type()->get_size();
            SetOff(FR[i].value, offset);
            gen_code("fst.s " + FR[i].print() + ", $fp, " + std::to_string(offset));
        }
    }
    // second load the arguments (if the number of arguments is more than 8)store the extra arguments in the memory
    int i,j,off=0;
    for(i=0,j=0;(i+j+1)<instr->get_num_operand();){
        auto arg = instr->get_operand(i+j+1);
        if(arg->get_type()->is_float_type())
            if(j++<8){// 寄存器传参
                auto s_reg = GetReg(arg);
                if(&FR[j-1]!=s_reg)
                    gen_code("fmov.s " + FR[j-1].print() + ", " + s_reg->print());
                UpdateReg(&FR[j-1], arg);
            }
            else{//  堆栈传参
                gen_code("fst.s " + GetReg(arg)->print() + ", $sp, " + std::to_string(off));
                off += arg->get_type()->get_size();
            }   
        else if(i++<8){// 寄存器传参
                auto s_reg = GetReg(arg);
                if(&R[i+3]!=s_reg)
                    gen_code("or " + R[i+3].print() + ", " + s_reg->print() + ", $zero");
                UpdateReg(&R[i+3], arg);
            }
        else{   //  堆栈传参
            gen_code("st.w " + GetReg(arg)->print() + ", $sp, " + std::to_string(off));
            off += arg->get_type()->get_size();
        }
    }
    if(off>max_arg_size)max_arg_size=off;
    // third "bl " function_name
    gen_code("bl " + instr->get_operand(0)->get_name());
    // fourth mark the value of registers as null
    for(int i=4;i<=20;i++)UpdateReg(&R[i], nullptr);
    for(int i=0;i<=23;i++)UpdateReg(&FR[i], nullptr);
    if(instr->get_type()->is_integer_type()){
        UpdateReg(&R[4], instr);
        DefineStore(instr, &R[4]);
    }
    else if(instr->get_type()->is_float_type()){
        UpdateReg(&FR[0], instr);
        DefineStore(instr, &FR[0]);
    }
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
            gen_code("la.local " + addr->print() + ", " + pt->get_name());
            if(elem_off*elem_size)gen_code("addi.d " + addr->print() + ", " + addr->print() + ", " + std::to_string(elem_off*elem_size));
        }
        else {
            auto index_reg = GetReg(instr->get_operand(2));
            addr = AllocaReg(instr);
            UpdateReg(index_reg, nullptr);
            int elem_size = type->get_array_element_type()->get_size();
            gen_code("la.local " + addr->print() + ", " + pt->get_name());
            if(elem_size){
                gen_code("slli.w " + index_reg->print() + ", " + index_reg->print() + ", " + std::to_string((int)(log2(elem_size))));
                gen_code("add.d " + addr->print() + ", " + addr->print() + ", " + index_reg->print());
            }
        }
        UpdateReg(addr, instr);
    }
    else{
        assert(LocalOff.find(pt) != LocalOff.end() || is_used(pt));
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
                gen_code("slli.d " + index_reg->print() + ", " + index_reg->print() + ", " + std::to_string((int)(log2(elem_size))));
                gen_code("addi.d " + addr->print() + ", $fp, " + std::to_string(LocalOff[pt]));
                gen_code("add.d " + addr->print() + ", " + addr->print() + ", " + index_reg->print());
                UpdateReg(index_reg, nullptr);
                UpdateReg(addr, instr);
            }
        }
        else{
            auto reg = GetReg(pt);
            if(dynamic_cast<ConstantInt*>(instr->get_operand(1))){
                addr = AllocaReg(instr);
                int elem_off = dynamic_cast<ConstantInt*>(instr->get_operand(1))->get_value();
                int elem_size = pt->get_type()->get_pointer_element_type()->get_size();
                gen_code("addi.d " + addr->print() + ", " + reg->print() + ", " + std::to_string(elem_off*elem_size));
            }
            else{
                auto index_reg = GetReg(instr->get_operand(1));
                addr = AllocaReg(instr);
                UpdateReg(index_reg, nullptr);
                int elem_size = type->get_size();
                gen_code("slli.w " + index_reg->print() + ", " + index_reg->print() + ", " + std::to_string((int)(log2(elem_size))));
                gen_code("add.d " + addr->print() + ", " + reg->print() + ", " + index_reg->print());
            }
            UpdateReg(addr, instr);
            DefineStore(instr, addr);
        }
    }
}
void CodeGen::zext_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    if(!is_used(val)){
        auto cur_point = point;
        point = dynamic_cast<Instruction*>(val);
        if(dynamic_cast<Instruction*>(val)->is_cmp())
            cmp_assembly(dynamic_cast<Instruction*>(val), instr);
        else if(dynamic_cast<Instruction*>(val)->is_fcmp())
            fcmp_assembly(dynamic_cast<Instruction*>(val), instr);
        else assert(false);
        point = cur_point;
    }
    if(const_optim_flag&&is_const(val)){
        set_ival(instr, get_ival(val));
        return;
    }
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    gen_code("or " + res->print() + ", " + reg->print() + ", $zero");
    UpdateReg(res, instr);
    DefineStore(instr, res);
}
void CodeGen::fptosi_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto tmp_fr = AllocaTmpReg();
    auto res = AllocaReg(instr);
    gen_code("ftint.w.s " + reg->print() + ", " + reg->print());
    gen_code("movfr2gr.s " + res->print() + ", " + reg->print());
    UpdateReg(res, instr);
    DefineStore(instr, res);
}
void CodeGen::sitofp_assembly(Instruction* instr){
    auto val = instr->get_operand(0);
    auto reg = GetReg(val);
    auto res = AllocaReg(instr);
    gen_code("movgr2fr.w " + res->print() + ", " + reg->print());
    gen_code("ffint.s.w " + res->print() + ", " + res->print());
    UpdateReg(res, instr);
    DefineStore(instr, res);
}
Reg* CodeGen::GetReg(Value* v) {// 一定会返回存有此值的寄存器
    if(is_in_reg(v)) {// 如果寄存器存在此变量的值
        auto reg = CurReg(v);
        locked_regs.insert(reg);
        return reg;
    }
    else{
        if(is_const(v)) {// 常量
            if(is_integer(v)){// 整型常量
                int val = get_ival(v);
                if(val==0)return &R[0];
                else if(val>0&&val<=4095){// 可直接加载的小立即数
                    auto reg = AllocaReg(v);
                    gen_code("ori " + reg->print() + ", $zero, " + std::to_string(val));
                    UpdateReg(reg, v);
                    return reg;
                }
                else if(val>=-2048&&val<0){
                    auto reg = AllocaReg(v);
                    gen_code("addi.w " + reg->print() + ", $zero, " + std::to_string(val));
                    UpdateReg(reg, v);
                    return reg;
                }
                else{ // 不可直接加载的大数
                    auto reg = AllocaReg(v);
                    gen_code("lu12i.w " + reg->print() + ", " + std::to_string(val>>12));
                    if(val%4096)gen_code("ori " + reg->print() + ", " + reg->print() + ", " + std::to_string(val%4096));
                    UpdateReg(reg, v);
                    return reg;
                }
            }
            else{//浮点型常量
                auto tmp_r = AllocaTmpReg();
                auto reg = AllocaReg(v);
                float f_val = get_fval(v);
                int i_val = *(int*)&f_val;
                gen_code("lu12i.w " + tmp_r->print() + ", " + std::to_string(i_val>>12));
                if(i_val%4096)gen_code("ori " + tmp_r->print() + ", " + tmp_r->print() + ", " + std::to_string(i_val%4096));
                gen_code("movgr2fr.w " + reg->print() + ", " + tmp_r->print());
                UpdateReg(reg, v);
                return reg;
            }
        }
        else if(dynamic_cast<GlobalVariable*>(v)) {// 全局变量
            auto reg = AllocaReg(v);
            gen_code("la.local " + reg->print() + "," + v->get_name());
            UpdateReg(reg, v);
            return reg;
        }
        else { //局部变量
            assert(is_used(v)||LocalOff.find(v)!=LocalOff.end());
            auto reg = AllocaReg(v);
            if(is_used(v)){// 局部int或float型变量值或参数指针值
                int off = CurOff(v);
                assert(off!=-1);
                if(v->get_type()->is_float_type())
                    gen_code("fld.s " + reg->print() + ", $fp, " + std::to_string(off));
                else if(v->get_type()->is_pointer_type())
                    gen_code("ld.d " + reg->print() + ", $fp, " + std::to_string(off));
                else
                    gen_code("ld.w " + reg->print() + ", $fp, " + std::to_string(off));
            }
            else {//局部指针值
                int off = LocalOff[v];
                gen_code("addi.d " + reg->print() + ", $fp, " + std::to_string(off));
            }
            UpdateReg(reg, v);
            return reg;
        }
    }
    return RandomReg();

}
Reg* CodeGen::AllocaReg(Value* v){
    if(v->get_type()->is_float_type()){
        auto reg = LinearScanFR();
        locked_regs.insert(reg);
        return reg;
    }
    else {
        auto reg = LinearScanR();
        locked_regs.insert(reg);
        return reg;
    }
}
Reg* CodeGen::AllocaTmpReg(){
    for(int i=4;i<=20;i++)
        if(!is_active(R[i].value, point)&&locked_regs.find(&R[i])==locked_regs.end()){
            return &R[i];
        }
} 
Reg* CodeGen::AllocaTmpFReg(){
    for(int i=0;i<=23;i++)
        if(!is_active(FR[i].value, point)&&locked_regs.find(&FR[i])==locked_regs.end()){
            return &FR[i];
        }
}
Reg* LinearScanR(){
    for(int i=4;i<=20;i++)
        if(!is_active(R[i].value, point)&&locked_regs.find(&R[i])==locked_regs.end())return &R[i];
}
Reg* LinearScanFR(){
    for(int i=0;i<=23;i++)
        if(!is_active(FR[i].value, point)&&locked_regs.find(&FR[i])==locked_regs.end())return &FR[i];
}
Reg* RandomReg(){
    next_reg = (next_reg+1)%9;
    return &R[next_reg+12];
}
Reg* RandomFReg(){
    next_freg = (next_freg+1)%16;
    return &FR[next_freg+8];
}
void UpdateReg(Reg* r, Value* val){
    if (val) locked_regs.insert(r); 
    if (is_in_reg(val)) CurReg(val)->value = nullptr;
    if (r->value!=nullptr) SetReg(r->value, nullptr);
    r->value = val;
    if (val) SetReg(val, r);
}