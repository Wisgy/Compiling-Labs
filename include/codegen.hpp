#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "Module.h"
#include "logging.hpp"

using std::string;
using std::vector;

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

  private:
    Module *m;
    vector<string> output;
};

#endif
