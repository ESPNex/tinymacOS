.section .rodata
.global icons,accountpic,systemicons,control_svg_masks
icons: .incbin TMC_ICON_ATLAS
systemicons: .incbin "assets/symbols.rgba"
control_svg_masks: .incbin TMC_CONTROL_SVG
accountpic: .incbin "assets/account.rgba"
.section .note.GNU-stack,"",@progbits
