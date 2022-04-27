@.str = private unnamed_addr constant [13 x i8] c"Hello world!\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = call i32 (i8*, ...) 
  @printf(i8* getelementptr inbounds 
  ([13 x i8], [13 x i8]* @.str, i64 0, i64 0))
  ret i32 0
}

declare dso_local i32 @printf(i8*, ...) #1