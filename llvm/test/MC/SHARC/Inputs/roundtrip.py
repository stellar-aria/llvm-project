"""Disassemble each byte line of a disassembler test, reassemble the text,
and check the encoding is the same bytes.

usage: roundtrip.py LLVM-MC TESTFILE [llvm-mc options...]
"""
import re
import subprocess
import sys

mc, path, opts = sys.argv[1], sys.argv[2], sys.argv[3:]
lines = [l.strip() for l in open(path) if l.strip() and not l.lstrip().startswith("#")]
bad = 0
equiv = 0
for line in lines:
    want = [int(x, 16) for x in line.split()]
    dis = subprocess.run([mc, "--disassemble", "-triple=sharc"] + opts, input=line,
                         capture_output=True, text=True)
    text = [l.strip() for l in dis.stdout.splitlines()
            if l.strip() and not l.strip().startswith(".text")]
    if len(text) != 1:
        print(f"FAIL disassembly of {line}: {text} {dis.stderr.strip()}")
        bad += 1
        continue
    asm = subprocess.run([mc, "-show-encoding", "-triple=sharc"] + opts, input=text[0],
                         capture_output=True, text=True)
    m = re.search(r"encoding: \[([^\]]*)\]", asm.stdout)
    got = [int(x, 16) for x in m.group(1).split(",")] if m else None
    if got == want:
        continue
    # The same text may have several encodings (e.g. Type 4b and Type 15b for
    # "R2 = DM(-5, I6)"). Accept another encoding only if it prints the same.
    again = None
    if got:
        redis = subprocess.run([mc, "--disassemble", "-triple=sharc"] + opts,
                               input=" ".join(f"0x{b:02x}" for b in got),
                               capture_output=True, text=True)
        again = [l.strip() for l in redis.stdout.splitlines()
                 if l.strip() and not l.strip().startswith(".text")]
    if again == text:
        equiv += 1
        print(f"EQUIVALENT {text[0]} ({line} -> {' '.join(f'0x{b:02x}' for b in got)})")
        continue
    print(f"FAIL {text[0]}\n  bytes  {line}\n  became {got} {asm.stderr.strip()}")
    bad += 1
print(f"roundtrip: {len(lines) - bad} of {len(lines)} ok ({equiv} by an equivalent encoding)")
