//
// Created by sunteng on 2023/10/26.
//

#include "HelloWorldNew.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;



void HelloWorldNew::visitor(Function &F){
  errs() << "(llvm-tutor) HelloWorld from:" << F.getName() << "\n";
  errs() << "(llvm-tutor) number of arguments:" << F.arg_size() << "\n";
}

// Main entry point, takes IR unit to run the pass on (&F) and the
// corresponding pass manager (to be queried if need be)
PreservedAnalyses HelloWorldNew::run(llvm::Function &F,
                                     llvm::FunctionAnalysisManager &) {
  visitor(F);
  return llvm::PreservedAnalyses::all();
}




// New PM Registration

llvm::PassPluginLibraryInfo getHelloWorldNewPluginInfo(){

  return {
      LLVM_PLUGIN_API_VERSION,"HelloWorldNew",LLVM_VERSION_STRING,
      [](PassBuilder &PB){
        PB.registerPipelineParsingCallback(
            [](StringRef Name,FunctionPassManager &FPM,
               ArrayRef<PassBuilder::PipelineElement>){
              if(Name == "hello-world-new"){
                FPM.addPass(HelloWorldNew());
                return true;
              }
              return false;
            });

      }
  };

}



extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
  return getHelloWorldNewPluginInfo();
}

