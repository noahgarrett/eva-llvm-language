#include <string>
#include <iostream>

#include "./src/EvaLLVM.h"

int main(int argc, char const *argv[])
{
    // Program to execute
    std::string program = R"(
        (var x 42)

        (begin
           (var (x string) "Hello")
           (printf "X: %s\n\n" x))

        (printf "X: %d\n\n" x)

        (set x 100)

        (printf "X: %d\n\n" x)
    )";

    EvaLLVM vm;
    vm.exec(program);
    printf("\n");

    return 0;
}
