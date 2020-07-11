//=============================================================================
// FILE:
//    Braids.cpp
//
// DESCRIPTION:
//    Prints the "Braids" of IR instructions present in each basic-blocks of a
//    function.
//
// USAGE:
//    1. Legacy PM
//      opt -load libBraids.dylib -legacy-braids -disable-output `\`
//        <input-llvm-file>
//    2. New PM
//      opt -load-pass-plugin=libBraids.dylib -passes="braids" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <vector>
#include <utility>

using namespace llvm;

//-----------------------------------------------------------------------------
// Braids implementation
//-----------------------------------------------------------------------------
namespace {

// This method implements what the pass does
void visitor(Function &F) {
  errs() << "\nFunction: "<< F.getName() << "\n";
  errs() << "  number of arguments: " << F.arg_size() << "\n";
  errs() << "  number of basic blocks: " << F.size() << "\n";

  // visit basic blocks
  for(BasicBlock& BB: F) {
    // build the database for color map
    std::map<Instruction*, int> cmap;
    int i = 0;
    errs() << "\n  Basic block (name=" << BB.getName() << ") has "
                 << BB.size() << " instructions.\n";
    for(Instruction &insn : BB) {
      errs() << "    " << insn << "\n";
      cmap[&insn] = -1;
      ++i;
    }
    // coloring
    int color = 0;
    std::vector<Instruction*> worklist;
    for(Instruction &insn : BB) {
      if(cmap[&insn] == -1) {
        worklist.push_back(&insn);
        cmap[&insn] = color;
        ++color;
      }
      while (!worklist.empty()) {
        auto node = worklist.back();
        worklist.pop_back();
        // visit parents
        for(Use &U : node->operands()) {
          if(Instruction* parent = dyn_cast<Instruction>(U.get())) {
            if(cmap[parent] == -1) {
              cmap[parent] = cmap[node];
              worklist.push_back(parent);
            }
          }
        }
        // visit children
        for(User* u : node->users()) {
          if(Instruction* child = dyn_cast<Instruction>(u)) {
            if(cmap[child] == -1) {
              cmap[child] = cmap[node];
              worklist.push_back(child);
            }
          }
        }
      }
    }
    errs() << "\n  Basic block (name=" << BB.getName() << ") has "
                 << color << " braids.\n";
    for(int i = 0; i < color; ++i) {
      for(Instruction &insn : BB) {
        if (cmap[&insn] == i)
          errs() << "    braid:" << i << " " << insn << "\n";
      }
    }
  }
}

//-----------------------------------------------------------------------------
// New PM implementation
//-----------------------------------------------------------------------------
struct Braids : PassInfoMixin<Braids> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    visitor(F);
    return PreservedAnalyses::all();
  }
};

// Legacy PM implementation
struct LegacyBraids : public FunctionPass {
  static char ID;
  LegacyBraids() : FunctionPass(ID) {}
  // Main entry point - the name conveys what unit of IR this is to be run on.
  bool runOnFunction(Function &F) override {
    visitor(F);
    // Doesn't modify the input unit of IR, hence 'false'
    return false;
  }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getBraidsPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Braids", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "braids") {
                    FPM.addPass(Braids());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize Braids when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getBraidsPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
// The address of this variable is used to uniquely identify the pass. The
// actual value doesn't matter.
char LegacyBraids::ID = 0;

// This is the core interface for pass plugins. It guarantees that 'opt' will
// recognize LegacyBraids when added to the pass pipeline on the command
// line, i.e.  via '--legacy-hello-world'
static RegisterPass<LegacyBraids>
    X("legacy-braids", "Braids",
      true, // This pass doesn't modify the CFG => true
      false // This pass is not a pure analysis pass => false
    );
