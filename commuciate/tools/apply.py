#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把 tools/_zh.txt 里的中文注释按 _map.json 记录的位置写回源文件。

只替换注释 token，其余字节原样保留（含换行风格）。
用法：
  python tools/apply.py            # 写入
  python tools/apply.py --check    # 只校验，不写入
  python tools/apply.py Lib/stm32f10x_gpio.c   # 只处理指定文件
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')


def load_zh():
    """解析 tools/zh/*.txt 全部译文分块 -> {uid: 中文注释文本}"""
    zdir = os.path.join(TOOLS, 'zh')
    if not os.path.isdir(zdir):
        return {}
    out = {}
    for fn in sorted(os.listdir(zdir)):
        if not fn.endswith('.txt'):
            continue
        with open(os.path.join(zdir, fn), 'rb') as f:
            text = f.read().decode('utf-8', errors='surrogateescape')
        cur = None
        buf = []
        for line in text.replace('\r\n', '\n').split('\n'):
            s = line.strip()
            if s.startswith('<<<COMMENT ') and s.endswith('>>>'):
                if cur is not None:
                    out[cur] = '\n'.join(buf).strip('\n')
                cur = int(s[len('<<<COMMENT '):-3])
                buf = []
            elif cur is not None:
                buf.append(line)
        if cur is not None:
            out[cur] = '\n'.join(buf).strip('\n')
    return {k: v for k, v in out.items() if v.strip()}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    check = '--check' in sys.argv
    only = set(a.replace('\\', '/') for a in args)

    with open(os.path.join(TOOLS, '_map.json'), 'rb') as f:
        mapping = json.loads(f.read().decode('utf-8'))
    zh = load_zh()

    done = 0
    touched = 0
    missing = set()

    for rel, entries in mapping.items():
        if only and rel not in only:
            continue
        path = os.path.join(ROOT, rel.replace('/', os.sep))
        # 始终以 tools/_backup 中的原始文件为基准，保证本脚本可重复执行
        base = os.path.join(TOOLS, '_backup', rel.replace('/', os.sep))
        if not os.path.exists(base):
            base = path
        with open(base, 'rb') as f:
            raw = f.read()
        src = raw.decode('utf-8', errors='surrogateescape')

        eol = '\r\n' if '\r\n' in src else '\n'

        # 从后往前替换，前面的下标才不会失效
        reps = []
        for e in entries:
            if e['uid'] in zh:
                reps.append((e['start'], e['end'], zh[e['uid']]))
            else:
                missing.add(e['uid'])
        reps.sort(key=lambda x: -x[0])

        for start, end, text in reps:
            body = text.replace('\r\n', '\n').replace('\n', eol)
            src = src[:start] + body + src[end:]

        n_hit = len({e['uid'] for e in entries if e['uid'] in zh})
        done += n_hit
        if reps:
            touched += 1
            if not check:
                with open(path, 'wb') as f:
                    f.write(src.encode('utf-8', errors='surrogateescape'))

    verb = '待写入' if check else '已写入'
    print('%s：%d 个文件，%d 处注释' % (verb, touched, done))
    if missing:
        print('尚未汉化的去重条目：%d 条' % len(missing))


if __name__ == '__main__':
    main()
