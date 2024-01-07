# Compile main:
clang++ -o dist/eva-llvm `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# Run main:
./dist/eva-llvm

# Execute generated IR:
lli ./dist/out.ll

# Print result
echo $?

printf "\n"