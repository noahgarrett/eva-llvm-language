/**
 * Environment (Symbol Table)
*/

#ifndef Environment_h
#define Environment_h

#include <map>
#include <memory>
#include <string>

#include "llvm/IR/Value.h"
#include "Logger.h"

class Environment : public std::enable_shared_from_this<Environment> {
    public:
    
        /**
         * Creates an environment with the given record.
        */
        Environment(std::map<std::string, llvm::Value*> record, std::shared_ptr<Environment> parent) : record_(record), parent_(parent) {}

        /**
         * Creates a variable with the given name and values
        */
        llvm::Value* define(const std::string& name, llvm::Value* value) {
            record_[name] = value;
            return value;
        }

        /**
         * Returns the value of a defined variable, or throws if the variable is not defined.
        */
        llvm::Value* lookup(const std::string& name) {
            return resolve(name)->record_[name];
        }

    private:

        /**
         * Returns specific environment in which a variable is defined, or throws if a variable is not defined
        */
        std::shared_ptr<Environment> resolve(const std::string& name) {
            if (record_.count(name) != 0) {
                return shared_from_this();
            }

            if (parent_ == nullptr) {
                DIE << "Variable \"" << name << "\" is not defined.";
            }

            return parent_->resolve(name);
        }

        /**
         * Bindings Storage
        */
        std::map<std::string, llvm::Value*> record_;

       /**
        * Parent Link
       */
        std::shared_ptr<Environment> parent_;
};

#endif