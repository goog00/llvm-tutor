//
// Created by sunteng on 2023/10/26.
//

#include "DeadCodeElimination.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"



#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"


using namespace llvm;


//Legacy
//char DeadCodeElimination::ID = 0;
//INITIALIZE_PASS(DeadCodeElimination,"DeadCodeElimination","my aggressive dead code elimination",false, false);

bool DeadCodeElimination::runOnFunction(Function& F){
//  if(skipOptnoneFunction(F))
//    return false;

  SmallPtrSet<Instruction*,32> Alive;
  SmallVector<Instruction*,128> Worklist;

  errs() << "开始收集指令" << "\n";
  //收集已知的根指令
  for(Instruction &I : instructions(F)){
    if(I.isDebugOrPseudoInst() || !I.isSafeToRemove()){
      errs() << "Alive.insert: " << I << "\n";
      Alive.insert(&I);
      Worklist.push_back(&I);
    }
  }

  errs() << "开始收集指令" << "\n";
  //向后传播生存性（liveness）
  while(!Worklist.empty()){
    Instruction *LiveInst = Worklist.pop_back_val();
    for(Use &OI : LiveInst->operands()){
      if(Instruction *Inst = dyn_cast<Instruction>(OI))
        if(Alive.insert(Inst).second)
          Worklist.push_back(Inst);
    }

  }
  errs() << "我在这" << "\n";
  //在这个Pass 中，不在生存集合中的指令被指令认为是无用的。
  // 不影响控制流、返回值，以及没有任何副作用的指令直接删除
  for(Instruction &I : instructions(F)){
    errs() << "#######" << I << "\n";
    if(Alive.count(&I)){
      errs() << "****" << I << "\n";
      Worklist.push_back(&I);
      I.dropAllReferences();
    }
  }

  for(Instruction *&I : Worklist){
    errs() << I << "\n";
    I->eraseFromParent();
  }

  return !Worklist.empty();

}


// Main entry point, takes IR unit to run the pass on (&F) and the
// corresponding pass manager (to be queried if need be)
PreservedAnalyses DeadCodeElimination::run(llvm::Function &F,
                                     llvm::FunctionAnalysisManager &) {
  runOnFunction(F);
  return llvm::PreservedAnalyses::all();
}

// New PM Registration
llvm::PassPluginLibraryInfo getDeadCodeEliminationPluginInfo(){

  return {
      LLVM_PLUGIN_API_VERSION,"DeadCodeElimination",LLVM_VERSION_STRING,
      [](PassBuilder &PB){
        PB.registerPipelineParsingCallback(
            [](StringRef Name,FunctionPassManager &FPM,
               ArrayRef<PassBuilder::PipelineElement>){
              if(Name == "dead-code-e"){
                FPM.addPass(DeadCodeElimination());
                return true;
              }
              return false;
            });

      }
  };

}

  void getAnalysisUsage(llvm::AnalysisUsage &AU) {
    AU.setPreservesCFG();
  }

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
  return getDeadCodeEliminationPluginInfo();
}

//FunctionPass *llvm::createMYAggressiveDCEPass(){
//  return new DeadCodeElimination();
//}