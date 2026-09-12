#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
校验译文分块：每条译文必须恰好是一个完整的注释 token。

若一条译文里出现 `*/` 后紧跟 `/*`，它会被切成两个注释，
回写后会在源码里留下游离空白甚至吞掉代码。本脚本专门抓这种情况。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract import tokenize, TOOLS


def load_blocks(path):
    text = open(path, 'rb').read().decode('utf-8', errors='surrogateescape')
    out = []
    cur = None
    buf = []
    for line in text.replace('\r\n', '\n').split('\n'):
        s = line.strip()
        if s.startswith('<<<COMMENT ') and s.endswith('>>>'):
            if cur is not None:
                out.append((cur, '\n'.join(buf).strip('\n')))
            cur = int(s[len('<<<COMMENT '):-3])
            buf = []
        elif cur is not None:
            buf.append(line)
    if cur is not None:
        out.append((cur, '\n'.join(buf).strip('\n')))
    return [(u, t) for u, t in out if t.strip()]


def is_single_comment(text):
    for allow_at in (False, True):
        toks = tokenize(text, allow_at)
        if len(toks) == 1 and toks[0][0] == 0 and toks[0][1] == len(text):
            return True, None
        if len(toks) == 1 and toks[0][2] == text:
            return True, None
    toks = tokenize(text, False)
    return False, toks


def main():
    zdir = os.path.join(TOOLS, 'zh')
    bad = []
    total = 0
    for fn in sorted(os.listdir(zdir)):
        if not fn.endswith('.txt'):
            continue
        for uid, text in load_blocks(os.path.join(zdir, fn)):
            total += 1
            ok, toks = is_single_comment(text)
            if not ok:
                bad.append((fn, uid, text, toks))

    print('译文条目总数: %d' % total)
    print('结构异常条目: %d' % len(bad))
    print()
    for fn, uid, text, toks in bad:
        print('--- %s  uid=%d  (被切成 %d 个 token)' % (fn, uid, len(toks)))
        print('    译文: %r' % text[:160])
        for s, e, t in toks:
            print('      token[%d:%d] = %r' % (s, e, t[:70]))
        print()
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
