SECTION MBT vstart=0x7c00
	mov ax, cs
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov sp, 0x7c00


	; INT 0x10		功能号 0x06		功能: 上卷窗口 

	; 输入
	; ah = 功能号 0x06
	; al = 上卷的行数 0则全部上卷
	; bh = 上卷行的属性
	; (cl, ch) 窗口左上角(x, y)的位置
	; (dl, dh) 窗口右下角(x, y)的位置
	; 无返回值
	mov ax, 0x600
	mov bx, 0x700
	mov cx, 0
	mov dx, 0x184f

	int 0x10
	
	mov ah, 3
	mov bh, 0
	int 0x10

	; get curse 获得当前光标位置 在光标位置处打印字符串;
	; es: bp 为串首地址 这时es = cs
	; 光标位置在dx寄存器里面
	; cx为字符串长度
	mov ax, message
	mov bp, ax
	mov cx, 5
	mov ax, 0x1301
	mov bx, 0x2
	int 0x10

	jmp $

	message db "1 MBR"
	times 510-($-$$) db 0
	db 0x55, 0xaa

