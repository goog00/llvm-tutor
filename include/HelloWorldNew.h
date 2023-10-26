//
// Created by sunteng on 2023/10/26.
//

#ifndef LLVM_TUTOR_HELLOWORLDNEW_H
#define LLVM_TUTOR_HELLOWORLDNEW_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
// New PM implementation
struct HelloWorldNew : public llvm::PassInfoMixin<HelloWorldNew> {

  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &);


  void visitor(llvm::Function &F);



  static bool isRequired() {return true;}
};

#endif // LLVM_TUTOR_HELLOWORLDNEW_H
