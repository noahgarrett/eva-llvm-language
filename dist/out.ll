; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"
target triple = "x86_64-pc-linux-gnu"

%Transformer_vTable = type { i32 (%Transformer*, i32)*, %Transformer* (%Transformer*, i32)* }
%Transformer = type { %Transformer_vTable*, i32 }

@VERSION = global i32 42, align 4
@Transformer_vTable = global %Transformer_vTable { i32 (%Transformer*, i32)* @Transformer___call__, %Transformer* (%Transformer*, i32)* @Transformer_constructor }, align 4
@0 = private unnamed_addr constant [21 x i8] c"(transform 10) = %d\0A\00", align 1
@1 = private unnamed_addr constant [21 x i8] c"(transform 10) = %d\0A\00", align 1
@2 = private unnamed_addr constant [25 x i8] c"(map 10 transform) = %d\0A\00", align 1

declare i32 @printf(i8*, ...)

declare i8* @GC_malloc(i64)

define i32 @main() {
entry:
  %transform = call i8* @GC_malloc(i64 16)
  %0 = bitcast i8* %transform to %Transformer*
  %1 = getelementptr inbounds %Transformer, %Transformer* %0, i32 0, i32 0
  store %Transformer_vTable* @Transformer_vTable, %Transformer_vTable** %1, align 8
  %2 = call %Transformer* @Transformer_constructor(%Transformer* %0, i32 5)
  %3 = call i32 @Transformer___call__(%Transformer* %0, i32 10)
  %4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([21 x i8], [21 x i8]* @0, i32 0, i32 0), i32 %3)
  %5 = getelementptr inbounds %Transformer, %Transformer* %0, i32 0, i32 0
  %vt = load %Transformer_vTable*, %Transformer_vTable** %5, align 8
  %6 = getelementptr inbounds %Transformer_vTable, %Transformer_vTable* %vt, i32 0, i32 0
  %7 = load i32 (%Transformer*, i32)*, i32 (%Transformer*, i32)** %6, align 8
  %8 = call i32 %7(%Transformer* %0, i32 10)
  %9 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([21 x i8], [21 x i8]* @1, i32 0, i32 0), i32 %8)
  %10 = call i32 @map(i32 10, %Transformer* %0)
  %11 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @2, i32 0, i32 0), i32 %10)
  ret i32 0
}

define %Transformer* @Transformer_constructor(%Transformer* %self, i32 %factor) {
entry:
  %self1 = alloca %Transformer*, align 8
  store %Transformer* %self, %Transformer** %self1, align 8
  %factor2 = alloca i32, align 4
  store i32 %factor, i32* %factor2, align 4
  %factor3 = load i32, i32* %factor2, align 4
  %self4 = load %Transformer*, %Transformer** %self1, align 8
  %pfactor = getelementptr inbounds %Transformer, %Transformer* %self4, i32 0, i32 1
  store i32 %factor3, i32* %pfactor, align 4
  %self5 = load %Transformer*, %Transformer** %self1, align 8
  ret %Transformer* %self5
}

define i32 @Transformer___call__(%Transformer* %self, i32 %v) {
entry:
  %self1 = alloca %Transformer*, align 8
  store %Transformer* %self, %Transformer** %self1, align 8
  %v2 = alloca i32, align 4
  store i32 %v, i32* %v2, align 4
  %self3 = load %Transformer*, %Transformer** %self1, align 8
  %pfactor = getelementptr inbounds %Transformer, %Transformer* %self3, i32 0, i32 1
  %factor = load i32, i32* %pfactor, align 4
  %v4 = load i32, i32* %v2, align 4
  %tmpmul = mul i32 %factor, %v4
  ret i32 %tmpmul
}

define i32 @map(i32 %x, %Transformer* %modify) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, i32* %x1, align 4
  %modify2 = alloca %Transformer*, align 8
  store %Transformer* %modify, %Transformer** %modify2, align 8
  %modify3 = load %Transformer*, %Transformer** %modify2, align 8
  %x4 = load i32, i32* %x1, align 4
  %0 = call i32 @Transformer___call__(%Transformer* %modify3, i32 %x4)
  ret i32 %0
}
