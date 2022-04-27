@.str = private unnamed_addr constant [3 x i8] c"10\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 8, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = icmp sgt i32 %3, 6
  br i1 %4, label %5, label %7

5:                                                ; preds = %0
  %6 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0))
  store i32 1, i32* %1, align 4
  br label %8

7:                                                ; preds = %0
  store i32 0, i32* %1, align 4
  br label %8

8:                                                ; preds = %7, %5
  %9 = load i32, i32* %1, align 4
  ret i32 %9
}

declare dso_local i32 @printf(i8*, ...) #1
