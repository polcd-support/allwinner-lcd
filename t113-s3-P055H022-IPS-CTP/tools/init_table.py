#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
init_table.py —— 面板初始化码 <-> 驱动 C 表 的转换 / 校验工具

用法:
  # 1) 厂家给的初始化码 txt -> 驱动用的 C 表（贴进 lcd_panel_init 的 for 循环即可）
  python3 init_table.py gen "5.0HSD +ST7703 MIPI 4L 20240312.txt"
  python3 init_table.py gen --aw drivers/.../lcd/mipi_5_720x1280.c      # AW 风格 {0x39,0x00,0x11,0xCE,...}
  python3 init_table.py gen --rk rk3568-kickpi-mipi0-XXX.dtsi          # Rockchip panel-init-sequence（带 11/29 尾巴）

  # 2) 校验驱动里的表与厂家码是否逐字节一致（含每条参数个数）
  python3 init_table.py verify "5.0HSD +ST7703 MIPI 4L 20240312.txt" \
        kernel/linux-5.4/drivers/video/fbdev/sunxi/disp2/disp/lcd/P050H031-V2-IPS.c
  python3 init_table.py verify --prefix "厂家码.txt" <driver.c>         # 驱动比厂家码多出的尾部（11/29/36）不算错

输入格式（自动识别，也可用 --aw/--rk/--flat 强制）:
  标准（默认）:
    {0xB9,0x03,{0xF1,0x12,0x83}},      -> cmd=0xB9, 3 个参数
    {REGFLAG_DELAY,250,{0x00}},        -> 延时 250ms（十进制或 0x 都认）
    // 注释、空行、行尾逗号都能处理

  扁平（--flat，很多 Polcd/HSD 的 txt 是这种）:
    Vsync：2 / VBP：16 / VFP：9 / H：45  -> 头部时序信息（会作为注释输出）
    0xB9,0xFF, 0x83, 0x94               -> cmd=0xB9, 参数 [0xFF,0x83,0x94]

  AW 风格（--aw）: {0x39,0x00,0x11,0xCE,0x01,...}
    第0列=DSI 包类型, 第1列=延时(ms), 第2列=payload 长度, 第3列起=payload
    -> cmd = payload[0], 参数 = payload[1:]

  Rockchip（--rk）: 39 FF 02 11 00
    第0列=DSI 包类型, 第1列=延时(ms，0xFF=255), 第2列=payload 长度, 第3列起=payload
    -> cmd = payload[0], 参数 = payload[1:]；延时输出为命令之后的 REGFLAG_DELAY
"""
import re
import sys


def read(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return f.read()


def parse_standard(text):
    """解析 {cmd,cnt,{params}} 形式 -> [(cmd, delay_ms, [params])]"""
    out = []
    for m in re.finditer(r"\{\s*([A-Za-z_0-9xX]+)\s*,\s*([0-9xXa-fA-F]+)\s*,\s*\{([^}]*)\}\s*\}", text):
        name, cnt, params = m.group(1), m.group(2), m.group(3)
        vals = [p.strip() for p in params.split(",") if p.strip()]
        if name.upper() == "REGFLAG_DELAY":
            ms = int(cnt, 0)
            out.append(("DELAY", ms, []))
        elif name.upper() in ("REGFLAG_END_OF_TABLE", "REGFLAG_DELAY_END"):
            break
        elif not re.match(r"^(0[xX][0-9a-fA-F]+|\d+)$", name):
            continue                      # 其它符号（宏）跳过
        else:
            cmd = int(name, 0)
            n = int(cnt, 0)
            out.append((cmd, 0, [int(v, 0) for v in vals]))
            if n != len(vals):
                out.append(("COUNT_ERR", n, len(vals)))
    return out


def parse_aw(text):
    """解析 AW 风格 {type,delay,len,payload...} -> [(cmd, delay_ms, [params])]"""
    out = []
    for line in text.splitlines():
        line = line.split("//")[0].strip()
        if not line.startswith("{") or not line.rstrip(",").endswith("}"):
            continue
        body = line.strip().rstrip(",").strip("{}")
        try:
            vals = [int(v.strip(), 0) for v in body.split(",") if v.strip()]
        except ValueError:
            continue
        if len(vals) < 3:
            continue
        typ, delay, ln = vals[0], vals[1], vals[2]
        payload = vals[3:]
        if not payload:
            continue
        if typ == 0x05:  # DCS short write, 0 param
            out.append((payload[0], delay, []))
        else:
            out.append((payload[0], delay, payload[1:]))
    return out


def parse_flat(text):
    """解析扁平格式 -> ([(cmd, delay_ms, [params])], {头部键: 值})
       0xB9,0xFF, 0x83, 0x94               -> cmd=0xB9, 参数 [0xFF,0x83,0x94]
       Vsync：2 / VBP：16 / VFP：9 / H：45  -> 头部时序（全角/半角冒号都认）
    """
    out, header = [], {}
    for raw in text.splitlines():
        line = raw.split("//")[0].strip()
        if not line:
            continue
        m = re.match(r"^([A-Za-z][A-Za-z0-9_ ]*)\s*[:：]\s*([0-9]+)\s*$", line)
        if m:
            header[m.group(1).strip()] = int(m.group(2))
            continue
        if not re.match(r"^(0[xX][0-9a-fA-F]+|\d+)\s*,", line):
            continue
        vals = []
        for tok in line.rstrip(",").split(","):
            tok = tok.strip()
            if not tok:
                continue
            try:
                vals.append(int(tok, 0))
            except ValueError:
                vals = []
                break
        if vals:
            out.append((vals[0], 0, vals[1:]))
    return out, header


def parse_rk(text):
    """解析 Rockchip panel-init-sequence -> ([(cmd, delay_ms, [params])], {})
       <type> <delay_ms> <len> <payload...>   例: 39 FF 02 11 00
       延时按 Allwinner 习惯输出在命令之后（0xFF = 255ms）
    """
    body = text.split("panel-init-sequence")[1].split("];")[0] if "panel-init-sequence" in text else text
    out = []
    for raw in body.splitlines():
        line = raw.split("//")[0].strip().rstrip(";")
        if not re.match(r"^[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}(\s|$)", line):
            continue
        try:
            vals = [int(t, 16) for t in line.split()]
        except ValueError:
            continue
        typ, delay, payload = vals[0], vals[1], vals[3:]
        if not payload:
            continue
        out.append((payload[0], 0, [] if typ == 0x05 else payload[1:]))
        if delay:
            out.append(("DELAY", delay, []))
    return out, {}


def parse_any(text, mode=None):
    """按模式/自动识别解析 -> ([(cmd, delay_ms, [params])], header)"""
    if mode == "aw":
        return parse_aw(text), {}
    if mode == "flat":
        return parse_flat(text)
    if mode == "rk":
        return parse_rk(text)
    if "panel-init-sequence" in text:
        return parse_rk(text)
    if re.search(r"\{\s*[A-Za-z_0-9xX]+\s*,\s*[0-9xXa-fA-F]+\s*,\s*\{", text):
        return parse_standard(text), {}
    return parse_flat(text)


def parse_driver_table(text):
    """从驱动 .c 里抽 lcm_initialization_setting[] / data[][] 表"""
    if "lcm_initialization_setting[]" in text:
        text = text.split("lcm_initialization_setting[]")[1]
    return parse_standard(text)


def entries_only(seq):
    return [e for e in seq if e[0] not in ("DELAY", "COUNT_ERR")]


def cmd_gen(src, mode):
    seq, header = parse_any(read(src), mode)
    bad = [e for e in seq if e[0] == "COUNT_ERR"]
    lines = ["static struct LCM_setting_table lcm_initialization_setting[] = {"]
    if header:
        lines.append("\t/* 厂家给的面板时序: %s */" % "  ".join("%s=%d" % (k, v) for k, v in header.items()))
    for cmd, delay, params in seq:
        if cmd == "DELAY":
            lines.append("\t{REGFLAG_DELAY, %d, {} }," % delay)
        elif cmd == "COUNT_ERR":
            continue
        else:
            if mode == "aw" and delay:
                lines.append("\t{REGFLAG_DELAY, %d, {} }," % delay)
            pl = ", ".join("0x%02X" % p for p in params)
            lines.append("\t{0x%02X, %d, {%s} }," % (cmd, len(params), pl))
    lines.append("\t{REGFLAG_END_OF_TABLE, 0, {} },")
    lines.append("};")
    print("\n".join(lines))
    if bad:
        sys.stderr.write("警告: 有 %d 条声明的参数个数与实际不符（已按实际输出）\n" % len(bad))
    n_delay = len([e for e in seq if e[0] == "DELAY"]) + \
        (len([e for e in seq if e[0] not in ("DELAY", "COUNT_ERR") and e[1]]) if mode == "aw" else 0)
    sys.stderr.write("共 %d 条命令 + %d 条延时\n" % (len(entries_only(seq)), n_delay))


def cmd_verify(src, driver, mode, prefix):
    ref = entries_only(parse_any(read(src), mode)[0])
    dev = entries_only(parse_driver_table(read(driver)))
    ok = True
    if len(dev) < len(ref) or (not prefix and len(ref) != len(dev)):
        print("条数不同: 厂家码 %d 条, 驱动 %d 条" % (len(ref), len(dev)))
        ok = False
    for i, (a, b) in enumerate(zip(ref, dev)):
        if a != b:
            print("第 %d 条不一致:\n  厂家: cmd=0x%02X params=%s\n  驱动: cmd=0x%02X params=%s"
                  % (i + 1, a[0], ["0x%02X" % p for p in a[2]], b[0], ["0x%02X" % p for p in b[2]]))
            ok = False
    if prefix and len(dev) > len(ref):
        print("（驱动尾部另有 %d 条：%s，按 --prefix 不判为差异）"
              % (len(dev) - len(ref), ", ".join("0x%02X" % e[0] for e in dev[len(ref):])))
    print("逐条一致 ✔" if ok else "存在差异 ✘")
    return 0 if ok else 1


def main():
    argv = sys.argv[1:]
    args = [a for a in argv if not a.startswith("--")]
    mode = next((m for f, m in (("--aw", "aw"), ("--flat", "flat"), ("--rk", "rk")) if f in argv), None)
    prefix = "--prefix" in argv
    if len(args) >= 2 and args[0] == "gen":
        cmd_gen(args[1], mode)
    elif len(args) >= 3 and args[0] == "verify":
        sys.exit(cmd_verify(args[1], args[2], mode, prefix))
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
