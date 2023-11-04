//
// Created by sunteng on 2023/11/4.
//


#include "InjectFuncCall.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "inject-func-call"

//InjectFuncCall implementation

bool InjectFuncCall::runOnModule(Module &M){

  bool InsertedAtLeastOnePrintf = false;

  auto &CTX = M.getContext();

  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));

  FunctionType *PrintfTy = FunctionType::get(IntegerType::getInt32Ty(CTX),PrintfArgTy,true);

  FunctionCallee Printf = M.getOrInsertFunction("printf",PrintfTy);

  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());

  PrintfF->setDoesNotThrow();
  PrintfF->addParamAttr(0,Attribute::NoCapture);
  PrintfF->addParamAttr(0,Attribute::ReadOnly);



  llvm::Constant *PrintfFormatStr = llvm::ConstantDataArray::getString(CTX,"(llvm-tutor) Hello from:%s\n"
                                                                            "(llvm-tutor) number of arguments:%d \n");

  Constant *PrintfFormatStrVar = M.getOrInsertGlobal("PrintfFormatStr",PrintfFormatStr->getType());

  dyn_cast<GlobalVariable>(PrintfFormatStrVar)->setInitializer(PrintfFormatStr);



  for(auto &F : M){

    if(F.isDeclaration())
      continue;

    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    auto FuncName = Builder.CreateGlobalStringPtr(F.getName());

    llvm::Value *FormatStrPtr = Builder.CreatePointerCast(PrintfFormatStrVar,PrintfArgTy,"formatStr");

    LLVM_DEBUG(dbgs() << " Injecting call to printf inside " << F.getName() << "\n");

    Builder.CreateCall(Printf,{FormatStrPtr,FuncName,Builder.getInt32(F.arg_size())});

    InsertedAtLeastOnePrintf = true;
  }


  return InsertedAtLeastOnePrintf;

}

PreservedAnalyses InjectFuncCall::run(llvm::Module &M,
                                      llvm::ModuleAnalysisManager &){
  bool Changed = runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all());

}

llvm::PassPluginLibraryInfo getInjectFuncCallPluginInfo(){
  return {LLVM_PLUGIN_API_VERSION,"inject-func-call",LLVM_VERSION_STRING,
        [](PassBuilder &PB){PB.registerPipelineParsingCallback([](StringRef Name,ModulePassManager &MPM,ArrayRef<PassBuilder::PipelineElement>){
                                if(Name == "inject-func-call"){
                                  MPM.addPass(InjectFuncCall());
                                  return true;
                                }
                                return false;
                              });
        }
  };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
    llvmGetPassPluginInfo(){
  return getInjectFuncCallPluginInfo();
}

