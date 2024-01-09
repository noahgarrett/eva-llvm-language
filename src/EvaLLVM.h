/**
 * Eva to LLVM IR compiler
*/

#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <string>
#include <regex>
#include <memory>
#include <map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "./parser/EvaParser.h"
#include "Logger.h"
#include "Environment.h"

using syntax::EvaParser;

/**
 * Environment Type
*/
using Env = std::shared_ptr<Environment>;

/**
 * Class Info. Contains struct type and field names
*/
struct ClassInfo {
    llvm::StructType* cls;
    llvm::StructType* parent;
    std::map<std::string, llvm::Type*> fieldsMap;
    std::map<std::string, llvm::Function*> methodsMap;
};

/**
 * Index of the vTable in the class fields
*/
static const size_t VTABLE_INDEX = 0;

/**
 * Each class has a set of reserved fields at the beginning of its layout.
 * Currently it's only the vTable used to resolve methods.
*/
static const size_t RESERVED_FIELDS_COUNT = 1;

// Generic Binary Operator
#define GEN_BINARY_OP(Op, varName)         \
  do {                                     \
    auto op1 = gen(expr.list[1], env);      \
    auto op2 = gen(expr.list[2], env);      \
    return builder->Op(op1, op2, varName); \
  } while (false);

class EvaLLVM {
    public:
        EvaLLVM() : parser(std::make_unique<EvaParser>()) { 
            moduleInit(); 
            setupExternalFunctions();
            setupGlobalEnvironment();
            setupTargetTriple();
        }
    
    void exec(const std::string& program) {
        // 1. Parse the program
        auto ast = parser->parse("(begin " + program + ")");

        // 2. Compile to LLVM IR
        compile(ast);
        
        // Print Generated code
        module->print(llvm::outs(), nullptr);

        // 3. Save module IR to file
        saveModuleToFile("./dist/out.ll");
    }

    private:
        /**
         * Compiles an expression
        */
        void compile(const Exp& ast) {
            // 1. Create main function:
            fn = createFunction(
                "main", 
                llvm::FunctionType::get(
                    /* return type */ builder->getInt32Ty(),
                    /* vararg */ false
                ),
                GlobalEnv
            );

            createGlobalVar("VERSION", builder->getInt32(42))->getInitializer();

            // 2. Compile main body
            gen(ast, GlobalEnv);

            builder->CreateRet(builder->getInt32(0));
        }

        /**
         * Main Compile Loop
        */
        llvm::Value* gen(const Exp& expr, Env env) {
            switch (expr.type) {
                // Numbers
                case ExpType::NUMBER: {
                    return builder->getInt32(expr.number);
                }

                // Strings
                case ExpType::STRING: {
                    // Unescape all special characters. TODO: support all chars or handle in parser
                    auto re = std::regex("\\\\n");
                    auto str = std::regex_replace(expr.string, re, "\n");

                    return builder->CreateGlobalStringPtr(str);
                }
                
                // Symbols
                case ExpType::SYMBOL: {
                    // Boolean
                    if (expr.string == "true" || expr.string == "false") {
                        return builder->getInt1(expr.string == "true" ? true : false);
                    } else {
                        // Variables
                        auto varName = expr.string;
                        auto value = env->lookup(varName);

                        // 1. Local Variables
                        if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(value)) {
                            return builder->CreateLoad(localVar->getAllocatedType(), localVar, varName.c_str());
                        }

                        // 2. Global Variables
                        else if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
                            return builder->CreateLoad(globalVar->getInitializer()->getType(), globalVar, varName.c_str());
                        }

                        // 3. Functions
                        else {
                            return value;
                        }
                    }
                }
                
                // Lists
                case ExpType::LIST: {
                    auto tag = expr.list[0];

                    // Special Cases
                    if (tag.type == ExpType::SYMBOL) {
                        auto op = tag.string;

                        // Binary Math Operations:
                        if (op == "+") {
                            GEN_BINARY_OP(CreateAdd, "tmpadd");
                        }
                        else if (op == "-") {
                            GEN_BINARY_OP(CreateSub, "tmpsub")
                        }
                        else if (op == "*") {
                            GEN_BINARY_OP(CreateMul, "tmpmul");
                        }
                        else if (op == "/") {
                            GEN_BINARY_OP(CreateSDiv, "tmpdiv");
                        }

                        // Comparison Operations (> 5 10):
                        else if (op == ">") {
                            GEN_BINARY_OP(CreateICmpUGT, "tmpcmp");
                        }
                        else if (op == "<") {
                            GEN_BINARY_OP(CreateICmpULT, "tmpcmp");
                        }
                        else if (op == "==") {
                            GEN_BINARY_OP(CreateICmpEQ, "tmpcmp");
                        }
                        else if (op == "!=") {
                            GEN_BINARY_OP(CreateICmpNE, "tmpcmp");
                        }
                        else if (op == ">=") {
                            GEN_BINARY_OP(CreateICmpUGE, "tmpcmp");
                        }
                        else if (op == "<=") {
                            GEN_BINARY_OP(CreateICmpULE, "tmpcmp");
                        }

                        // Branch Instructions:
                        /**
                         * (if <cond> <then> <else>)
                        */
                        else if (op == "if") {
                            // Compile <cond>
                            auto cond = gen(expr.list[1], env);

                            // Branch Blocks:

                            // Then-block appeneded right away
                            auto thenBlock = createBasicBlock("then", fn);

                            // Else-IfEnd blocks are appended later to handle nested if-expressions
                            auto elseBlock = createBasicBlock("else");
                            auto ifEndBlock = createBasicBlock("ifend");

                            // Condition Branch:
                            builder->CreateCondBr(cond, thenBlock, elseBlock);

                            // Then branch:
                            builder->SetInsertPoint(thenBlock);
                            auto thenRes = gen(expr.list[2], env);
                            builder->CreateBr(ifEndBlock);

                            // Result the block to handle nested if-expressions. This is needed for the `phi` instruction
                            thenBlock = builder->GetInsertBlock();

                            // Else branch:
                            // Append the block to the function now:
                            fn->getBasicBlockList().push_back(elseBlock);
                            builder->SetInsertPoint(elseBlock);
                            auto elseRes = gen(expr.list[3], env);
                            builder->CreateBr(ifEndBlock);
                            
                            elseBlock = builder->GetInsertBlock();

                            // Once the two blocks have finished, Head to the ending block
                            fn->getBasicBlockList().push_back(ifEndBlock);
                            builder->SetInsertPoint(ifEndBlock);

                            // Result of the If expression is `phi` instruction
                            auto phi = builder->CreatePHI(thenRes->getType(), 2, "tmpif");

                            phi->addIncoming(thenRes, thenBlock);
                            phi->addIncoming(elseRes, elseBlock);

                            return phi;
                        }

                        // While Loop (while <cond> <body>)
                        else if (op == "while") {
                            auto condBlock = createBasicBlock("cond", fn);
                            builder->CreateBr(condBlock);

                            // Body / While-End blocks
                            auto bodyBlock = createBasicBlock("body");
                            auto loopEndBlock = createBasicBlock("loopend");

                            // Compile <cond>
                            builder->SetInsertPoint(condBlock);
                            auto cond = gen(expr.list[1], env);

                            // Condition branch
                            builder->CreateCondBr(cond, bodyBlock, loopEndBlock);

                            // Body
                            fn->getBasicBlockList().push_back(bodyBlock);
                            builder->SetInsertPoint(bodyBlock);
                            gen(expr.list[2], env);
                            builder->CreateBr(condBlock);

                            fn->getBasicBlockList().push_back(loopEndBlock);
                            builder->SetInsertPoint(loopEndBlock);

                            return builder->getInt32(0);
                        }

                        // Function Declaration: (def <name> <params> <body>)
                        else if (op == "def") {
                            return compileFunction(expr, /* name */ expr.list[1].string, env);
                        }

                        // Variable declaration: (var x (+ y 10))
                        // Typed: (var (x number) 42)
                        // Note: Locals are allocated on the stack
                        if (op == "var") {
                            // Special case for class fields, which are already defined during class info allocation:
                            if (cls != nullptr) {
                                return builder->getInt32(0);
                            }

                            auto varNameDecl = expr.list[1];
                            auto varName = extractVarName(varNameDecl);

                            // Special case for `new` as it allocates a variable:
                            if (isNew(expr.list[2])) {
                                auto instance = createInstance(expr.list[2], env, varName);
                                return env->define(varName, instance);
                            }

                            // Initializer
                            auto init = gen(expr.list[2], env);

                            // Type:
                            auto varTy = extractVarType(varNameDecl);

                            // Variable:
                            auto varBinding = allocVar(varName, varTy, env);

                            // Set Value:
                            return builder->CreateStore(init, varBinding);
                        }

                        // Variable update: (set x 100)
                        // Property update: (set (prop self x) 100)
                        else if (op == "set") {
                            // Value:
                            auto value = gen(expr.list[2], env);
                            
                            // 1. Properties

                            // Special case for property writes:
                            if (isProp(expr.list[1])) {
                                auto instance = gen(expr.list[1].list[1], env);
                                auto fieldName = expr.list[1].list[2].string;
                                auto ptrName = std::string("p") + fieldName;

                                auto cls = (llvm::StructType*)(instance->getType()->getContainedType(0));
                                auto fieldIdx = getFieldIndex(cls, fieldName);
                                auto address = builder->CreateStructGEP(cls, instance, fieldIdx, ptrName);

                                builder->CreateStore(value, address);

                                return value;
                            }

                            // 2. Variables
                            else {
                                auto varName = expr.list[1].string;

                                // Variable:
                                auto varBinding = env->lookup(varName);

                                // Set value:
                                builder->CreateStore(value, varBinding);

                                return value;
                            }
                        }

                        // Blocks: (begin <expression>)
                        else if (op == "begin") {
                            // Block scope
                            auto blockEnv = std::make_shared<Environment>(std::map<std::string, llvm::Value*>{}, env);

                            // Compile each expression within the block.
                            // Result is the last evaluated expression.
                            llvm::Value* blockRes;

                            for (auto i = 1; i < expr.list.size(); i++) {
                                // Generate the expression code.
                                blockRes = gen(expr.list[i], blockEnv);
                            }

                            return blockRes;
                        }

                        // printf external function
                        // ( printf "Value: %d" 42)
                        else if (op == "printf") {
                            auto printfFn = module->getFunction("printf");

                            std::vector<llvm::Value*> args{};

                            for (auto i = 1; i < expr.list.size(); i++) {
                                args.push_back(gen(expr.list[i], env));
                            }

                            return builder->CreateCall(printfFn, args);
                        }

                        // Class Declaration: (class A <super> <body>)
                        else if (op == "class") {
                            auto name = expr.list[1].string;

                            auto parent = expr.list[2].string == "null" ? nullptr : getClassByName(expr.list[2].string);

                            // Currently compiling class
                            cls = llvm::StructType::create(*ctx, name);

                            // Super class data always sits at the first element
                            if (parent != nullptr) {
                                inheritClass(cls, parent);
                            } else {
                                classMap_[name] = {
                                    /* class */ cls,
                                    /* parent */ parent,
                                    /* fields */ {},
                                    /* methods */ {}
                                };
                            }

                            // Populate the class info with fields and methods
                            buildClassInfo(cls, expr, env);

                            // Compile the body:
                            gen(expr.list[3], env);

                            // Reset the class variable after compiling so normal functions do not pick the class name prefix
                            cls = nullptr;

                            return builder->getInt32(0);
                        }

                        // `new` Operator: (new <class> <args>)
                        else if (op == "new") {
                            return createInstance(expr, env, "");
                        }

                        // Prop access: (prop <instance> <name>)
                        else if (op == "prop") {
                            // Instance
                            auto instance = gen(expr.list[1], env);
                            auto fieldName = expr.list[2].string;
                            auto ptrName = std::string("p") + fieldName;

                            auto cls = (llvm::StructType*)(instance->getType()->getContainedType(0));

                            auto fieldIdx = getFieldIndex(cls, fieldName);

                            auto address = builder->CreateStructGEP(cls, instance, fieldIdx, ptrName);

                            return builder->CreateLoad(cls->getElementType(fieldIdx), address, fieldName);
                        }

                        // Method access: (method <instance> <name>) | (method (super <class>) <name>)
                        else if (op == "method") {
                            auto methodName = expr.list[2].string;

                            llvm::StructType* cls;
                            llvm::Value* vTable;
                            llvm::StructType* vTableTy;
                            
                            // 1. Load vTable
                            // (method (super <class>) <name>)
                            if (isSuper(expr.list[1])) {
                                auto className = expr.list[1].list[1].string;
                                cls = classMap_[className].parent;
                                auto parentName = std::string{cls->getName().data()};
                                vTable = module->getNamedGlobal(parentName + "_vTable");
                                vTableTy = llvm::StructType::getTypeByName(*ctx, parentName + "_vTable");
                            }

                            // (method <instance> <name>)
                            else {
                                auto instance = gen(expr.list[1], env);

                                cls = (llvm::StructType*)(instance->getType()->getContainedType(0));

                                auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX);
                                
                                vTable = builder->CreateLoad(cls->getElementType(VTABLE_INDEX), vTableAddr, "vt");

                                vTableTy = (llvm::StructType*)(vTable->getType()->getContainedType(0));
                            }

                            // 2. Load method from vTable
                            auto methodIdx = getMethodIndex(cls, methodName);
                            auto methodTy = (llvm::FunctionType*)vTableTy->getElementType(methodIdx);
                            auto methodAddr = builder->CreateStructGEP(vTableTy, vTable, methodIdx);

                            return builder->CreateLoad(methodTy, methodAddr);
                        }

                        // Function calls: (square 2)
                        else {
                            auto callable = gen(expr.list[0], env);

                            // Either a raw function or a functor (callable class):
                            auto callableTy = callable->getType()->getContainedType(0);   

                            std::vector<llvm::Value*> args{};
                            auto argIdx = 0;

                            // Callable classes:
                            if (callableTy->isStructTy()) {
                                auto cls = (llvm::StructType*)callableTy;

                                std::string className{cls->getName().data()};

                                // Push the factor itself as the first `self` arg:
                                args.push_back(callable);
                                argIdx++;  // and skip this argument

                                // TODO: support inheritance (load method from vTable)
                                callable = module->getFunction(className + "___call__");  // Note 3 underscores
                            }

                            auto fn = (llvm::Function*)callable;

                            for (auto i = 1; i < expr.list.size(); i++, argIdx++) {
                                auto argValue = gen(expr.list[i], env);

                                // Need to cast to parameter type to support sub-classes:
                                // We should be able to pass Point3D instance for the type of the parent class Point
                                auto paramTy = fn->getArg(argIdx)->getType();

                                auto bitCastArgVal = builder->CreateBitCast(argValue, paramTy);
                                args.push_back(bitCastArgVal);
                            }

                            return builder->CreateCall(fn, args);
                        }
                    }

                    // Method Calls: ((method p getX) 2)
                    else {
                        auto loadedMethod = (llvm::LoadInst*)gen(expr.list[0], env);
                        auto fnTy = (llvm::FunctionType*)(loadedMethod->getPointerOperand()
                                        ->getType()
                                        ->getContainedType(0)
                                        ->getContainedType(0));
                        
                        std::vector<llvm::Value*> args{};

                        for (auto i = 1; i < expr.list.size(); i++) {
                            auto argValue = gen(expr.list[i], env);

                            // Need to cast to parameter type to support sub-classes:
                            // We should be able to pass Point3D instance for the type of the parent class Point
                            auto paramTy = fnTy->getParamType(i - 1);

                            if (argValue->getType() != paramTy) {
                                auto bitCastArgVal = builder->CreateBitCast(argValue, paramTy);
                                args.push_back(bitCastArgVal);
                            } else {
                                args.push_back(argValue);
                            }
                        }

                        return builder->CreateCall(fnTy, loadedMethod, args);
                    }
                }
            }

            return builder->getInt32(0);
        }

        /**
         * Returns field index
        */
        size_t getFieldIndex(llvm::StructType* cls, const std::string& fieldName) {
            auto fields = &classMap_[cls->getName().data()].fieldsMap;
            auto it = fields->find(fieldName);

            return std::distance(fields->begin(), it) + RESERVED_FIELDS_COUNT;
        }

        /**
         * Returns method index
        */
        size_t getMethodIndex(llvm::StructType* cls, const std::string& methodName) {
            auto methods = &classMap_[cls->getName().data()].methodsMap;
            auto it = methods->find(methodName);

            return std::distance(methods->begin(), it);
        }

        /**
         * Creates an instane of a class
        */
        llvm::Value* createInstance(const Exp& exp, Env env, const std::string& name) {
            auto className = exp.list[1].string;
            auto cls = getClassByName(className);

            if (cls == nullptr) {
                DIE << "[EvaLLVM]: Unknown class " << cls;
            }

            // NOTE: Stack allocation (TODO: Heap allocation)
            // auto instance = name.empty() ? builder->CreateAlloca(cls) : builder->CreateAlloca(cls, 0, name);

            // We do not use stack allocation for objects, since we need to support constructor (factory) pattern
            // i.e. return a object from a callee to the caller, outside
            
            // Heap Allocation:
            auto instance = mallocInstance(cls, name);

            // Call constructor
            auto ctor = module->getFunction(className + "_constructor");

            std::vector<llvm::Value*> args{instance};

            for (auto i = 2; i < exp.list.size(); i++) {
                args.push_back(gen(exp.list[i], env));
            }

            builder->CreateCall(ctor, args);

            return instance;
        }

        /**
         * Allocates an object of a given class on the heap
        */
        llvm::Value* mallocInstance(llvm::StructType* cls, const std::string& name) {
            auto typeSize = builder->getInt64(getTypeSize(cls));

            // void*
            auto mallocPtr = builder->CreateCall(module->getFunction("GC_malloc"), typeSize, name);

            // void* -> Class*
            auto instance = builder->CreatePointerCast(mallocPtr, cls->getPointerTo());

            // Install the vTable to lookup methods:
            std::string className{cls->getName().data()};
            auto vTableName = className + "_vTable";
            auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX);
            auto vTable = module->getNamedGlobal(vTableName);
            builder->CreateStore(vTable, vTableAddr);

            return instance;
        }

        /**
         * Returns the size of a type in bytes
        */
        size_t getTypeSize(llvm::Type* type_) {
            return module->getDataLayout().getTypeAllocSize(type_);
        }

        /**
         * Inherits parent class fields
        */
        void inheritClass(llvm::StructType* cls, llvm::StructType* parent) {
            auto parentClassInfo = &classMap_[parent->getName().data()];

            // Inherit the field and method names
            classMap_[cls->getName().data()] = {
                /* class */ cls,
                /* parent */ parent,
                /* fields */ parentClassInfo->fieldsMap,
                /* methods */ parentClassInfo->methodsMap
            };
        }

        /**
         * Extracts fields and methods from a class expression
        */
        void buildClassInfo(llvm::StructType* cls, const Exp& clsExp, Env env) {
            auto className = clsExp.list[1].string;
            auto classInfo = &classMap_[className];

            // Body block: (begin ...)
            auto body = clsExp.list[3];

            for (auto i = 1; i < body.list.size(); i++) {
                auto exp = body.list[i];

                // If is variable
                if (isVar(exp)) {
                    auto varNameDecl = exp.list[1];

                    auto fieldName = extractVarName(varNameDecl);
                    auto fieldTy = extractVarType(varNameDecl);

                    classInfo->fieldsMap[fieldName] = fieldTy;
                }

                // If is function
                else if (isDef(exp)) {
                    auto methodName = exp.list[1].string;
                    auto fnName = className + "_" + methodName;

                    classInfo->methodsMap[methodName] = createFunctionProto(fnName, extractFunctionType(exp), env);
                }
            }

            // Create fields and vTable:
            buildClassBody(cls);
        }

        /**
         * Builds the class body from class info
        */
        void buildClassBody(llvm::StructType* cls) {
            std::string className{cls->getName().data()};

            auto classInfo = &classMap_[className];

            // Allocate vTable to set its type in the body
            // The table itself is populated later in buildVTable
            auto vTableName = className + "_vTable";
            auto vTableType = llvm::StructType::create(*ctx, vTableName);

            auto clsFields = std::vector<llvm::Type*>{
                // First element is always the vTable:
                vTableType->getPointerTo()
            };

            // Field types:
            for (const auto& fieldInfo : classInfo->fieldsMap) {
                clsFields.push_back(fieldInfo.second);
            }

            cls->setBody(clsFields, /* packed */ false);

            // Methods:
            buildVTable(cls);
        }

        /**
         * Creates a vTable per class.
         * 
         * vTables store method references to support inheritance and methods overloading
        */
        void buildVTable(llvm::StructType* cls) {
            std::string className{cls->getName().data()};
            auto vTableName = className + "_vTable";

            // The vTable should already exist:
            auto vTableTy = llvm::StructType::getTypeByName(*ctx, vTableName);

            std::vector<llvm::Constant*> vTableMethods;
            std::vector<llvm::Type*> vTableMethodTys;

            for (auto& methodInfo : classMap_[className].methodsMap) {
                auto method = methodInfo.second;
                
                vTableMethods.push_back(method);
                vTableMethodTys.push_back(method->getType());
            }

            vTableTy->setBody(vTableMethodTys);

            auto vTableValue = llvm::ConstantStruct::get(vTableTy, vTableMethods);
            createGlobalVar(vTableName, vTableValue);
        }

        /**
         * Tagged Lists
        */
        bool isTaggedList(const Exp& exp, const std::string& tag) {
            return exp.type == ExpType::LIST && exp.list[0].type == ExpType::SYMBOL && exp.list[0].string == tag;
        }

        /**
         * Is: (var ...)
        */
        bool isVar(const Exp& exp) { return isTaggedList(exp, "var"); }

        /**
         * Is: (def ...)
        */
        bool isDef(const Exp& exp) { return isTaggedList(exp, "def"); }

        /**
         * Is: (new ...)
        */
        bool isNew(const Exp& exp) { return isTaggedList(exp, "new"); }

        /**
         * Is: (prop ...)
        */
        bool isProp(const Exp& exp) { return isTaggedList(exp, "prop"); }

        /**
         * Is: (super ...)
        */
        bool isSuper(const Exp& exp) { return isTaggedList(exp, "super"); }

        /**
         * Returns a class type by name
        */
        llvm::StructType* getClassByName(const std::string& name) {
            return llvm::StructType::getTypeByName(*ctx, name);
        }

        /**
         * Extracts var or parameter name considering type.
         * 
         * x -> x
         * (x number) -> x
        */
        std::string extractVarName(const Exp& expr) {
            return expr.type == ExpType::LIST ? expr.list[0].string : expr.string;
        }

        /**
         * Extracts var or parameter type with i32 as default
         * 
         * x -> i32
         * (x number) -> number
        */
        llvm::Type* extractVarType(const Exp& expr) {
            return expr.type == ExpType::LIST ? getTypeFromString(expr.list[1].string) : builder->getInt32Ty();
        }

        /**
         * Returns LLVM type from string representation
        */
        llvm::Type* getTypeFromString(const std::string& type_) {
            // number -> i32
            if (type_ == "number") {
                return builder->getInt32Ty();
            }

            // string -> i8* (aka char*)
            if (type_ == "string") {
                return builder->getInt8Ty()->getPointerTo();
            }

            // Classes:
            return classMap_[type_].cls->getPointerTo();
        }

        /**
         * Whether the function has a return type defined
        */
        bool hasReturnType(const Exp& fnExp) {
            return fnExp.list[3].type == ExpType::SYMBOL && fnExp.list[3].string == "->";
        }

        /**
         * Expr Function to LLVM Function params.
         * 
         * (def square ((number x)) -> number ...)
         * 
         * llvm::FunctionType::get(returnType, paramTypes, false);
        */
        llvm::FunctionType* extractFunctionType(const Exp& fnExp) {
            auto params = fnExp.list[2];

            // Return type:
            auto returnType = hasReturnType(fnExp) ? getTypeFromString(fnExp.list[4].string) : builder->getInt32Ty();

            // Parameter Types:
            std::vector<llvm::Type*> paramTypes{};

            for (auto& param : params.list) {
                auto paramName = extractVarName(param);
                auto paramTy = extractVarType(param);

                // The `self` name is special, meaning instance of a class
                paramTypes.push_back(paramName == "self" ? (llvm::Type*)cls->getPointerTo() : paramTy);
            }

            return llvm::FunctionType::get(returnType, paramTypes, /* varargs */ false);
        }

        /**
         * Compiles a function
         * 
         * Untyped: (def square (x) (* x x)) - i32 by default
         * 
         * Typed: (def square ((x number)) -> number (* x x))
        */
        llvm::Value* compileFunction(const Exp& fnExp, std::string fnName, Env env) {
            auto params = fnExp.list[2];
            auto body = hasReturnType(fnExp) ? fnExp.list[5] : fnExp.list[3];

            // Save current fn:
            auto prevFn = fn;
            auto prevBlock = builder->GetInsertBlock();

            // TODO: DELETE VAR IF NOT USED
            auto origFnName = fnName;

            // Class method names:
            if (cls != nullptr) {
                fnName = std::string(cls->getName().data()) + "_" + fnName;
            }

            // Override fn to compile body:
            auto newFn = createFunction(fnName, extractFunctionType(fnExp), env);
            fn = newFn;

            // Set parameter names:
            auto idx = 0;

            // Function environment for params:
            auto fnEnv = std::make_shared<Environment>(std::map<std::string, llvm::Value*>{}, env);

            for (auto& arg : fn->args()) {
                auto param = params.list[idx++];
                auto argName = extractVarName(param);

                arg.setName(argName);

                // Allocate a local variable per argument to make arguments mutable
                auto argBinding = allocVar(argName, arg.getType(), fnEnv);
                builder->CreateStore(&arg, argBinding);
            }

            builder->CreateRet(gen(body, fnEnv));

            // Restore the previous fn after compiling
            builder->SetInsertPoint(prevBlock);
            fn = prevFn;

            return newFn;
        }

        /**
         * Allocates a local variable on the stack. Result is the alloca instruction
        */
        llvm::Value* allocVar(const std::string& name, llvm::Type* type_, Env env) {
            varsBuilder->SetInsertPoint(&fn->getEntryBlock());

            auto varAlloc = varsBuilder->CreateAlloca(type_, 0, name.c_str());

            // Add to the environment:
            env->define(name, varAlloc);

            return varAlloc;
        }

        /**
         * Creates a global variable
        */
        llvm::GlobalVariable* createGlobalVar(const std::string& name, llvm::Constant* init) {
            module->getOrInsertGlobal(name, init->getType());

            auto variable = module->getNamedGlobal(name);
            variable->setAlignment(llvm::MaybeAlign(4));
            variable->setConstant(false);
            variable->setInitializer(init);

            return variable;
        }

        /**
         * Define external functions (from libc++)
        */
        void setupExternalFunctions() {
            // i8* to substitute for char*, void*, etc
            auto bytePtrTy = builder->getInt8Ty()->getPointerTo();

            // int printf (const char* format, ...);
            module->getOrInsertFunction("printf", llvm::FunctionType::get(
                /* return type */ builder->getInt32Ty(),
                /* format arg */ bytePtrTy,
                /* vararg */ true
            ));

            // void* malloc(size_t size), void* GC_malloc(size_t size)
            // size_t is i64
            module->getOrInsertFunction("GC_malloc", llvm::FunctionType::get(bytePtrTy, builder->getInt64Ty(), /* vararg */ false));
        }

        /**
         * Creates a function
        */
        llvm::Function* createFunction(const std::string& fnName, llvm::FunctionType* fnType, Env env) {
            // Function prototype might already be defined
            auto fn = module->getFunction(fnName);

            // If not, allocate the function:
            if (fn == nullptr) {
                fn = createFunctionProto(fnName, fnType, env);
            }

            createFunctionBlock(fn);

            return fn;
        }

        /**
         * Creates a function prototype (defines the function, not the body)
        */
        llvm::Function* createFunctionProto(const std::string& fnName, llvm::FunctionType* fnType, Env env) {
            auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, *module);

            verifyFunction(*fn);

            // Install in the environment
            env->define(fnName, fn);

            return fn;
        }

        /**
         * Creates a function block
        */
        void createFunctionBlock(llvm::Function* fn) {
            auto entry = createBasicBlock("entry", fn);
            builder->SetInsertPoint(entry);
        }

        /**
         * Creates a basic block.
         * 
         * If the `fn` is passed in, the block is automatically appended to the parent function.
         * Otherwise, the block should later be appeneded manually via fn->getBasicBlockList().push_back(block)
        */
        llvm::BasicBlock* createBasicBlock(std::string name, llvm::Function* fn = nullptr) {
            return llvm::BasicBlock::Create(*ctx, name, fn)
;        }

        /**
         * Saves the IR to a file
        */
        void saveModuleToFile(const std::string& fileName) {
            std::error_code errorCode;
            llvm::raw_fd_ostream outLL(fileName, errorCode);
            module->print(outLL, nullptr);
        }

        /**
         * Initializes the module
        */
        void moduleInit() {
            // Open a new context and module
            ctx = std::make_unique<llvm::LLVMContext>();
            module = std::make_unique<llvm::Module>("EvaLLVM", *ctx);

            // Create a new builder for the module
            builder = std::make_unique<llvm::IRBuilder<>>(*ctx);

            // Vars builder
            varsBuilder = std::make_unique<llvm::IRBuilder<>>(*ctx);
        }

        /**
         * Sets up the Global Environment
        */
        void setupGlobalEnvironment() {
            std::map<std::string, llvm::Value*> globalObject{
                {"VERSION", builder->getInt32(42)}
            };

            std::map<std::string, llvm::Value*> globalRec{};

            for (auto& entry : globalObject) {
                globalRec[entry.first] = createGlobalVar(entry.first, (llvm::Constant*)entry.second);
            }

            GlobalEnv = std::make_shared<Environment>(globalRec, nullptr);
        }

        /**
         * Sets up target triple.
        */
        void setupTargetTriple() {
            // x86_64-pc-linux-gnu | llvm::sys::getDefaultTargetTriple()
            module->setTargetTriple("x86_64-pc-linux-gnu");
        }

        /**
         * Parser
        */
        std::unique_ptr<EvaParser> parser;

        /**
         * Global Environment (Symbol Table)
        */
        std::shared_ptr<Environment> GlobalEnv;

        /**
         * Currently compiling function
        */
        llvm::Function* fn;

        /**
         * Currently compiling class
        */
        llvm::StructType* cls = nullptr;

        /**
         * Class Info.
        */
        std::map<std::string, ClassInfo> classMap_;

        std::unique_ptr<llvm::LLVMContext> ctx;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::IRBuilder<>> builder;

        /**
         * Extra builder for variables declaration.
         * This builder always prepends to the beginning of the function entry block
        */
        std::unique_ptr<llvm::IRBuilder<>> varsBuilder;
};

#endif