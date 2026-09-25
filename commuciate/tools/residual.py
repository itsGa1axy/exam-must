#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
残留英文检查：直接扫描源码文件里现存的注释，找出仍含英文散文的条目。

这是判定「所有注释是否已汉化」的最终标准——它不看中间产物，
只看源文件当前的真实内容。

用法：
  python tools/residual.py            # 汇总
  python tools/residual.py --list      # 列出每条残留
  python tools/residual.py --file Lib/stm32f10x_gpio.c
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract import tokenize, TARGET_DIRS, EXTS, SKIP_DIRS, ROOT

# 判定「含英文散文」：连续出现 2 个及以上纯英文单词
WORD = re.compile(r'[A-Za-z]{2,}')
STOP = {
    'the', 'and', 'for', 'with', 'this', 'that', 'are', 'not', 'you', 'can',
    'when', 'from', 'has', 'have', 'its', 'was', 'will', 'all', 'any', 'use',
    'into', 'out', 'set', 'get', 'bit', 'reg', 'per', 'via', 'see', 'not',
}


def english_runs(text):
    """返回 (该注释里最长的连续英文单词串长度, 串内容)。用来区分
    纯标识符（如 `__MISC_H`、`CAN_IT_TME`）和真正的英文句子。"""
    # 先去掉 doxygen 标签、标识符形态的词（含下划线或全大写的）
    cleaned = re.sub(r'@\w+', ' ', text)
    best = ''
    for m in re.finditer(r'(?:[A-Za-z][a-z]{1,}\s+){2,}[A-Za-z][a-z]{1,}', cleaned):
        s = m.group(0).strip()
        if len(s) > len(best):
            best = s
    return best


def main():
    listing = '--list' in sys.argv
    only = None
    if '--file' in sys.argv:
        only = sys.argv[sys.argv.index('--file') + 1].replace('\\', '/')

    total = 0
    left = 0
    rows = []

    for d in TARGET_DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if x not in SKIP_DIRS]
            for fn in sorted(filenames):
                if os.path.splitext(fn)[1] not in EXTS:
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, ROOT).replace('\\', '/')
                if only and rel != only:
                    continue
                src = open(path, 'rb').read().decode('utf-8', errors='surrogateescape')
                allow_at = os.path.splitext(fn)[1] in ('.S', '.s')

                n = 0
                miss = []
                for _, _, text in tokenize(src, allow_at):
                    n += 1
                    run = english_runs(text.replace('\n', ' '))
                    if run:
                        miss.append((text.strip()[:90], run))
                total += n
                left += len(miss)
                rows.append((rel, n, len(miss), miss))

    print('注释总数      : %d' % total)
    print('仍含英文散文  : %d' % left)
    print('已汉化比例    : %.1f%%' % (100.0 * (total - left) / max(total, 1)))
    print()

    if listing:
        for rel, n, m, miss in rows:
            if not m:
                continue
            print('=== %s  (%d/%d 待处理)' % (rel, m, n))
            for t, run in miss[:60]:
                print('   %s' % t.replace('\n', ' '))
            if m > 60:
                print('   ... 还有 %d 条' % (m - 60))
    else:
        for rel, n, m, _ in sorted(rows, key=lambda r: -r[2]):
            if m:
                print('  %-42s %4d / %-4d 条待处理' % (rel, m, n))

    return 0 if left == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
