
## Next Steps
1. Optimizing compiler
    - LLVM allows 'Optimization Passes'
    - One tool named `opt`
2. Arrays / Lists
    - (list 1 2 3) -> llvm::ArrayType
3. Custom Garbage Collector hooks -> https://llvm.org/docs/GarbageCollection.html + Essentials of Garbage Collectors Course
4. Interfaces `(interface Callable ... (def __call__ (self) throw))`
    - `(class Transformer Callable ...)`
5. Rest arguments:
    - `(interface Callable ... (def __call__ (self ...) throw))`
6. Opaque pointers: i32 -> ptr, i8* -> ptr, etc
7. LLVM IR & MLIR
8. `(async def fetch (...) ...) -> (await fetch ...)`

## Example
```lisp
// Functors - callable objects (aka Closures)

// Closures are syntactic sugar for functors
// which share the same closured cells

// 1. Cell - a heap-allocated value, i.e. instance of some class
(class Cell null
    (begin
    
        (var value 0)

        (def constructor (self value) -> Cell
            (begin
                (set (prop self value) value)
                self
            )
        )
    
    )
)

(class SetFunctor null
    (begin
        (var (cell Cell) 0)

        (def constructor (self (cell Cell)) -> SetFunctor
            (begin
                (set (prop self cell) cell)
                self
            )
        )

        (def __call__ (self value)
            (begin
                (set (prop (prop self cell) value) value)
                value
            )
        )
    )
)

(class GetFunctor null
    (begin
        (var (cell Cell) 0)

        (def constructor (self (cell Cell)) -> GetFunctor
            (begin
                (set (prop self cell) cell)
                self
            )
        )

        (def __call__ (self)
            (prop (prop self  cell) value)
        )
    )
)

(var n (new Cell 10))

(var setN (new SetFunctor n))
(var getN (new GetFunctor n))

(printf "getN.__call__ result = %d\n" (getN)) // 10
(printf "setN.__call__ result = %d\n" (setN 20))
(printf "getN.__call__ result = %d\n" (getN)) // 20
```