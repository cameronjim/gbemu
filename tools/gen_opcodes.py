#!/usr/bin/env python3
# generates core/opcodes.inc and tests/unit/opcode_meta.inc from tools/Opcodes.json
# regenerate, never hand-edit the outputs
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "tools" / "Opcodes.json"
INC_PATH = ROOT / "core" / "opcodes.inc"
META_PATH = ROOT / "tests" / "unit" / "opcode_meta.inc"

R8 = {"A": "regs_.a", "B": "regs_.b", "C": "regs_.c", "D": "regs_.d", "E": "regs_.e", "H": "regs_.h", "L": "regs_.l"}
R16_GET = {"BC": "bc()", "DE": "de()", "HL": "hl()", "SP": "sp()", "AF": "af()"}
R16_SET = {"BC": "set_bc({})", "DE": "set_de({})", "HL": "set_hl({})", "SP": "regs_.sp = {}", "AF": "set_af({})"}
COND = {"NZ": "!flag_z()", "Z": "flag_z()", "NC": "!flag_c()", "C": "flag_c()"}
CONTROL = {"JP", "JR", "CALL", "RET", "RETI", "RST"}
ALU = {"ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP"}
ROT_A = {"RLCA": "alu_rlc", "RLA": "alu_rl", "RRCA": "alu_rrc", "RRA": "alu_rr"}
CB_SHIFT = {"RLC": "alu_rlc", "RRC": "alu_rrc", "RL": "alu_rl", "RR": "alu_rr",
            "SLA": "alu_sla", "SRA": "alu_sra", "SWAP": "alu_swap", "SRL": "alu_srl"}
CARRY_IN = {"alu_rl", "alu_rr"}


def op_text(info):
    parts = []
    for o in info["operands"]:
        name = o["name"]
        if o.get("increment"):
            name += "+"
        if o.get("decrement"):
            name += "-"
        if not o.get("immediate", True):
            name = "(" + name + ")"
        parts.append(name)
    text = info["mnemonic"].lower()
    if parts:
        text += " " + ", ".join(parts).lower()
    return text


def is_mem8(o):
    # an 8-bit memory place: (bc) (de) (hl) (hl+) (hl-) (a16) (a8) (c)
    return not o.get("immediate", True)


def read8_expr(o, lines):
    # returns c++ expression reading the 8-bit operand; may emit setup lines
    name = o["name"]
    if o.get("immediate", True):
        if name in R8:
            return R8[name]
        if name == "n8":
            return "fetch8()"
        raise ValueError(f"read8 {name}")
    if name in ("BC", "DE"):
        return f"bus_.read8({R16_GET[name]})"
    if name == "HL":
        expr = "bus_.read8(hl())"
        if o.get("increment"):
            lines.append("const uint8_t m = " + expr + ";")
            lines.append("set_hl(static_cast<uint16_t>(hl() + 1));")
            return "m"
        if o.get("decrement"):
            lines.append("const uint8_t m = " + expr + ";")
            lines.append("set_hl(static_cast<uint16_t>(hl() - 1));")
            return "m"
        return expr
    if name == "a16":
        return "bus_.read8(fetch16())"
    if name == "a8":
        return "bus_.read8(static_cast<uint16_t>(0xFF00 | fetch8()))"
    if name == "C":
        return "bus_.read8(static_cast<uint16_t>(0xFF00 | regs_.c))"
    raise ValueError(f"read8 mem {name}")


def write8_stmts(o, value_expr):
    # returns statements writing value_expr into the 8-bit operand
    name = o["name"]
    if o.get("immediate", True):
        if name in R8:
            return [f"{R8[name]} = {value_expr};"]
        raise ValueError(f"write8 {name}")
    if name in ("BC", "DE"):
        return [f"bus_.write8({R16_GET[name]}, {value_expr});"]
    if name == "HL":
        out = [f"bus_.write8(hl(), {value_expr});"]
        if o.get("increment"):
            out.append("set_hl(static_cast<uint16_t>(hl() + 1));")
        if o.get("decrement"):
            out.append("set_hl(static_cast<uint16_t>(hl() - 1));")
        return out
    if name == "a16":
        return [f"bus_.write8(fetch16(), {value_expr});"]
    if name == "a8":
        return [f"bus_.write8(static_cast<uint16_t>(0xFF00 | fetch8()), {value_expr});"]
    if name == "C":
        return [f"bus_.write8(static_cast<uint16_t>(0xFF00 | regs_.c), {value_expr});"]
    raise ValueError(f"write8 mem {name}")


def gen_ld(info):
    ops = info["operands"]
    lines = []
    if len(ops) == 3:
        # ld hl, sp+e8: h/c from the low-byte add, z=0
        lines.append("const int8_t off = static_cast<int8_t>(fetch8());")
        lines.append("const Alu16Result r = alu_add_sp_e8(sp(), off);")
        lines.append("set_hl(r.value);")
        lines.append("regs_.f = r.flags;")
        return lines
    dst, src = ops
    if dst["name"] == "a16" and src["name"] == "SP":
        lines.append("const uint16_t addr = fetch16();")
        lines.append("bus_.write8(addr, static_cast<uint8_t>(sp() & 0xFF));")
        lines.append("bus_.write8(static_cast<uint16_t>(addr + 1), static_cast<uint8_t>(sp() >> 8));")
        return lines
    if dst["name"] == "SP" and src["name"] == "HL":
        return ["regs_.sp = hl();"]
    if dst["name"] in R16_SET and src["name"] == "n16":
        return [R16_SET[dst["name"]].format("fetch16()") + ";"]
    # remaining forms are 8-bit moves
    src_expr = read8_expr(src, lines)
    lines.extend(write8_stmts(dst, src_expr))
    return lines


def gen_alu(info):
    m = info["mnemonic"]
    ops = info["operands"]
    lines = []
    if m == "ADD" and ops[0]["name"] == "HL" and ops[0].get("immediate", True):
        lines.append(f"const Alu16Result r = alu_add16(hl(), {R16_GET[ops[1]['name']]}, regs_.f);")
        lines.append("set_hl(r.value);")
        lines.append("regs_.f = r.flags;")
        return lines
    if m == "ADD" and ops[0]["name"] == "SP":
        lines.append("const int8_t off = static_cast<int8_t>(fetch8());")
        lines.append("const Alu16Result r = alu_add_sp_e8(sp(), off);")
        lines.append("regs_.sp = r.value;")
        lines.append("regs_.f = r.flags;")
        return lines
    src = ops[1] if len(ops) == 2 else ops[0]
    src_expr = read8_expr(src, lines)
    if m == "CP":
        # cp is sub with the result discarded
        lines.append(f"regs_.f = alu_sub(regs_.a, {src_expr}).flags;")
        return lines
    call = {"ADD": f"alu_add(regs_.a, {src_expr})",
            "ADC": f"alu_adc(regs_.a, {src_expr}, flag_c())",
            "SUB": f"alu_sub(regs_.a, {src_expr})",
            "SBC": f"alu_sbc(regs_.a, {src_expr}, flag_c())",
            "AND": f"alu_and(regs_.a, {src_expr})",
            "XOR": f"alu_xor(regs_.a, {src_expr})",
            "OR": f"alu_or(regs_.a, {src_expr})"}[m]
    lines.append(f"const AluResult r = {call};")
    lines.append("regs_.a = r.value;")
    lines.append("regs_.f = r.flags;")
    return lines


def gen_incdec(info):
    m = info["mnemonic"]
    o = info["operands"][0]
    name = o["name"]
    if o.get("immediate", True) and name in R16_GET and name not in R8:
        delta = "+ 1" if m == "INC" else "- 1"
        return [R16_SET[name].format(f"static_cast<uint16_t>({R16_GET[name]} {delta})") + ";"]
    helper = "alu_inc" if m == "INC" else "alu_dec"
    lines = []
    src_expr = read8_expr(o, lines)
    lines.append(f"const AluResult r = {helper}({src_expr}, regs_.f);")
    lines.extend(write8_stmts(o, "r.value"))
    lines.append("regs_.f = r.flags;")
    return lines


def gen_jump(info, taken, not_taken):
    m = info["mnemonic"]
    ops = info["operands"]
    cond = None
    if ops and ops[0]["name"] in COND and (m == "RET" or len(ops) == 2):
        cond = COND[ops[0]["name"]]
    lines = []
    if m == "JR":
        lines.append("const int8_t off = static_cast<int8_t>(fetch8());")
        act = ["regs_.pc = static_cast<uint16_t>(regs_.pc + off);"]
    elif m == "JP" and ops[-1]["name"] == "HL":
        act = ["regs_.pc = hl();"]
    elif m == "JP":
        lines.append("const uint16_t target = fetch16();")
        act = ["regs_.pc = target;"]
    elif m == "CALL":
        lines.append("const uint16_t target = fetch16();")
        act = ["push16(regs_.pc);", "regs_.pc = target;"]
    elif m == "RET":
        act = ["regs_.pc = pop16();"]
    elif m == "RETI":
        # pandocs: reti enables ime with no delay
        act = ["regs_.pc = pop16();", "ime_ = true;", "ime_delay_ = 0;"]
    elif m == "RST":
        vec = int(ops[0]["name"].lstrip("$"), 16)
        act = ["push16(regs_.pc);", f"regs_.pc = 0x{vec:04X};"]
    else:
        raise ValueError(m)
    if cond is None:
        lines.extend(act)
        lines.append(f"return {taken};")
        return lines
    lines.append(f"if ({cond}) {{")
    lines.extend("    " + s for s in act)
    lines.append(f"    return {taken};")
    lines.append("}")
    lines.append(f"return {not_taken};")
    return lines


def gen_unprefixed(op, info):
    m = info["mnemonic"]
    cycles = info["cycles"]
    taken = cycles[0]
    lines = []
    if m == "NOP":
        pass
    elif m == "STOP":
        return ["return do_stop();"]
    elif m == "HALT":
        return ["return do_halt();"]
    elif m == "DI":
        # di also cancels a pending ei
        lines.append("ime_ = false;")
        lines.append("ime_delay_ = 0;")
    elif m == "EI":
        # ei takes effect one instruction late
        lines.append("ime_delay_ = 2;")
    elif m == "PREFIX":
        return ["return execute_cb(fetch8());"]
    elif m.startswith("ILLEGAL_"):
        return ["return trap_unknown(opcode);"]
    elif m in ("LD", "LDH"):
        lines = gen_ld(info)
    elif m in ("INC", "DEC"):
        lines = gen_incdec(info)
    elif m in ALU:
        lines = gen_alu(info)
    elif m in ROT_A:
        args = "regs_.a, flag_c()" if ROT_A[m] in CARRY_IN else "regs_.a"
        # a-register rotates always clear z
        lines.append(f"const AluResult r = {ROT_A[m]}({args});")
        lines.append("regs_.a = r.value;")
        lines.append("regs_.f = static_cast<uint8_t>(r.flags & ~kFlagZ);")
    elif m == "DAA":
        lines.append("const AluResult r = alu_daa(regs_.a, regs_.f);")
        lines.append("regs_.a = r.value;")
        lines.append("regs_.f = r.flags;")
    elif m == "CPL":
        lines.append("regs_.a = static_cast<uint8_t>(~regs_.a);")
        lines.append("regs_.f = static_cast<uint8_t>((regs_.f & (kFlagZ | kFlagC)) | kFlagN | kFlagH);")
    elif m == "SCF":
        lines.append("regs_.f = static_cast<uint8_t>((regs_.f & kFlagZ) | kFlagC);")
    elif m == "CCF":
        lines.append("regs_.f = static_cast<uint8_t>((regs_.f & kFlagZ) | ((regs_.f ^ kFlagC) & kFlagC));")
    elif m == "PUSH":
        lines.append(f"push16({R16_GET[info['operands'][0]['name']]});")
    elif m == "POP":
        lines.append(R16_SET[info["operands"][0]["name"]].format("pop16()") + ";")
    elif m in CONTROL:
        return gen_jump(info, taken, cycles[1] if len(cycles) == 2 else None)
    else:
        raise ValueError(f"unhandled {m} at {op:#04x}")
    lines.append(f"return {taken};")
    return lines


def gen_cb(op, info):
    m = info["mnemonic"]
    o = info["operands"][-1]
    cycles = info["cycles"][0]
    lines = []
    if m in CB_SHIFT:
        src_expr = read8_expr(o, lines)
        args = f"{src_expr}, flag_c()" if CB_SHIFT[m] in CARRY_IN else src_expr
        lines.append(f"const AluResult r = {CB_SHIFT[m]}({args});")
        lines.extend(write8_stmts(o, "r.value"))
        lines.append("regs_.f = r.flags;")
    elif m == "BIT":
        bit = int(info["operands"][0]["name"])
        src_expr = read8_expr(o, lines)
        lines.append(f"regs_.f = alu_bit({src_expr}, {bit}, regs_.f);")
    elif m in ("RES", "SET"):
        bit = int(info["operands"][0]["name"])
        src_expr = read8_expr(o, lines)
        mask = f"& static_cast<uint8_t>(~(1u << {bit}))" if m == "RES" else f"| (1u << {bit})"
        lines.append(f"const uint8_t v = static_cast<uint8_t>({src_expr} {mask});")
        lines.extend(write8_stmts(o, "v"))
    else:
        raise ValueError(f"unhandled cb {m}")
    lines.append(f"return {cycles};")
    return lines


def emit_switch(fn_name, table, gen_fn, default_lines):
    out = [f"uint32_t Cpu::{fn_name}(uint8_t opcode) {{", "    switch (opcode) {"]
    for op in range(256):
        info = table[f"0x{op:02X}"]
        body = gen_fn(op, info)
        needs_block = any(("const " in s or s.startswith("if (")) for s in body)
        label = f"    case 0x{op:02X}:"
        comment = f" // {op_text(info)}"
        if needs_block:
            out.append(label + " {" + comment)
            out.extend("        " + s for s in body)
            out.append("    }")
        else:
            out.append(label + comment)
            out.extend("        " + s for s in body)
    out.append("    }")
    out.extend("    " + s for s in default_lines)
    out.append("}")
    return out


def emit_meta(unprefixed, cb):
    rows = []
    for op in range(256):
        info = unprefixed[f"0x{op:02X}"]
        if info["mnemonic"] == "PREFIX":
            continue
        cycles = info["cycles"]
        rows.append({
            "opcode": op, "cb": False, "length": info["bytes"],
            "cycles": cycles[0], "branch": cycles[1] if len(cycles) == 2 else 0,
            "illegal": info["mnemonic"].startswith("ILLEGAL_"),
            "control": info["mnemonic"] in CONTROL,
        })
    for op in range(256):
        info = cb[f"0x{op:02X}"]
        rows.append({
            "opcode": op, "cb": True, "length": info["bytes"],
            "cycles": info["cycles"][0], "branch": 0, "illegal": False, "control": False,
        })
    out = ["// generated by tools/gen_opcodes.py -- do not edit",
           "struct OpcodeMeta {",
           "    uint8_t opcode;",
           "    bool cb;",
           "    uint8_t length;",
           "    uint8_t cycles;",
           "    uint8_t cycles_branch;",
           "    bool illegal;",
           "    bool control;",
           "};",
           f"inline constexpr std::array<OpcodeMeta, {len(rows)}> kOpcodeMeta = {{{{"]
    for r in rows:
        out.append(f"    {{0x{r['opcode']:02X}, {str(r['cb']).lower()}, {r['length']}, {r['cycles']}, "
                   f"{r['branch']}, {str(r['illegal']).lower()}, {str(r['control']).lower()}}},")
    out.append("}};")
    return out


def main():
    data = json.loads(JSON_PATH.read_text())
    unprefixed = data["unprefixed"]
    cb = data["cbprefixed"]
    out = ["// generated by tools/gen_opcodes.py from tools/Opcodes.json -- do not edit", ""]
    out += emit_switch("execute_opcode", unprefixed, gen_unprefixed, ["return trap_unknown(opcode);"])
    out.append("")
    out += emit_switch("execute_cb", cb, gen_cb, ["return trap_unknown(opcode);"])
    INC_PATH.write_text("\n".join(out) + "\n", newline="\n")
    META_PATH.write_text("\n".join(emit_meta(unprefixed, cb)) + "\n", newline="\n")
    print(f"wrote {INC_PATH.name} and {META_PATH.name}")


if __name__ == "__main__":
    main()
