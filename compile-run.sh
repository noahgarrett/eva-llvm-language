# Compile main:
clang++ -o dist/eva-llvm `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# Run main:
./dist/eva-llvm

# Execute generated IR:
# lli ./dist/out.ll

# Optimize the output:
opt ./dist/out.ll -passes='default<O3>' -S -o ./dist/out-opt.ll

clang++ -O3 -I/usr/local/include/gc/ ./dist/out-opt.ll /usr/lib/x86_64-linux-gnu/libgc.a -o ./dist/out

# Run the compiled program
./dist/out

# Print result
echo $?

printf "\n"