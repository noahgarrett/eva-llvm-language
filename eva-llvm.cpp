#include <string>
#include <iostream>
#include <fstream>

#include "./src/EvaLLVM.h"

void printHelp() {
    std::cout << "\nUsage: eva-llvm [options]\n\n"
              << "Options:\n"
              << "  -e, --expression    Expression to parse\n"
              << "  -f, --file          File to parse\n\n";
}

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        printHelp();
        return 0;
    }

    /**
     * Expression Mode
    */
    std::string mode = argv[1];

    /**
     * Program to execute
    */
    std::string program;

    /**
     * Simple expression
    */
    if (mode == "-e") {
        program = argv[2];
    }

    /**
     * Eva File
    */
    else if (mode == "-f") {
        // Read the file
        std::ifstream programFile(argv[2]);
        std::stringstream buffer;
        buffer << programFile.rdbuf() << "\n";

        // Program:
        program = buffer.str();
    }

    /**
     * Compiler Instance
    */
    EvaLLVM vm;

    /**
     * Generate LLVM IR
    */
    vm.exec(program);
    printf("\n");

    return 0;
}
