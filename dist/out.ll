; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"
target triple = "x86_64-pc-linux-gnu"

%Point = type { i32, i32 }

@VERSION = global i32 42, align 4
@0 = private unnamed_addr constant [10 x i8] c"p.x = %d\0A\00", align 1
@1 = private unnamed_addr constant [10 x i8] c"p.y = %d\0A\00", align 1

declare i32 @printf(i8*, ...)

declare i8* @GC_malloc(i64)

define i32 @main() {
entry:
  %p = call i8* @GC_malloc(i64 8)
  %0 = bitcast i8* %p to %Point*
  %1 = call i32 @Point_constructor(%Point* %0, i32 10, i32 20)
  %px = getelementptr inbounds %Point, %Point* %0, i32 0, i32 0
  %x = load i32, i32* %px, align 4
  %2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([10 x i8], [10 x i8]* @0, i32 0, i32 0), i32 %x)
  %py = getelementptr inbounds %Point, %Point* %0, i32 0, i32 1
  %y = load i32, i32* %py, align 4
  %3 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([10 x i8], [10 x i8]* @1, i32 0, i32 0), i32 %y)
  ret i32 0
}

define i32 @Point_constructor(%Point* %self, i32 %x, i32 %y) {
entry:
  %self1 = alloca %Point*, align 8
  store %Point* %self, %Point** %self1, align 8
  %x2 = alloca i32, align 4
  store i32 %x, i32* %x2, align 4
  %y3 = alloca i32, align 4
  store i32 %y, i32* %y3, align 4
  %x4 = load i32, i32* %x2, align 4
  %self5 = load %Point*, %Point** %self1, align 8
  %px = getelementptr inbounds %Point, %Point* %self5, i32 0, i32 0
  store i32 %x4, i32* %px, align 4
  %y6 = load i32, i32* %y3, align 4
  %self7 = load %Point*, %Point** %self1, align 8
  %py = getelementptr inbounds %Point, %Point* %self7, i32 0, i32 1
  store i32 %y6, i32* %py, align 4
  ret i32 %y6
}

define i32 @Point_calc(%Point* %self) {
entry:
  %self1 = alloca %Point*, align 8
  store %Point* %self, %Point** %self1, align 8
  %self2 = load %Point*, %Point** %self1, align 8
  %px = getelementptr inbounds %Point, %Point* %self2, i32 0, i32 0
  %x = load i32, i32* %px, align 4
  %self3 = load %Point*, %Point** %self1, align 8
  %py = getelementptr inbounds %Point, %Point* %self3, i32 0, i32 1
  %y = load i32, i32* %py, align 4
  %tmpadd = add i32 %x, %y
  ret i32 %tmpadd
}
