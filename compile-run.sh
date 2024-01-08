# Compile main:
clang++ -o dist/eva-llvm `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# Run main:
./dist/eva-llvm

# Execute generated IR:
# lli ./dist/out.ll

clang++ -O3 -I/usr/local/include/gc/ ./dist/out.ll /usr/lib/x86_64-linux-gnu/libgc.a -o ./dist/out

# Run the compiled program
./dist/out

# Print result
echo $?

printf "\n"