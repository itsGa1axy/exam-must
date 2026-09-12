#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""汉化进度追踪：按文件统计已汉化 / 未汉化的注释条目。"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')


def load_translated_uids():
    zdir = os.path.join(TOOLS, 'zh')
    uids = set()
    if not os.path.isdir(zdir):
        return uids
    for fn in sorted(os.listdir(zdir)):
        if not fn.endswith('.txt'):
            continue
        with open(os.path.join(zdir, fn), 'rb') as f:
            text = f.read().decode('utf-8', errors='surrogateescape')
        for line in text.replace('\r\n', '\n').split('\n'):
            s = line.strip()
            if s.startswith('<<<COMMENT ') and s.endswith('>>>'):
                try:
                    uids.add(int(s[len('<<<COMMENT '):-3]))
                except ValueError:
                    pass
    return uids


def main():
    with open(os.path.join(TOOLS, '_map.json'), 'rb') as f:
        mapping = json.loads(f.read().decode('utf-8'))
    done = load_translated_uids()

    total_u = set()
    rows = []
    for rel, entries in mapping.items():
        uids = {e['uid'] for e in entries}
        total_u |= uids
        hit = len(uids & done)
        rows.append((rel, hit, len(uids), len(entries)))

    full = sum(1 for r in rows if r[1] == r[2])
    print('文件完成度：%d / %d' % (full, len(rows)))
    print('去重条目：  %d / %d  (%.1f%%)'
          % (len(total_u & done), len(total_u), 100.0 * len(total_u & done) / max(len(total_u), 1)))
    print()
    mark = {True: '[✓]', False: '[ ]'}
    for rel, hit, nu, nc in sorted(rows, key=lambda r: (r[1] == r[2], -r[2])):
        print('  %s %-42s %4d/%-4d 处=%d' % (mark[hit == nu], rel, hit, nu, nc))
    rest = len(rows) - full
    if rest:
        print('\n剩余未完成文件：%d 个' % rest)
    else:
        print('\n全部文件已完成。')


if __name__ == '__main__':
    main()
