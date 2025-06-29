import sys
import struct

REG = {
    'R0': 0, 'R1': 1, 'R2': 2, 'R3': 3,
    'R4': 4, 'R5': 5, 'R6': 6, 'R7': 7
}

OPCODES = {
    'ADD': 0x1, 'AND': 0x5, 'NOT': 0x9,
    'BR':  0x0, 'BRN': 0x0, 'BRZ': 0x0, 'BRP': 0x0,
    'BRNZ': 0x0, 'BRNP': 0x0, 'BRZP': 0x0, 'BRNZP': 0x0,
    'JMP': 0xC, 'JSR': 0x4,
    'LD':  0x2, 'ST':  0x3,
    'TRAP': 0xF, 'LEA': 0xE,
    'RET': 0xC
}

def parse_line(line):
    line = line.split(';')[0].strip()
    if not line:
        return None
    return [token.strip(',') for token in line.split()]

def emit(pc, tokens, instr, output):
    print(f"[ASM] {pc:04X}: {tokens} → {instr:04X}")
    output.append(instr)

def to_signed_imm(x, bits):
    if x.startswith('#'):
        n = int(x[1:], 10)
    elif x.startswith('x'):
        n = int(x[1:], 16)
    else:
        n = int(x, 10)
    if n < 0:
        n = (1 << bits) + n
    return n & ((1 << bits) - 1)

def assemble(filename):
    with open(filename, encoding='utf-8') as f:
        lines = f.readlines()

    orig = 0x3000
    labels = {}
    program = []

    # First pass: collect labels
    pc = orig
    for line in lines:
        tokens = parse_line(line)
        if not tokens:
            continue

        if tokens[0].upper() == '.ORIG':
            orig = int(tokens[1][1:], 16)
            pc = orig
            continue

        label = None
        if tokens[0].endswith(':'):
            label = tokens[0][:-1]
            tokens = tokens[1:]
        elif tokens[0].upper() not in OPCODES and not tokens[0].startswith('.'):
            label = tokens[0]
            tokens = tokens[1:]

        if label:
            labels[label] = pc

        if tokens:
            program.append((pc, tokens))
            pc += 1

    print(f"[DEBUG] Labels: {labels}")



    # Second pass: generate instructions
    output = []

    for pc, tokens in program:
        op = tokens[0].upper()
        if op in OPCODES:
            opcode = OPCODES[op]
            if op == 'ADD' or op == 'AND':
                dr = REG[tokens[1]]
                sr1 = REG[tokens[2]]
                if tokens[3].startswith('R'):
                    sr2 = REG[tokens[3]]
                    instr = (opcode << 12) | (dr << 9) | (sr1 << 6) | sr2
                else:
                    imm5 = to_signed_imm(tokens[3], 5)
                    instr = (opcode << 12) | (dr << 9) | (sr1 << 6) | (1 << 5) | imm5
                emit(pc, tokens, instr, output)
            elif op == 'NOT':
                dr = REG[tokens[1]]
                sr = REG[tokens[2]]
                instr = (opcode << 12) | (dr << 9) | (sr << 6) | 0x3F
                emit(pc, tokens, instr, output)
            elif op.startswith('BR'):
                cond = 0
                if 'n' in op: cond |= 0x4
                if 'z' in op: cond |= 0x2
                if 'p' in op: cond |= 0x1
                label = tokens[1]
                raw_offset = labels[label] - (pc + 1)
                print(f"[DEBUG] BR target {label} at {labels[label]:04X}, offset = {raw_offset}")
                offset = to_signed_imm(str(raw_offset), 9)
                instr = (0x0 << 12) | (cond << 9) | offset
                emit(pc, tokens, instr, output)
            elif op == 'LEA':
                dr = REG[tokens[1]]
                label = tokens[2]
                offset = to_signed_imm(str(labels[label] - (pc + 1)), 9)
                instr = (opcode << 12) | (dr << 9) | offset
                emit(pc, tokens, instr, output)
            elif op == 'JMP':
                base = REG[tokens[1]]
                instr = (opcode << 12) | (base << 6)
                emit(pc, tokens, instr, output)
            elif op == 'JSR':
                label = tokens[1]
                offset = to_signed_imm(str(labels[label] - (pc + 1)), 11)
                instr = (opcode << 12) | (1 << 11) | offset
                emit(pc, tokens, instr, output)
            elif op == 'TRAP':
                trapvect = to_signed_imm(tokens[1], 8)
                instr = (opcode << 12) | trapvect
                emit(pc, tokens, instr, output)
            elif op == 'RET':
                instr = (opcode << 12) | (7 << 6)
                emit(pc, tokens, instr, output)
        elif op == '.FILL':
            val = to_signed_imm(tokens[1], 16)
            output.append(val)
        elif op == '.END':
            continue

    objname = filename.replace('.asm', '.obj')
    with open(objname, 'wb') as out:
        out.write(struct.pack('>H', orig))
        for word in output:
            out.write(struct.pack('>H', word))

    print(f"Wrote {objname} with origin x{orig:04X} and {len(output)} words")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python asm.py file.asm")
        sys.exit(1)
    assemble(sys.argv[1])
