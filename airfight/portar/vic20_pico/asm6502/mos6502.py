
from instr6502 import INSTRUCTION_TABLE

class MOS6502:

    # status register flags
    N = 7  # Negative
    V = 6  # Overflow
    U = 5  # Unused (always 1)
    B = 4  # Break
    D = 3  # Decimal
    I = 2  # Interrupt Disable
    Z = 1  # Zero
    C = 0  # Carry

    def __init__(self):
        # registers
        self.A = 0x00      # Accumulator
        self.X = 0x00      # X Index
        self.Y = 0x00      # Y Index
        self.SP = 0xFF     # Stack Pointer (starts at 0xFF, grows downward)
        self.PC = 0x0000   # Program Counter
        self.SR = 0x34     # Status Register (U and I initially set)
        
        # 64K
        self.memory = [0x00] * 0x10000
        
        # cycle counter
        self.cycles = 0
        
        # init instruction table
        self.instructions = {}
        self.init_instructions()

    def init_instructions(self):
        self.instructions = INSTRUCTION_TABLE

    # methods for bit manipulation
    def get_bit(self, value, bit):
        return (value >> bit) & 1
    
    def set_bit(self, value, bit):
        return value | (1 << bit)
    
    def clear_bit(self, value, bit):
        return value & ~(1 << bit)
    
    # memory access methods
    def read_byte(self, addr):
        return self.memory[addr & 0xFFFF]
    
    def write_byte(self, addr, value):
        addr &= 0xFFFF
        value &= 0xFF
        self.memory[addr] = value
    
    def read_word(self, addr):
        low = self.read_byte(addr)
        high = self.read_byte((addr + 1) & 0xFFFF)
        return (high << 8) | low
    
    def write_word(self, addr, value):
        low = value & 0xFF
        high = (value >> 8) & 0xFF
        self.write_byte(addr, low)
        self.write_byte((addr + 1) & 0xFFFF, high)
    
    # stack operations
    def push_byte(self, value):
        self.write_byte(0x0100 + self.SP, value)
        self.SP = (self.SP - 1) & 0xFF
    
    def pop_byte(self):
        self.SP = (self.SP + 1) & 0xFF
        return self.read_byte(0x0100 + self.SP)
    
    def push_word(self, value):
        high = (value >> 8) & 0xFF
        low = value & 0xFF
        self.push_byte(high)
        self.push_byte(low)
    
    def pop_word(self):
        low = self.pop_byte()
        high = self.pop_byte()
        return (high << 8) | low
    
    # flag operations
    def update_zn_flags(self, value):
        # Zero flag
        self.SR = self.set_bit(self.SR, self.Z) if (value & 0xFF) == 0 else self.clear_bit(self.SR, self.Z)
        # Negative flag
        self.SR = self.set_bit(self.SR, self.N) if value & 0x80 else self.clear_bit(self.SR, self.N)


    # addressing modes (return effective address)
    def addr_immediate(self):
        addr = self.PC
        self.PC += 1
        return addr
    
    def addr_zeropage(self):
        addr = self.read_byte(self.PC)
        self.PC += 1
        return addr & 0xFF
    
    def addr_zeropage_x(self):
        addr = (self.read_byte(self.PC) + self.X) & 0xFF
        self.PC += 1
        return addr
    
    def addr_zeropage_y(self):
        addr = (self.read_byte(self.PC) + self.Y) & 0xFF
        self.PC += 1
        return addr
    
    def addr_absolute(self):
        addr = self.read_word(self.PC)
        self.PC += 2
        return addr
    
    def addr_absolute_x(self):
        base = self.read_word(self.PC)
        self.PC += 2
        return (base + self.X) & 0xFFFF
    
    def addr_absolute_y(self):
        base = self.read_word(self.PC)
        self.PC += 2
        return (base + self.Y) & 0xFFFF
    
    def addr_indirect(self):
        ptr = self.read_word(self.PC)
        self.PC += 2
        
        # 6502 bug: if pointer is at page boundary
        if (ptr & 0xFF) == 0xFF:
            low = self.read_byte(ptr)
            high = self.read_byte(ptr & 0xFF00)
        else:
            low = self.read_byte(ptr)
            high = self.read_byte(ptr + 1)
        
        return (high << 8) | low
    
    def addr_indirect_x(self):
        ptr = (self.read_byte(self.PC) + self.X) & 0xFF
        self.PC += 1
        
        low = self.read_byte(ptr & 0xFF)
        high = self.read_byte((ptr + 1) & 0xFF)
        
        return (high << 8) | low
    
    def addr_indirect_y(self):
        ptr = self.read_byte(self.PC)
        self.PC += 1

        low = self.read_byte(ptr & 0xFF)
        high = self.read_byte((ptr + 1) & 0xFF)
    
        #return ((high << 8) | low + self.Y) & 0xFFFF
        return ((high << 8) | low) + self.Y & 0xFFFF

    def addr_relative(self):
        offset = self.read_byte(self.PC)
        print(f"Read branch offset {offset:02X} at {self.PC:04X}")
        self.PC += 1
        offset = offset if offset < 0x80 else offset - 0x100
        target = (self.PC + offset) & 0xFFFF
        print(f"Calculated branch target: {self.PC:04X} + {offset} = {target:04X}")
        return target


    # -------------------------
    # Instructions
    def ADC(self, addr):
        value = self.read_byte(addr)
        
        if self.get_bit(self.SR, self.D):  # Decimal mode
            # Simplified decimal mode implementation
            a_low = (self.A & 0x0F) + (value & 0x0F) + self.get_bit(self.SR, self.C)
            a_high = (self.A >> 4) + (value >> 4)
            
            if a_low > 9:
                a_low -= 10
                a_high += 1
                
            if a_high > 9:
                a_high -= 10
                self.SR = self.set_bit(self.SR, self.C)
            else:
                self.SR = self.clear_bit(self.SR, self.C)
                
            result = ((a_high & 0x0F) << 4) | (a_low & 0x0F)
        else:  # Binary mode
            result = self.A + value + self.get_bit(self.SR, self.C)
            
            # Set carry flag
            self.SR = self.set_bit(self.SR, self.C) if result > 0xFF else self.clear_bit(self.SR, self.C)
            
            # Set overflow flag - occurs when sign of result differs from both operands
            if (~(self.A ^ value) & (self.A ^ result) & 0x80) != 0:
                self.SR = self.set_bit(self.SR, self.V)
            else:
                self.SR = self.clear_bit(self.SR, self.V)
                
            result = result & 0xFF
            
        self.A = result
        self.update_zn_flags(self.A)
        
    def AND(self, addr):
        value = self.read_byte(addr)
        self.A &= value
        self.update_zn_flags(self.A)
        
    def ASL(self, addr):
        value = self.read_byte(addr)
        self.SR = self.set_bit(self.SR, self.C) if value & 0x80 else self.clear_bit(self.SR, self.C)
        value = (value << 1) & 0xFF
        self.write_byte(addr, value)
        self.update_zn_flags(value)
        
    def ASL_A(self):
        self.SR = self.set_bit(self.SR, self.C) if self.A & 0x80 else self.clear_bit(self.SR, self.C)
        self.A = (self.A << 1) & 0xFF
        self.update_zn_flags(self.A)
        
    def BCC(self, addr):
        if not self.get_bit(self.SR, self.C):
            print(f"BCC taken from {self.PC:04X} to {addr:04X}")
            # Add 1 cycle for taken branch (total = 3)
            self.cycles += 1
            # Add 1 more cycle if crossing page boundary (total = 4)
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr        

    def BCS(self, addr):
        if self.get_bit(self.SR, self.C):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
        
    def BEQ(self, addr):
        if self.get_bit(self.SR, self.Z):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
            
    def BIT(self, addr):
        value = self.read_byte(addr)
        # Set Z flag based on AND with accumulator
        if (self.A & value) == 0:
            self.SR = self.set_bit(self.SR, self.Z)
        else:
            self.SR = self.clear_bit(self.SR, self.Z)
        # Copy bits 6 and 7 of value to status register
        self.SR = (self.SR & ~(1 << self.V)) | ((value >> 6) & 1) << self.V
        self.SR = (self.SR & ~(1 << self.N)) | ((value >> 7) & 1) << self.N
            
    def BMI(self, addr):
        if self.get_bit(self.SR, self.N):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
            
    def BNE(self, addr):
        if not self.get_bit(self.SR, self.Z):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
            
    def BPL(self, addr):
        if not self.get_bit(self.SR, self.N):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
            
    def BRK(self):
        self.PC += 1  # Skip signature byte
        
        # Push PC to stack
        self.push_word(self.PC)
        
        # Push status with B flag set
        status = self.set_bit(self.SR, self.B)
        # The unused flag is always set
        status = self.set_bit(status, self.U)
        self.push_byte(status)
        
        # Set I flag
        self.SR = self.set_bit(self.SR, self.I)
        
        # Load interrupt vector
        self.PC = self.read_word(0xFFFE)
            
    def BVC(self, addr):
        if not self.get_bit(self.SR, self.V):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
            
    def BVS(self, addr):
        if self.get_bit(self.SR, self.V):
            self.cycles += 1
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
            self.PC = addr
            
    def CLC(self):
        self.SR = self.clear_bit(self.SR, self.C)
        
    def CLD(self):
        self.SR = self.clear_bit(self.SR, self.D)
        
    def CLI(self):
        self.SR = self.clear_bit(self.SR, self.I)
        
    def CLV(self):
        self.SR = self.clear_bit(self.SR, self.V)
        
    def CMP(self, addr):
        value = self.read_byte(addr)
        result = (self.A - value) & 0xFF
        
        # Set carry flag if A >= M
        self.SR = self.set_bit(self.SR, self.C) if self.A >= value else self.clear_bit(self.SR, self.C)
        self.update_zn_flags(result)

    def CPX(self, addr):
        value = self.read_byte(addr)
        result = (self.X - value) & 0xFF
        
        # Set Carry flag (X >= value)
        self.SR = self.set_bit(self.SR, self.C) if self.X >= value else self.clear_bit(self.SR, self.C)
        
        # Update Z and N flags from result
        self.update_zn_flags(result)
        
        # Debug print
        print(f"CPX: X={self.X:02X} cmp {value:02X} -> result={result:02X}, "
            f"Z={self.get_bit(self.SR, self.Z)}, C={self.get_bit(self.SR, self.C)}")

    def CPY(self, addr):
        value = self.read_byte(addr)
        result = (self.Y - value) & 0xFF
        
        self.SR = self.set_bit(self.SR, self.C) if self.Y >= value else self.clear_bit(self.SR, self.C)
        self.update_zn_flags(result)
        
    def DEC(self, addr):
        value = (self.read_byte(addr) - 1) & 0xFF
        self.write_byte(addr, value)
        self.update_zn_flags(value)
        
    def DEX(self):
        self.X = (self.X - 1) & 0xFF
        self.update_zn_flags(self.X)
        
    def DEY(self):
        self.Y = (self.Y - 1) & 0xFF
        self.update_zn_flags(self.Y)
        
    def EOR(self, addr):
        value = self.read_byte(addr)
        self.A ^= value
        self.update_zn_flags(self.A)
        
    def INC(self, addr):
        value = (self.read_byte(addr) + 1) & 0xFF
        self.write_byte(addr, value)
        self.update_zn_flags(value)
        
    def INX(self):
        self.X = (self.X + 1) & 0xFF
        self.update_zn_flags(self.X)
        
    def INY(self):
        self.Y = (self.Y + 1) & 0xFF
        self.update_zn_flags(self.Y)
        
    def JMP(self, addr):
        self.PC = addr
        
    def JSR(self, addr):
        # Push return address (PC-1) to stack! 
        # PC points to the next instruction, so we need to subtract 1
        self.push_word(self.PC - 1)
        self.PC = addr

    def LDA(self, addr):
        self.A = self.read_byte(addr)
        self.update_zn_flags(self.A)
        
    def LDX(self, addr):
        self.X = self.read_byte(addr)
        self.update_zn_flags(self.X)
        
    def LDY(self, addr):
        self.Y = self.read_byte(addr)
        self.update_zn_flags(self.Y)
        
    def LSR(self, addr):
        value = self.read_byte(addr)
        self.SR = self.set_bit(self.SR, self.C) if value & 0x01 else self.clear_bit(self.SR, self.C)
        value = value >> 1
        self.write_byte(addr, value)
        self.update_zn_flags(value)
        
    def LSR_A(self):
        self.SR = self.set_bit(self.SR, self.C) if self.A & 0x01 else self.clear_bit(self.SR, self.C)
        self.A = self.A >> 1
        self.update_zn_flags(self.A)
        
    def NOP(self):
        pass
        
    def ORA(self, addr):
        value = self.read_byte(addr)
        self.A |= value
        self.update_zn_flags(self.A)
        
    def PHA(self):
        self.push_byte(self.A)
        
    def PHP(self):
        # Push status with B flag set
        status = self.set_bit(self.SR, self.B)
        # The unused flag is always set
        status = self.set_bit(status, self.U)
        self.push_byte(status)
        
    def PLA(self):
        self.A = self.pop_byte()
        self.update_zn_flags(self.A)
        
    def PLP(self):
        self.SR = self.pop_byte()
        # Ensure unused flag is always set
        self.SR = self.set_bit(self.SR, self.U)
        # Break flag is not actually in the status register
        self.SR = self.clear_bit(self.SR, self.B)
        
    def ROL(self, addr):
        value = self.read_byte(addr)
        carry = self.get_bit(self.SR, self.C)
        self.SR = self.set_bit(self.SR, self.C) if value & 0x80 else self.clear_bit(self.SR, self.C)
        value = ((value << 1) & 0xFF) | carry
        self.write_byte(addr, value)
        self.update_zn_flags(value)
        
    def ROL_A(self):
        carry = self.get_bit(self.SR, self.C)
        self.SR = self.set_bit(self.SR, self.C) if self.A & 0x80 else self.clear_bit(self.SR, self.C)
        self.A = ((self.A << 1) & 0xFF) | carry
        self.update_zn_flags(self.A)
        
    def ROR(self, addr):
        value = self.read_byte(addr)
        carry = self.get_bit(self.SR, self.C)
        self.SR = self.set_bit(self.SR, self.C) if value & 0x01 else self.clear_bit(self.SR, self.C)
        value = (value >> 1) | (carry << 7)
        self.write_byte(addr, value)
        self.update_zn_flags(value)
        
    def ROR_A(self):
        carry = self.get_bit(self.SR, self.C)
        self.SR = self.set_bit(self.SR, self.C) if self.A & 0x01 else self.clear_bit(self.SR, self.C)
        self.A = (self.A >> 1) | (carry << 7)
        self.update_zn_flags(self.A)
        
    def RTI(self):
        # Pull processor status
        self.SR = self.pop_byte()
        # Ensure unused flag is always set
        self.SR = self.set_bit(self.SR, self.U)
        # Break flag is not actually in the status register
        self.SR = self.clear_bit(self.SR, self.B)
        
        # Pull program counter
        self.PC = self.pop_word()
        
    def RTS(self):
        # Pull program counter
        self.PC = self.pop_word()
        self.PC += 1  # RTS returns to address+1
        
    def SBC(self, addr):
        value = self.read_byte(addr) ^ 0xFF  # Invert for subtraction
        
        if self.get_bit(self.SR, self.D):  # Decimal mode
            # BCD subtraction
            a_low = (self.A & 0x0F) + (value & 0x0F) + self.get_bit(self.SR, self.C)
            a_high = (self.A >> 4) + (value >> 4)
            
            if a_low > 9:
                a_low -= 10
                a_high += 1
                
            if a_high > 9:
                a_high -= 10
                self.SR = self.set_bit(self.SR, self.C)
            else:
                self.SR = self.clear_bit(self.SR, self.C)
                
            result = ((a_high & 0x0F) << 4) | (a_low & 0x0F)
        else:  # Binary mode
            result = self.A + value + self.get_bit(self.SR, self.C)
            
            # Set carry flag
            self.SR = self.set_bit(self.SR, self.C) if result > 0xFF else self.clear_bit(self.SR, self.C)
            
            # Set overflow flag
            if ((~(self.A ^ value)) & (self.A ^ result) & 0x80) != 0:
                self.SR = self.set_bit(self.SR, self.V)
            else:
                self.SR = self.clear_bit(self.SR, self.V)
                
            result = result & 0xFF
            
        self.A = result
        self.update_zn_flags(self.A)
        
    def SEC(self):
        self.SR = self.set_bit(self.SR, self.C)
        
    def SED(self):
        self.SR = self.set_bit(self.SR, self.D)
        
    def SEI(self):
        self.SR = self.set_bit(self.SR, self.I)
        
    def STA(self, addr):
        self.write_byte(addr, self.A)
        
    def STX(self, addr):
        self.write_byte(addr, self.X)
        
    def STY(self, addr):
        self.write_byte(addr, self.Y)
        
    def TAX(self):
        self.X = self.A
        self.update_zn_flags(self.X)
        
    def TAY(self):
        self.Y = self.A
        self.update_zn_flags(self.Y)
        
    def TSX(self):
        self.X = self.SP
        self.update_zn_flags(self.X)
        
    def TXA(self):
        self.A = self.X
        self.update_zn_flags(self.A)
        
    def TXS(self):
        self.SP = self.X
        
    def TYA(self):
        self.A = self.Y
        self.update_zn_flags(self.A)

    def print_state(self):
        print(f"PC: 0x{self.PC:04X}  A: 0x{self.A:02X}  X: 0x{self.X:02X}  Y: 0x{self.Y:02X}  SP: 0x{self.SP:02X}")
        print(f"Status: {'N' if self.get_bit(self.SR, self.N) else '-'}"
              f"{'V' if self.get_bit(self.SR, self.V) else '-'}"
              f"{'U' if self.get_bit(self.SR, self.U) else '-'}"
              f"{'B' if self.get_bit(self.SR, self.B) else '-'}"
              f"{'D' if self.get_bit(self.SR, self.D) else '-'}"
              f"{'I' if self.get_bit(self.SR, self.I) else '-'}"
              f"{'Z' if self.get_bit(self.SR, self.Z) else '-'}"
              f"{'C' if self.get_bit(self.SR, self.C) else '-'}")
        print(f"Cycles: {self.cycles}")

    def execute_instruction(self):
        opcode = self.read_byte(self.PC)
        self.PC += 1
        
        if opcode not in self.instructions:
            raise ValueError(f"Unknown opcode: 0x{opcode:02X} at 0x{self.PC-1:04X}")
            
        instruction = self.instructions[opcode]
        self.cycles += instruction["cycles"]
        
        # Handle different addressing modes
        if instruction["mode"] == "implied":
            getattr(self, instruction["name"])()
        elif instruction["mode"] == "immediate":
            addr = self.addr_immediate()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "zeropage":
            addr = self.addr_zeropage()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "zeropage_x":
            addr = self.addr_zeropage_x()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "zeropage_y":
            addr = self.addr_zeropage_y()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "absolute":
            addr = self.addr_absolute()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "absolute_x":
            addr = self.addr_absolute_x()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "absolute_y":
            addr = self.addr_absolute_y()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "indirect":
            addr = self.addr_indirect()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "indirect_x":
            addr = self.addr_indirect_x()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "indirect_y":
            addr = self.addr_indirect_y()
            getattr(self, instruction["name"])(addr)
        elif instruction["mode"] == "relative":
            addr = self.addr_relative()
            getattr(self, instruction["name"])(addr)
        
    def run(self, addr=None, steps=None):
        if addr is not None:
            self.PC = addr
            
        step_count = 0
        try:
            while steps is None or step_count < steps:
                self.execute_instruction()
                step_count += 1
        except Exception as e:
            print(f"Error: {e}")
            self.print_state()

    def load_program(self, program, start_addr=0x8000):
        for i, byte in enumerate(program):
            self.write_byte(start_addr + i, byte)
        
        self.PC = start_addr

    def reset(self):
        self.__init__()  # Reinitialize to reset all state
        
    def disassemble(self, addr, num_instructions=10):
        for _ in range(num_instructions):
            if addr >= 0xFFFF:
                break
                
            opcode = self.read_byte(addr)
            if opcode not in self.instructions:
                print(f"Unknown opcode: 0x{opcode:02X} at 0x{addr:04X}")
                addr += 1
                continue
                
            instruction = self.instructions[opcode]
            size = 1
            operand = ""
            
            if instruction["mode"] == "immediate":
                size = 2
                operand = f"#${self.read_byte(addr+1):02X}"
            elif instruction["mode"] == "zeropage":
                size = 2
                operand = f"${self.read_byte(addr+1):02X}"
            elif instruction["mode"] == "zeropage_x":
                size = 2
                operand = f"${self.read_byte(addr+1):02X},X"
            elif instruction["mode"] == "zeropage_y":
                size = 2
                operand = f"${self.read_byte(addr+1):02X},Y"
            elif instruction["mode"] == "absolute":
                size = 3
                operand = f"${self.read_word(addr+1):04X}"
            elif instruction["mode"] == "absolute_x":
                size = 3
                operand = f"${self.read_word(addr+1):04X},X"
            elif instruction["mode"] == "absolute_y":
                size = 3
                operand = f"${self.read_word(addr+1):04X},Y"
            elif instruction["mode"] == "indirect":
                size = 3
                operand = f"(${self.read_word(addr+1):04X})"
            elif instruction["mode"] == "indirect_x":
                size = 2
                operand = f"(${self.read_byte(addr+1):02X},X)"
            elif instruction["mode"] == "indirect_y":
                size = 2
                operand = f"(${self.read_byte(addr+1):02X}),Y"
            elif instruction["mode"] == "relative":
                size = 2
                offset = self.read_byte(addr+1)
                target = (addr + 2 + (offset if offset < 0x80 else offset - 0x100)) & 0xFFFF
                operand = f"${target:04X}"
            
            bytes_str = " ".join(f"{self.read_byte(addr+i):02X}" for i in range(size))
            print(f"0x{addr:04X}: {bytes_str.ljust(8)} {instruction['name']} {operand}")
            addr += size

def hex_dump(cpu, start_addr, end_addr):
    print(f"Memory dump from ${start_addr:04X} to ${end_addr:04X}:")
    for addr in range(start_addr, end_addr + 1, 16):
        line = f"{addr:04X}: "
        for i in range(16):
            if addr + i <= end_addr:
                line += f"{cpu.read_byte(addr + i):02X} "
            else:
                line += "   "
        line += " "
        for i in range(16):
            if addr + i <= end_addr:
                char = cpu.read_byte(addr + i)
                line += chr(char) if 32 <= char < 127 else "."
        print(line)

