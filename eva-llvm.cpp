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

        (class Point3D Point
            (begin
            
                (var z 100)

                (def constructor (self x y z)
                    (begin
                        ((method (super Point3D) constructor) self x y)
                        (set (prop self z) z)
                    )
                )

                (def calc (self)
                    (+ ((method (super Point3D) calc) self) (prop self z))
                )
            
            ))

        (var p1 (new Point 10 20))
        (var p2 (new Point3D 100 200 300))

        (printf "p2.x = %d\n" (prop p2 x))
        (printf "p2.y = %d\n" (prop p2 y))
        (printf "p2.z = %d\n" (prop p2 z))

        (printf "Point3D.calc result = %d\n" ((method p2 calc) p2))

        (def check ((obj Point))
            (begin
                ((method obj calc) obj)
            )
        )

        (check p1) // Point.calc
        (check p2) // Point3D.calc
    )";

    EvaLLVM vm;
    vm.exec(program);
    printf("\n");

    return 0;
}
