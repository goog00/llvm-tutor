//
// Created by sunteng on 2023/11/4.
//

#ifndef LLVM_TUTOR_INJECTFUNCCALL2_H
#define LLVM_TUTOR_INJECTFUNCCALL2_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

struct InjectFuncCall : public llvm::PassInfoMixin<InjectFuncCall>{

  llvm::PreservedAnalyses run(llvm::Moudle &M,
                              llvm::MoudleAnalysisManager &);


  bool runOnModule(llvm::Module &M);

  static bool isRequired(){return true;}
};

#endif // LLVM_TUTOR_INJECTFUNCCALL2_H
