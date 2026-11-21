.section .rodata
.global icons,accountpic,systemicons
icons: .incbin TMC_ICON_ATLAS
systemicons: .incbin "assets/symbols.rgba"
accountpic: .incbin "assets/account.rgba"
.section .note.GNU-stack,"",@progbits
