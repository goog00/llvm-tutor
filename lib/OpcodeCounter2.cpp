//
// Created by sunteng on 2023/11/4.
//

#include "OpcodeCount2.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

static void printOpcodeCounterResult(llvm::raw_ostream &,const ResultOpcodeCounter &OC);

llvm::AnalysisKey OpcodeCounter::Key;

OpcodeCounter::Result OpcodeCounter::generateOpcodeMap(llvm::Function &Func){

  //Result是在头文件中声明的map: llvm::StringMap<unsigned>;
  OpcodeCounter::Result OpcodeMap;

  for(auto &BB : Func){
    for(auto &Inst : BB){
      StringRef Name = Inst.getOpcodeName();

      if(OpcodeMap.find(Name) == OpcodeMap.end()){
        OpcodeMap[Inst.getOpcodeName()] = 1;
      } else {
        OpcodeMap[Inst.getOpcodeName] ++;
      }
    }
  }

  return OpcodeMap;
}


OpcodeCounter::Result OpcodeCounter::run(llvm::Function &Func,
                                         llvm::FunctionAnalysisManager &){

  return generateOpcodeMap(Func);
}


PreservedAnalyses OpcodeCounterPrinter::run(Function &Func,
                                            FunctionAnalysisManager &FAM){

  auto &OpcodeMap = FAM.getResult<OpcodeCounter>(Func);

  OS << "Printing analysis 'OpcodeCounter Pass' for function '"
     << Func.getName() << "':\n";

  printOpcodeCounterResult(OS,OpcodeMap);
  return PreservedAnalyses:all();

}


//New PM Registration


llvm::PassPlugiinLibraryInfo getOpcodeCounterPluginInfo(){
  return {
      LLVM_PLUGIN_API_VERSION,"OpcodeCounter",LLVM_VERSION_STRING,
        [](PassBuilder &PB){
          //#1
          PB.registerPipelineParsingCallback([&](StringRef Name,FunctionPassManager &FPM,
                                                 ArrayRef<PassBuilder::PipelineElement>){
            if(Name == "print<opcode-counter>"){
              FPM.addPass(OpcodeCounterPrinter(llvm::errs()));
              return true;
            }
            return false;
          });
          //#2
          PB.registerVectorizerStartEPCallback([](llvm::FunctionPassManager &PM,
                                                  llvm::OptimizationLevel level){

            PM.addPass(OpcodeCounterPrinter(llvm::errs()));
          });


          PB.registerAnalysisRegistrationCallback([](FunctionAnalysisManager &FAM){
            FAM.registerPass([&]{return OpcodeCounter();})
          });


    }



  };
}


extern "C" LLVM_ATTRIBUTE_WEAK :: llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
  return getOpcodeCounterPluginInfo();
}


static void printOpcodeCounterResult(raw_ostream &OutS, const ResultOpcodeCounter &OpcodeMap){

  OutS << "=========================\n"
  OutS << "LLVM-TUTOR: opcodecounter result \n";
  OutS << "===========================================\n";
  const char *str1 = "OPCODE";
  const char *str2 = "#TIME USED";

  OutS << format("%-20s %-10s\n",str1,str2);
  OutS << "-------------------------------------------\n";

  for(auto &Inst : OpcodeMap){
    OutS << format("%-20s %-10lu\n",Inst.first().str().c_str(),Inst.second);
  }

  OutS << "-----------------------------\n\n"



}