; infinite loop
define i32 @main(){
    br label %loop
loop:
    call i32 @putchar(i32 65)
    br label %loop
}

declare i32 @putchar(i32)