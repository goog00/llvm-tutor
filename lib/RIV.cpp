//=============================================================================
// FILE:
//    RIV.cpp
//
// DESCRIPTION:
//    For every basic block  in the input function, this pass creates a list of
//    integer values reachable from that block. It uses the results of the
//    DominatorTree pass.
//
// ALGORITHM:
//    -------------------------------------------------------------------------
//    v_N = set of integer values defined in basic block N (BB_N)
//    RIV_N = set of reachable integer values for basic block N (BB_N)
//    -------------------------------------------------------------------------
//    STEP 1:
//    For every BB_N in F:
//      compute v_N and store it in DefinedValuesMap
//    -------------------------------------------------------------------------
//    STEP 2:
//    Compute the RIVs for the entry block (BB_0):
//      RIV_0 = {input args, global vars}
//    -------------------------------------------------------------------------
//    STEP 3: Traverse the CFG and for every BB_M that BB_N dominates,
//    calculate RIV_M as follows:
//      RIV_M = {RIV_N, v_N}
//    -------------------------------------------------------------------------
//
// REFERENCES:
//    Based on examples from:
//    "Building, Testing and Debugging a Simple out-of-tree LLVM Pass", Serge
//    Guelton and Adrien Guinet, LLVM Dev Meeting 2015
//
// License: MIT
//=============================================================================

//LLVM pass 是指在LLVM编译器中执行的一种算法或处理步骤。它们通常用于在编译过程中执行各种分析和转换。
//在你提到的RIV（Reachable Integer Values）pass中，它对输入函数中的每个基本块（basic block）进行计算，得出可达整数值（reachable integer values）的集合。
//换句话说，它找出在每个基本块中可以使用的整数值。这个pass在LLVM的中间表示（Intermediate Representation，IR）层面上运作，所以它会考虑到所有具有整数类型的值，包括布尔值。
//在LLVM的IR中，布尔值被表示为1位宽的整数（即i1）。
//这个pass展示了如何在LLVM中从其他分析pass获取结果。它依赖于Dominator Tree分析pass来获取基本块的支配树（dominance tree）。
//支配树是一种数据结构，用于表示程序中基本块的执行顺序。它对于一些优化和代码分析任务非常重要。
//总的来说，RIV pass的作用是分析给定函数中的每个基本块，并确定哪些整数值可以在该基本块中使用。这对于理解和改进代码可能很有用，例如在寻找潜在的溢出错误或优化计算等方面。


#include "RIV.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Format.h"

#include <deque>

using namespace llvm;

//类型别名：使用using关键字可以为类型定义一个别名

// DominatorTree node types used in RIV. One could use auto instead, but IMO
// being verbose makes it easier to follow.
using NodeTy = DomTreeNodeBase<llvm::BasicBlock> *;
// A map that a basic block BB holds a set of pointers to values defined in BB.
using DefValMapTy = RIV::Result;

// Pretty-prints the result of this analysis
static void printRIVResult(llvm::raw_ostream &OutS, const RIV::Result &RIVMap);

//-----------------------------------------------------------------------------
// RIV Implementation
//-----------------------------------------------------------------------------
RIV::Result RIV::buildRIV(Function &F, NodeTy CFGRoot) {
  Result ResultMap;

  //双端队列
  // Initialise a double-ended queue that will be used to traverse all BBs in F
  std::deque<NodeTy> BBsToProcess;
  BBsToProcess.push_back(CFGRoot);

  // STEP 1: For every basic block BB compute the set of integer values defined
  // in BB
  DefValMapTy DefinedValuesMap;
  for (BasicBlock &BB : F) {
    auto &Values = DefinedValuesMap[&BB];
    for (Instruction &Inst : BB)
      if (Inst.getType()->isIntegerTy())
        Values.insert(&Inst);
  }

  // STEP 2: Compute the RIVs for the entry BB. This will include global
  // variables and input arguments.
  auto &EntryBBValues = ResultMap[&F.getEntryBlock()];

#if LLVM_VERSION_MAJOR >= 17
  for (auto &Global : F.getParent()->globals())
#else
  for (auto &Global : F.getParent()->getGlobalList())
#endif
    if (Global.getValueType()->isIntegerTy())
      EntryBBValues.insert(&Global);

  for (Argument &Arg : F.args())
    if (Arg.getType()->isIntegerTy())
      EntryBBValues.insert(&Arg);

  // STEP 3: Traverse the CFG for every BB in F calculate its RIVs
  while (!BBsToProcess.empty()) {
    auto *Parent = BBsToProcess.back();
    BBsToProcess.pop_back();

    // Get the values defined in Parent
    auto &ParentDefs = DefinedValuesMap[Parent->getBlock()];
    // Get the RIV set of for Parent
    // (Since RIVMap is updated on every iteration, its contents are likely to
    // be moved around when resizing. This means that we need a copy of it
    // (i.e. a reference is not sufficient).
    llvm::SmallPtrSet<llvm::Value *, 8> ParentRIVs =
        ResultMap[Parent->getBlock()];

    // Loop over all BBs that Parent dominates and update their RIV sets
    for (NodeTy Child : *Parent) {
      BBsToProcess.push_back(Child);
      auto ChildBB = Child->getBlock();

      // Add values defined in Parent to the current child's set of RIV
      ResultMap[ChildBB].insert(ParentDefs.begin(), ParentDefs.end());

      // Add Parent's set of RIVs to the current child's RIV
      ResultMap[ChildBB].insert(ParentRIVs.begin(), ParentRIVs.end());
    }
  }

  return ResultMap;
}

RIV::Result RIV::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
  DominatorTree *DT = &FAM.getResult<DominatorTreeAnalysis>(F);
  Result Res = buildRIV(F, DT->getRootNode());

  return Res;
}

PreservedAnalyses RIVPrinter::run(Function &Func,
                                  FunctionAnalysisManager &FAM) {

  auto RIVMap = FAM.getResult<RIV>(Func);

  printRIVResult(OS, RIVMap);
  return PreservedAnalyses::all();
}

bool LegacyRIV::runOnFunction(llvm::Function &F) {
  // Clear the results from previous runs.
  RIVMap.clear();

  // Get the entry node for the CFG for the input function
  NodeTy Root =
      getAnalysis<DominatorTreeWrapperPass>().getDomTree().getRootNode();

  RIVMap = Impl.buildRIV(F, Root);

  return false;
}

void LegacyRIV::print(raw_ostream &out, Module const *) const {
  printRIVResult(out, RIVMap);
}

// This method defines how this pass interacts with other passes
void LegacyRIV::getAnalysisUsage(AnalysisUsage &AU) const {
  // Request the results from Dominator Tree Pass
  AU.addRequired<DominatorTreeWrapperPass>();
  // We do not modify the input module
  AU.setPreservesAll();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
AnalysisKey RIV::Key;

llvm::PassPluginLibraryInfo getRIVPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "riv", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #1 REGISTRATION FOR "opt -passes=print<riv>"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<riv>") {
                    FPM.addPass(RIVPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            // #2 REGISTRATION FOR "FAM.getResult<RIV>(Function)"
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return RIV(); });
                });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getRIVPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
char LegacyRIV::ID = 0;

// #1 REGISTRATION FOR "opt -analyze -legacy-riv"
static RegisterPass<LegacyRIV> X(/*PassArg=*/"legacy-riv",
                                 /*Name=*/"Compute Reachable Integer values",
                                 /*CFGOnly=*/true,
                                 /*is_analysis*/ true);

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
static void printRIVResult(raw_ostream &OutS, const RIV::Result &RIVMap) {
  OutS << "=================================================\n";
  OutS << "LLVM-TUTOR: RIV analysis results\n";
  OutS << "=================================================\n";

  const char *Str1 = "BB id";
  const char *Str2 = "Reachable Ineger Values";
  OutS << format("%-10s %-30s\n", Str1, Str2);
  OutS << "-------------------------------------------------\n";

  const char *EmptyStr = "";

  for (auto const &KV : RIVMap) {
    std::string DummyStr;
    raw_string_ostream BBIdStream(DummyStr);
    KV.first->printAsOperand(BBIdStream, false);
    OutS << format("BB %-12s %-30s\n", BBIdStream.str().c_str(), EmptyStr);
    for (auto const *IntegerValue : KV.second) {
      std::string DummyStr;
      raw_string_ostream InstrStr(DummyStr);
      IntegerValue->print(InstrStr);
      OutS << format("%-12s %-30s\n", EmptyStr, InstrStr.str().c_str());
    }
  }

  OutS << "\n\n";
}
