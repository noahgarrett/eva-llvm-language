; ModuleID = './dist/out.ll'
source_filename = "EvaLLVM"
target triple = "x86_64-pc-linux-gnu"

%Transformer_vTable = type { i32 (%Transformer*, i32)*, %Transformer* (%Transformer*, i32)* }
%Transformer = type { %Transformer_vTable*, i32 }

@VERSION = local_unnamed_addr global i32 42, align 4
@Transformer_vTable = global %Transformer_vTable { i32 (%Transformer*, i32)* @Transformer___call__, %Transformer* (%Transformer*, i32)* @Transformer_constructor }, align 4
@0 = private unnamed_addr constant [21 x i8] c"(transform 10) = %d\0A\00", align 1
@1 = private unnamed_addr constant [25 x i8] c"(map 10 transform) = %d\0A\00", align 1

; Function Attrs: nofree nounwind
declare noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #0

declare i8* @GC_malloc(i64) local_unnamed_addr

define i32 @main() local_unnamed_addr {
entry:
  %transform = tail call i8* @GC_malloc(i64 16)
  %0 = bitcast i8* %transform to %Transformer*
  %1 = bitcast i8* %transform to %Transformer_vTable**
  store %Transformer_vTable* @Transformer_vTable, %Transformer_vTable** %1, align 8
  %pfactor.i = getelementptr inbounds i8, i8* %transform, i64 8
  %2 = bitcast i8* %pfactor.i to i32*
  store i32 5, i32* %2, align 4
  %3 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([21 x i8], [21 x i8]* @0, i64 0, i64 0), i32 50)
  %vt = load %Transformer_vTable*, %Transformer_vTable** %1, align 8
  %4 = getelementptr inbounds %Transformer_vTable, %Transformer_vTable* %vt, i64 0, i32 0
  %5 = load i32 (%Transformer*, i32)*, i32 (%Transformer*, i32)** %4, align 8
  %6 = tail call i32 %5(%Transformer* %0, i32 10)
  %7 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([21 x i8], [21 x i8]* @0, i64 0, i64 0), i32 %6)
  %factor.i.i = load i32, i32* %2, align 4
  %tmpmul.i.i = mul i32 %factor.i.i, 10
  %8 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([25 x i8], [25 x i8]* @1, i64 0, i64 0), i32 %tmpmul.i.i)
  ret i32 0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define %Transformer* @Transformer_constructor(%Transformer* returned writeonly %self, i32 %factor) #1 {
entry:
  %pfactor = getelementptr inbounds %Transformer, %Transformer* %self, i64 0, i32 1
  store i32 %factor, i32* %pfactor, align 4
  ret %Transformer* %self
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Transformer___call__(%Transformer* nocapture readonly %self, i32 %v) #2 {
entry:
  %pfactor = getelementptr inbounds %Transformer, %Transformer* %self, i64 0, i32 1
  %factor = load i32, i32* %pfactor, align 4
  %tmpmul = mul i32 %factor, %v
  ret i32 %tmpmul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @map(i32 %x, %Transformer* nocapture readonly %modify) local_unnamed_addr #2 {
entry:
  %pfactor.i = getelementptr inbounds %Transformer, %Transformer* %modify, i64 0, i32 1
  %factor.i = load i32, i32* %pfactor.i, align 4
  %tmpmul.i = mul i32 %factor.i, %x
  ret i32 %tmpmul.i
}

attributes #0 = { nofree nounwind }
attributes #1 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
