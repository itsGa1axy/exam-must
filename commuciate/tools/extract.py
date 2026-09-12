#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
抽取 STM32 标准外设库源码中的全部注释，供汉化使用。

只读源文件，绝不修改源码。产出两个中间文件：
  tools/_unique.txt  去重后的注释原文，每条带编号（= uid）
  tools/_map.json    {文件: [{start, end, uid}, ...]}，start/end 为字符下标

汉化流程：
  extract.py  ->  _unique.txt  ->  人工汉化  ->  _zh.txt  ->  apply.py
"""
import json
import os
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')

TARGET_DIRS = ['Lib', 'Start', 'Inc', 'Src']
EXTS = {'.c', '.h', '.S', '.s'}
SKIP_DIRS = {'build', '.git', 'tools', '.vscode', '.settings', 'cmake'}

MARK = '<<<COMMENT %d>>>'


def tokenize(src, allow_at_comment):
    """返回 [(start, end, text)]。start/end 是字符下标，覆盖整个注释符号。

    必须跳过字符串/字符字面量，否则会把 "http://x" 里的 // 当成注释。
    """
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n:
            d = src[i + 1]
            if d == '*':                      # 块注释
                j = src.find('*/', i + 2)
                end = n if j == -1 else j + 2
                out.append((i, end, src[i:end]))
                i = end
                continue
            if d == '/':                      # 行注释
                j = src.find('\n', i)
                end = n if j == -1 else j
                out.append((i, end, src[i:end]))
                i = end
                continue
        if allow_at_comment and c == '@':     # ARM 汇编行注释
            j = src.find('\n', i)
            end = n if j == -1 else j
            out.append((i, end, src[i:end]))
            i = end
            continue
        if c == '"' or c == "'":              # 跳过字面量
            q = c
            i += 1
            while i < n:
                if src[i] == '\\':
                    i += 2
                    continue
                if src[i] == q:
                    i += 1
                    break
                if src[i] == '\n':
                    break
                i += 1
            continue
        i += 1
    return out


def norm(t):
    """归一化：统一换行、去掉行尾空白与首尾空行。仅用于去重比对。"""
    t = t.replace('\r\n', '\n').replace('\r', '\n')
    lines = [ln.rstrip() for ln in t.split('\n')]
    return '\n'.join(lines).strip('\n')


def collect():
    files = []
    for d in TARGET_DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if x not in SKIP_DIRS]
            for fn in sorted(filenames):
                if os.path.splitext(fn)[1] in EXTS:
                    files.append(os.path.join(dirpath, fn))
    return sorted(files)


def main():
    unique = OrderedDict()     # 归一化文本 -> uid
    raws = []                  # uid -> 首次出现的原文
    mapping = OrderedDict()    # 相对路径 -> [{start,end,uid}]
    stats = []
    total_cmts = 0

    for path in collect():
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        with open(path, 'rb') as f:
            raw = f.read()
        src = raw.decode('utf-8', errors='surrogateescape')

        allow_at = os.path.splitext(path)[1] in ('.S', '.s')
        toks = tokenize(src, allow_at)
        if not toks:
            continue

        entries = []
        for start, end, text in toks:
            key = norm(text)
            if key not in unique:
                unique[key] = len(unique)
                raws.append(text)
            entries.append({'start': start, 'end': end, 'uid': unique[key]})

        mapping[rel] = entries
        total_cmts += len(entries)
        stats.append((rel, len(entries)))

    # 写出 _unique.txt / _zh.txt（_zh.txt 供人工填写，已存在则不覆盖）
    # 用 surrogateescape 保证非 UTF-8 字节原样往返，不会被转码
    with open(os.path.join(TOOLS, '_unique.txt'), 'w', encoding='utf-8',
              newline='\n', errors='surrogateescape') as f:
        for uid, text in enumerate(raws):
            f.write((MARK % uid) + '\n')
            f.write(text.replace('\r\n', '\n') + '\n')

    os.makedirs(os.path.join(TOOLS, 'zh'), exist_ok=True)

    with open(os.path.join(TOOLS, '_map.json'), 'w', encoding='utf-8') as f:
        json.dump(mapping, f, ensure_ascii=False, indent=1)

    print('文件数        : %d' % len(mapping))
    print('注释总数      : %d' % total_cmts)
    print('去重后条目    : %d' % len(unique))
    print('去重节省      : %d 条 (%.1f%%)'
          % (total_cmts - len(unique), 100.0 * (total_cmts - len(unique)) / max(total_cmts, 1)))
    print()
    print('各文件注释数：')
    for rel, c in sorted(stats, key=lambda x: -x[1]):
        print('  %-40s %5d' % (rel, c))


if __name__ == '__main__':
    main()
