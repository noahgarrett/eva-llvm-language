#include <string>
#include <iostream>

#include "./src/EvaLLVM.h"

int main(int argc, char const *argv[])
{
    // Program to execute
    std::string program = R"(
        (class Point null
            (begin
                
                (var x 0)
                (var y 0)

                (def constructor (self x y)
                    (begin
                        (set (prop self x) x)
                        (set (prop self y) y)
                    ))

                (def calc (self)
                    (+ (prop self x) (prop self y))
                    )
                
                ))

        (var p (new Point 10 20))

        (printf "p.x = %d\n" (prop p x))
        (printf "p.y = %d\n" (prop p y))
    )";

    EvaLLVM vm;
    vm.exec(program);
    printf("\n");

    return 0;
}
