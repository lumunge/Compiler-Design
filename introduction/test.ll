define i32 @main(){
    ; hello
    call i32 @putchar(i32 72)
    call i32 @putchar(i32 101)
    call i32 @putchar(i32 108)
    call i32 @putchar(i32 108)
    call i32 @putchar(i32 111)

    ; space
    call i32 @putchar(i32 32)

    ; world
    call i32 @putchar(i32 119)
    call i32 @putchar(i32 111)
    call i32 @putchar(i32 114)
    call i32 @putchar(i32 108)
    call i32 @putchar(i32 100)

    ; new line
    call i32 @putchar(i32 10)

    ret i32 0
}

declare i32 @putchar(i32)