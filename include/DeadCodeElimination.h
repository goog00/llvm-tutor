//
// Created by sunteng on 2023/10/26.
//

#ifndef LLVM_TUTOR_DEADCODEELIMINATION_H
#define LLVM_TUTOR_DEADCODEELIMINATION_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

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

struct DeadCodeElimination : public llvm::PassInfoMixin<DeadCodeElimination>{
  //pass 标志 Legacy PM interface
//  static char ID;
//  DeadCodeElimination() : FunctionPass(ID){
//    initializeDeadCodeEliminationPass(*PassRegistry::getPassRegistry());
//  }

  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &);

  bool runOnFunction(llvm::Function &F);

  void getAnalysisUsage(llvm::AnalysisUsage &Au) ;

  static bool isRequired() {return true;}

};

#endif // LLVM_TUTOR_DEADCODEELIMINATION_H
