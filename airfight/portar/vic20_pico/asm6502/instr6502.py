

INSTRUCTION_TABLE = {
    # ADC - Add with Carry
    0x69: {"name": "ADC", "mode": "immediate", "cycles": 2},
    0x65: {"name": "ADC", "mode": "zeropage", "cycles": 3},
    0x75: {"name": "ADC", "mode": "zeropage_x", "cycles": 4},
    0x6D: {"name": "ADC", "mode": "absolute", "cycles": 4},
    0x7D: {"name": "ADC", "mode": "absolute_x", "cycles": 4},
    0x79: {"name": "ADC", "mode": "absolute_y", "cycles": 4},
    0x61: {"name": "ADC", "mode": "indirect_x", "cycles": 6},
    0x71: {"name": "ADC", "mode": "indirect_y", "cycles": 5},
    
    # AND - Logical AND
    0x29: {"name": "AND", "mode": "immediate", "cycles": 2},
    0x25: {"name": "AND", "mode": "zeropage", "cycles": 3},
    0x35: {"name": "AND", "mode": "zeropage_x", "cycles": 4},
    0x2D: {"name": "AND", "mode": "absolute", "cycles": 4},
    0x3D: {"name": "AND", "mode": "absolute_x", "cycles": 4},
    0x39: {"name": "AND", "mode": "absolute_y", "cycles": 4},
    0x21: {"name": "AND", "mode": "indirect_x", "cycles": 6},
    0x31: {"name": "AND", "mode": "indirect_y", "cycles": 5},
    
    # ASL - Arithmetic Shift Left
    0x0A: {"name": "ASL_A", "mode": "implied", "cycles": 2},
    0x06: {"name": "ASL", "mode": "zeropage", "cycles": 5},
    0x16: {"name": "ASL", "mode": "zeropage_x", "cycles": 6},
    0x0E: {"name": "ASL", "mode": "absolute", "cycles": 6},
    0x1E: {"name": "ASL", "mode": "absolute_x", "cycles": 7},
    
    # BCC - Branch if Carry Clear
    0x90: {"name": "BCC", "mode": "relative", "cycles": 2}, # Base cycles (not taken)
    
    # BCS - Branch if Carry Set
    0xB0: {"name": "BCS", "mode": "relative", "cycles": 2},
    
    # BEQ - Branch if Equal
    0xF0: {"name": "BEQ", "mode": "relative", "cycles": 2},
    
    # BIT - Bit Test
    0x24: {"name": "BIT", "mode": "zeropage", "cycles": 3},
    0x2C: {"name": "BIT", "mode": "absolute", "cycles": 4},
    
    # BMI - Branch if Minus
    0x30: {"name": "BMI", "mode": "relative", "cycles": 2},
    
    # BNE - Branch if Not Equal
    0xD0: {"name": "BNE", "mode": "relative", "cycles": 2},
    
    # BPL - Branch if Positive
    0x10: {"name": "BPL", "mode": "relative", "cycles": 2},
    
    # BRK - Force Interrupt
    0x00: {"name": "BRK", "mode": "implied", "cycles": 7},
    
    # BVC - Branch if Overflow Clear
    0x50: {"name": "BVC", "mode": "relative", "cycles": 2},
    
    # BVS - Branch if Overflow Set
    0x70: {"name": "BVS", "mode": "relative", "cycles": 2},
    
    # CLC - Clear Carry Flag
    0x18: {"name": "CLC", "mode": "implied", "cycles": 2},
    
    # CLD - Clear Decimal Mode
    0xD8: {"name": "CLD", "mode": "implied", "cycles": 2},
    
    # CLI - Clear Interrupt Disable
    0x58: {"name": "CLI", "mode": "implied", "cycles": 2},
    
    # CLV - Clear Overflow Flag
    0xB8: {"name": "CLV", "mode": "implied", "cycles": 2},
    
    # CMP - Compare
    0xC9: {"name": "CMP", "mode": "immediate", "cycles": 2},
    0xC5: {"name": "CMP", "mode": "zeropage", "cycles": 3},
    0xD5: {"name": "CMP", "mode": "zeropage_x", "cycles": 4},
    0xCD: {"name": "CMP", "mode": "absolute", "cycles": 4},
    0xDD: {"name": "CMP", "mode": "absolute_x", "cycles": 4},
    0xD9: {"name": "CMP", "mode": "absolute_y", "cycles": 4},
    0xC1: {"name": "CMP", "mode": "indirect_x", "cycles": 6},
    0xD1: {"name": "CMP", "mode": "indirect_y", "cycles": 5},
    
    # CPX - Compare X Register
    0xE0: {"name": "CPX", "mode": "immediate", "cycles": 2},
    0xE4: {"name": "CPX", "mode": "zeropage", "cycles": 3},
    0xEC: {"name": "CPX", "mode": "absolute", "cycles": 4},
    
    # CPY - Compare Y Register
    0xC0: {"name": "CPY", "mode": "immediate", "cycles": 2},
    0xC4: {"name": "CPY", "mode": "zeropage", "cycles": 3},
    0xCC: {"name": "CPY", "mode": "absolute", "cycles": 4},
    
    # DEC - Decrement Memory
    0xC6: {"name": "DEC", "mode": "zeropage", "cycles": 5},
    0xD6: {"name": "DEC", "mode": "zeropage_x", "cycles": 6},
    0xCE: {"name": "DEC", "mode": "absolute", "cycles": 6},
    0xDE: {"name": "DEC", "mode": "absolute_x", "cycles": 7},
    
    # DEX - Decrement X Register
    0xCA: {"name": "DEX", "mode": "implied", "cycles": 2},
    
    # DEY - Decrement Y Register
    0x88: {"name": "DEY", "mode": "implied", "cycles": 2},
    
    # EOR - Exclusive OR
    0x49: {"name": "EOR", "mode": "immediate", "cycles": 2},
    0x45: {"name": "EOR", "mode": "zeropage", "cycles": 3},
    0x55: {"name": "EOR", "mode": "zeropage_x", "cycles": 4},
    0x4D: {"name": "EOR", "mode": "absolute", "cycles": 4},
    0x5D: {"name": "EOR", "mode": "absolute_x", "cycles": 4},
    0x59: {"name": "EOR", "mode": "absolute_y", "cycles": 4},
    0x41: {"name": "EOR", "mode": "indirect_x", "cycles": 6},
    0x51: {"name": "EOR", "mode": "indirect_y", "cycles": 5},
    
    # INC - Increment Memory
    0xE6: {"name": "INC", "mode": "zeropage", "cycles": 5},
    0xF6: {"name": "INC", "mode": "zeropage_x", "cycles": 6},
    0xEE: {"name": "INC", "mode": "absolute", "cycles": 6},
    0xFE: {"name": "INC", "mode": "absolute_x", "cycles": 7},
    
    # INX - Increment X Register
    0xE8: {"name": "INX", "mode": "implied", "cycles": 2},
    
    # INY - Increment Y Register
    0xC8: {"name": "INY", "mode": "implied", "cycles": 2},
    
    # JMP - Jump
    0x4C: {"name": "JMP", "mode": "absolute", "cycles": 3},
    0x6C: {"name": "JMP", "mode": "indirect", "cycles": 5},
    
    # JSR - Jump to Subroutine
    0x20: {"name": "JSR", "mode": "absolute", "cycles": 6},
    
    # LDA - Load Accumulator
    0xA9: {"name": "LDA", "mode": "immediate", "cycles": 2},
    0xA5: {"name": "LDA", "mode": "zeropage", "cycles": 3},
    0xB5: {"name": "LDA", "mode": "zeropage_x", "cycles": 4},
    0xAD: {"name": "LDA", "mode": "absolute", "cycles": 4},
    0xBD: {"name": "LDA", "mode": "absolute_x", "cycles": 4},
    0xB9: {"name": "LDA", "mode": "absolute_y", "cycles": 4},
    0xA1: {"name": "LDA", "mode": "indirect_x", "cycles": 6},
    0xB1: {"name": "LDA", "mode": "indirect_y", "cycles": 5},
    
    # LDX - Load X Register
    0xA2: {"name": "LDX", "mode": "immediate", "cycles": 2},
    0xA6: {"name": "LDX", "mode": "zeropage", "cycles": 3},
    0xB6: {"name": "LDX", "mode": "zeropage_y", "cycles": 4},
    0xAE: {"name": "LDX", "mode": "absolute", "cycles": 4},
    0xBE: {"name": "LDX", "mode": "absolute_y", "cycles": 4},
    
    # LDY - Load Y Register
    0xA0: {"name": "LDY", "mode": "immediate", "cycles": 2},
    0xA4: {"name": "LDY", "mode": "zeropage", "cycles": 3},
    0xB4: {"name": "LDY", "mode": "zeropage_x", "cycles": 4},
    0xAC: {"name": "LDY", "mode": "absolute", "cycles": 4},
    0xBC: {"name": "LDY", "mode": "absolute_x", "cycles": 4},
    
    # LSR - Logical Shift Right
    0x4A: {"name": "LSR_A", "mode": "implied", "cycles": 2},
    0x46: {"name": "LSR", "mode": "zeropage", "cycles": 5},
    0x56: {"name": "LSR", "mode": "zeropage_x", "cycles": 6},
    0x4E: {"name": "LSR", "mode": "absolute", "cycles": 6},
    0x5E: {"name": "LSR", "mode": "absolute_x", "cycles": 7},
    
    # NOP - No Operation
    0xEA: {"name": "NOP", "mode": "implied", "cycles": 2},
    
    # ORA - Logical Inclusive OR
    0x09: {"name": "ORA", "mode": "immediate", "cycles": 2},
    0x05: {"name": "ORA", "mode": "zeropage", "cycles": 3},
    0x15: {"name": "ORA", "mode": "zeropage_x", "cycles": 4},
    0x0D: {"name": "ORA", "mode": "absolute", "cycles": 4},
    0x1D: {"name": "ORA", "mode": "absolute_x", "cycles": 4},
    0x19: {"name": "ORA", "mode": "absolute_y", "cycles": 4},
    0x01: {"name": "ORA", "mode": "indirect_x", "cycles": 6},
    0x11: {"name": "ORA", "mode": "indirect_y", "cycles": 5},
    
    # PHA - Push Accumulator
    0x48: {"name": "PHA", "mode": "implied", "cycles": 3},
    
    # PHP - Push Processor Status
    0x08: {"name": "PHP", "mode": "implied", "cycles": 3},
    
    # PLA - Pull Accumulator
    0x68: {"name": "PLA", "mode": "implied", "cycles": 4},
    
    # PLP - Pull Processor Status
    0x28: {"name": "PLP", "mode": "implied", "cycles": 4},
    
    # ROL - Rotate Left
    0x2A: {"name": "ROL_A", "mode": "implied", "cycles": 2},
    0x26: {"name": "ROL", "mode": "zeropage", "cycles": 5},
    0x36: {"name": "ROL", "mode": "zeropage_x", "cycles": 6},
    0x2E: {"name": "ROL", "mode": "absolute", "cycles": 6},
    0x3E: {"name": "ROL", "mode": "absolute_x", "cycles": 7},
    
    # ROR - Rotate Right
    0x6A: {"name": "ROR_A", "mode": "implied", "cycles": 2},
    0x66: {"name": "ROR", "mode": "zeropage", "cycles": 5},
    0x76: {"name": "ROR", "mode": "zeropage_x", "cycles": 6},
    0x6E: {"name": "ROR", "mode": "absolute", "cycles": 6},
    0x7E: {"name": "ROR", "mode": "absolute_x", "cycles": 7},
    
    # RTI - Return from Interrupt
    0x40: {"name": "RTI", "mode": "implied", "cycles": 6},
    
    # RTS - Return from Subroutine
    0x60: {"name": "RTS", "mode": "implied", "cycles": 6},
    
    # SBC - Subtract with Carry
    0xE9: {"name": "SBC", "mode": "immediate", "cycles": 2},
    0xE5: {"name": "SBC", "mode": "zeropage", "cycles": 3},
    0xF5: {"name": "SBC", "mode": "zeropage_x", "cycles": 4},
    0xED: {"name": "SBC", "mode": "absolute", "cycles": 4},
    0xFD: {"name": "SBC", "mode": "absolute_x", "cycles": 4},
    0xF9: {"name": "SBC", "mode": "absolute_y", "cycles": 4},
    0xE1: {"name": "SBC", "mode": "indirect_x", "cycles": 6},
    0xF1: {"name": "SBC", "mode": "indirect_y", "cycles": 5},
    
    # SEC - Set Carry Flag
    0x38: {"name": "SEC", "mode": "implied", "cycles": 2},
    
    # SED - Set Decimal Flag
    0xF8: {"name": "SED", "mode": "implied", "cycles": 2},
    
    # SEI - Set Interrupt Disable
    0x78: {"name": "SEI", "mode": "implied", "cycles": 2},
    
    # STA - Store Accumulator
    0x85: {"name": "STA", "mode": "zeropage", "cycles": 3},
    0x95: {"name": "STA", "mode": "zeropage_x", "cycles": 4},
    0x8D: {"name": "STA", "mode": "absolute", "cycles": 4},
    0x9D: {"name": "STA", "mode": "absolute_x", "cycles": 5},
    0x99: {"name": "STA", "mode": "absolute_y", "cycles": 5},
    0x81: {"name": "STA", "mode": "indirect_x", "cycles": 6},
    0x91: {"name": "STA", "mode": "indirect_y", "cycles": 6},
    
    # STX - Store X Register
    0x86: {"name": "STX", "mode": "zeropage", "cycles": 3},
    0x96: {"name": "STX", "mode": "zeropage_y", "cycles": 4},
    0x8E: {"name": "STX", "mode": "absolute", "cycles": 4},
    
    # STY - Store Y Register
    0x84: {"name": "STY", "mode": "zeropage", "cycles": 3},
    0x94: {"name": "STY", "mode": "zeropage_x", "cycles": 4},
    0x8C: {"name": "STY", "mode": "absolute", "cycles": 4},
    
    # TAX - Transfer A to X
    0xAA: {"name": "TAX", "mode": "implied", "cycles": 2},
    
    # TAY - Transfer A to Y
    0xA8: {"name": "TAY", "mode": "implied", "cycles": 2},
    
    # TSX - Transfer SP to X
    0xBA: {"name": "TSX", "mode": "implied", "cycles": 2},
    
    # TXA - Transfer X to A
    0x8A: {"name": "TXA", "mode": "implied", "cycles": 2},
    
    # TXS - Transfer X to SP
    0x9A: {"name": "TXS", "mode": "implied", "cycles": 2},
    
    # TYA - Transfer Y to A
    0x98: {"name": "TYA", "mode": "implied", "cycles": 2},
}
