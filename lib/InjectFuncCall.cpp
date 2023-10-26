//========================================================================
// FILE:
//    InjectFuncCall.cpp
//
// DESCRIPTION:
//    For each function defined in the input IR module, InjectFuncCall inserts
//    a call to printf (from the C standard I/O library). The injected IR code
//    corresponds to the following function call in ANSI C:
//    ```C
//      printf("(llvm-tutor) Hello from: %s\n(llvm-tutor)   number of arguments: %d\n",
//             FuncName, FuncNumArgs);
//    ```
//    This code is inserted at the beginning of each function, i.e. before any
//    other instruction is executed.
//
//    To illustrate, for `void foo(int a, int b, int c)`, the code added by InjectFuncCall
//    will generated the following output at runtime:
//    ```
//    (llvm-tutor) Hello World from: foo
//    (llvm-tutor)   number of arguments: 3
//    ```
//
// USAGE:
//    1. Legacy pass manager:
//      $ opt -load <BUILD_DIR>/lib/libInjectFuncCall.so `\`
//        --legacy-inject-func-call <bitcode-file>
//    2. New pass maanger:
//      $ opt -load-pass-plugin <BUILD_DIR>/lib/libInjectFunctCall.so `\`
//        -passes=-"inject-func-call" <bitcode-file>
//
// License: MIT
//========================================================================
#include "InjectFuncCall.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "inject-func-call"

//-----------------------------------------------------------------------------
// InjectFuncCall implementation
//-----------------------------------------------------------------------------
bool InjectFuncCall::runOnModule(Module &M) {
  bool InsertedAtLeastOnePrintf = false;

  auto &CTX = M.getContext();
  //指针类型
  PointerType *PrintfArgTy = PointerType::getUnqual(Type::getInt8Ty(CTX));

  // STEP 1: Inject the declaration of printf
  // 创建一个名为“printf”的函数，这个函数接受一个可变参数的整数作为输入，然后返回一个整数。同时设置了该函数的一些属性，以确保它可以正确地被调用。
  // ----------------------------------------
  // Create (or _get_ in cases where it's already available) the following
  // declaration in the IR module:
  //    declare i32 @printf(i8*, ...)
  // It corresponds to the following C declaration:
  //    int printf(char *, ...)
  //这行代码创建了一个函数类型（FunctionType）。
  // 这个函数接受一个整数类型（通过IntegerType::getInt32Ty(CTX)获取）作为返回类型，getInt32Ty(CTX) 表示在给定的 context（CTX）中获取一个32位整数类型。
  // 参数类型为PrintfArgTy（这是一个已知的类型，可能在代码的其他部分定义），并且这个函数是可变参数的（IsVarArgs=true）。
  FunctionType *PrintfTy = FunctionType::get(
      IntegerType::getInt32Ty(CTX),
      PrintfArgTy,
      /*IsVarArgs=*/true);

  //这行代码在模块（M）中查找一个名为“printf”的函数，如果找不到，则插入一个新函数。这个新函数的类型是前面定义的PrintfTy。
  // 这样做的目的是在模块中创建一个函数调用者，可以用来在其他地方引用这个函数。
  FunctionCallee Printf = M.getOrInsertFunction("printf", PrintfTy);

  // Set attributes as per inferLibFuncAttributes in BuildLibCalls.cpp
  //这行代码将Printf调用者的类型转换为Function。dyn_cast是一种动态类型转换，它尝试将对象转换为指定的类型。如果转换成功，则返回转换后的对象；如果转换失败，则返回空指针。
  Function *PrintfF = dyn_cast<Function>(Printf.getCallee());
  //这行代码设置函数PrintfF为不抛出异常。这意味着调用这个函数不会导致异常被抛出。
  PrintfF->setDoesNotThrow();
  //这行代码为函数PrintfF的第一个参数设置一个属性，即NoCapture。这个属性表示该参数不会被捕获（即不会被存储在栈上）。
  PrintfF->addParamAttr(0, Attribute::NoCapture);
  //这行代码为函数PrintfF的第一个参数设置另一个属性，即ReadOnly。这个属性表示该参数是只读的（即不能被修改）。
  PrintfF->addParamAttr(0, Attribute::ReadOnly);


  // STEP 2: Inject a global variable that will hold the printf format string
  // ------------------------------------------------------------------------
  //创建了一个ConstantDataArray类型的对象PrintfFormatStr，这个对象是一个字符串常量，内容是"(llvm-tutor) Hello from: %s\n(llvm-tutor) number of arguments: %d\n"。
  // 在Printf函数中，%s和%d是格式化参数的占位符。
  llvm::Constant *PrintfFormatStr = llvm::ConstantDataArray::getString(
      CTX, "(llvm-tutor) Hello from: %s\n(llvm-tutor)   number of arguments: %d\n");

  //这行代码在模块M中查找一个名为PrintfFormatStr的全局变量，如果找不到，就插入一个新的全局变量。这个全局变量的类型是PrintfFormatStr的类型的。
  Constant *PrintfFormatStrVar =
      M.getOrInsertGlobal("PrintfFormatStr", PrintfFormatStr->getType());
  //这行代码将PrintfFormatStrVar动态转换为GlobalVariable类型，然后设置这个全局变量的初始化为前面创建的字符串常量PrintfFormatStr。
  dyn_cast<GlobalVariable>(PrintfFormatStrVar)->setInitializer(PrintfFormatStr);

  // STEP 3: For each function in the module, inject a call to printf
  // ----------------------------------------------------------------
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    // Get an IR builder. Sets the insertion point to the top of the function
    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    // Inject a global variable that contains the function name
    auto FuncName = Builder.CreateGlobalStringPtr(F.getName());

    // Printf requires i8*, but PrintfFormatStrVar is an array: [n x i8]. Add
    // a cast: [n x i8] -> i8*
    //这段代码的主要目的是将一个类型为数组的常量值转换为 printf 函数需要的特定类型（一个指向字节的指针）
    //这行声明了一个 llvm::Value 指针变量 FormatStrPtr。在LLVM中，llvm::Value 是所有类型值的基类。
    //这行代码使用了一个 LLVMBuilder 对象（通常命名为 Builder）的 CreatePointerCast 方法。这个方法用于将一种类型的指针转换为另一种类型的指针。
    //这里，它正在将 PrintfFormatStrVar（一个数组类型）转换为 PrintfArgTy 类型（一个 i8* 类型，也就是一个指向字节的指针）。这个转换会创建一个新的 llvm::Value 对象。
    //formatStr 这是一个字符串，它为新创建的指针提供了一个名字，以便在调试或日志记录中使用。这个名字在这个特定的上下文中可能并不重要，但它在某些情况下可能会很有用。
    llvm::Value *FormatStrPtr =
        Builder.CreatePointerCast(PrintfFormatStrVar, PrintfArgTy, "formatStr");

    // The following is visible only if you pass -debug on the command line
    // *and* you have an assert build.
    LLVM_DEBUG(dbgs() << " Injecting call to printf inside " << F.getName()
                      << "\n");

    // Finally, inject a call to printf
    //这是LLVM的IR构建器的一个方法，用于创建一个函数调用。
    //这是被调用的函数的名称，也就是printf。
    //这是被调用的函数的参数列表。其中，FormatStrPtr是格式化字符串的指针，FuncName函数名，F.arg_size()可能是函数参数的数量。
    Builder.CreateCall(
        Printf, {FormatStrPtr, FuncName, Builder.getInt32(F.arg_size())});

    InsertedAtLeastOnePrintf = true;
  }

  return InsertedAtLeastOnePrintf;
}

PreservedAnalyses InjectFuncCall::run(llvm::Module &M,
                                       llvm::ModuleAnalysisManager &) {
  bool Changed =  runOnModule(M);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

bool LegacyInjectFuncCall::runOnModule(llvm::Module &M) {
  bool Changed = Impl.runOnModule(M);

  return Changed;
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getInjectFuncCallPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "inject-func-call", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "inject-func-call") {
                    MPM.addPass(InjectFuncCall());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getInjectFuncCallPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
char LegacyInjectFuncCall::ID = 0;

// Register the pass - required for (among others) opt
static RegisterPass<LegacyInjectFuncCall>
    X(/*PassArg=*/"legacy-inject-func-call", /*Name=*/"LegacyInjectFuncCall",
      /*CFGOnly=*/false, /*is_analysis=*/false);
