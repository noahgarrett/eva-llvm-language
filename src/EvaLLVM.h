/**
 * Eva to LLVM IR compiler
*/

#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <string>
#include <regex>
#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "./parser/EvaParser.h"
#include "Environment.h"

using syntax::EvaParser;

/**
 * Environment Type
*/
using Env = std::shared_ptr<Environment>;

class EvaLLVM {
    public:
        EvaLLVM() : parser(std::make_unique<EvaParser>()) { 
            moduleInit(); 
            setupExternalFunctions();
            setupGlobalEnvironment();
        }
    
    void exec(const std::string& program) {
        // 1. Parse the program
        auto ast = parser->parse("(begin" + program + ")");

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
                        if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
                            return builder->CreateLoad(globalVar->getInitializer()->getType(), globalVar, varName.c_str());
                        }
                    }
                }
                
                // Lists
                case ExpType::LIST: {
                    auto tag = expr.list[0];

                    // Special Cases
                    if (tag.type == ExpType::SYMBOL) {
                        auto op = tag.string;

                        // Variable declaration: (var x (+ y 10))
                        // Typed: (var (x number) 42)
                        // Note: Locals are allocated on the stack
                        if (op == "var") {
                            auto varNameDecl = expr.list[1];
                            auto varName = extractVarName(varNameDecl);

                            // Initializer
                            auto init = gen(expr.list[2], env);

                            // Type:
                            auto varTy = extractVarType(varNameDecl);

                            // Variable:
                            auto varBinding = allocVar(varName, varTy, env);

                            // Set Value:
                            return builder->CreateStore(init, varBinding);
                        }

                        // Variable update
                        // (set x 100)
                        else if (op == "set") {
                            // Value:
                            auto value = gen(expr.list[2], env);

                            auto varName = expr.list[1].string;

                            // Variable:
                            auto varBinding = env->lookup(varName);

                            // Set value:
                            return builder->CreateStore(value, varBinding);
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
                    }
                }
            }

            return builder->getInt32(0);
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

            // default:
            return builder->getInt32Ty();
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