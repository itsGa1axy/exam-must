#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""校验汉化是否安全：剥离全部注释后，代码必须与 tools/_backup 中的原始文件逐字节一致。"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract import tokenize, TARGET_DIRS, EXTS, SKIP_DIRS, ROOT, TOOLS


def strip(src, allow_at):
    """去掉所有注释 token，只留下代码。"""
    out = []
    pos = 0
    for start, end, _ in tokenize(src, allow_at):
        out.append(src[pos:start])
        pos = end
    out.append(src[pos:])
    return ''.join(out)


def main():
    backup = os.path.join(TOOLS, '_backup')
    bad = []
    same = []
    n = 0

    for d in TARGET_DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if x not in SKIP_DIRS]
            for fn in sorted(filenames):
                if os.path.splitext(fn)[1] not in EXTS:
                    continue
                cur_path = os.path.join(dirpath, fn)
                rel = os.path.relpath(cur_path, ROOT)
                old_path = os.path.join(backup, rel)
                if not os.path.exists(old_path):
                    continue
                n += 1
                allow_at = os.path.splitext(fn)[1] in ('.S', '.s')
                cur = open(cur_path, 'rb').read().decode('utf-8', errors='surrogateescape')
                old = open(old_path, 'rb').read().decode('utf-8', errors='surrogateescape')

                if strip(cur, allow_at) == strip(old, allow_at):
                    if cur != old:
                        same.append(rel)
                else:
                    bad.append(rel)

    print('检查文件数        : %d' % n)
    print('代码保持逐字节一致: %d' % (n - len(bad)))
    print('已汉化的文件      : %d' % len(same))
    if bad:
        print()
        print('!! 代码被改动的文件（必须修复）：')
        for r in bad:
            print('   ' + r)
        return 1
    print()
    print('OK — 所有文件的代码部分均未被改动。')
    return 0


if __name__ == '__main__':
    sys.exit(main())
