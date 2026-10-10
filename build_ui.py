#!/usr/bin/env python3
"""Assembles index.html from css/base.css + js/core.js + js/shell.js + js/ux.js + pages/*.js. Run: python3 build_ui.py"""
import glob, os, re
d = os.path.dirname(os.path.abspath(__file__))

def lean(src):
    """Embedded copy only: drop /* */ comments, whole-line // comments, indentation and blank lines. The source files stay untouched."""
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    out = []
    for line in src.split('\n'):
        t = line.strip()
        if t and not t.startswith('//'):
            out.append(t)
    return '\n'.join(out)

rd = lambda p: lean(open(os.path.join(d, p), encoding='utf-8').read())
pages = sorted(glob.glob(os.path.join(d, 'pages', '*.js')))
scripts = [rd('js/core.js'), rd('js/shell.js'), rd('js/ux.js')] + [lean(open(p, encoding='utf-8').read()) for p in pages]
html = '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n<title>WinKit</title>\n<style>\n' + rd('css/base.css') + '\n</style>\n</head>\n<body>\n<div id="wk-root">\n<aside id="wk-side"><div class="brand"><span class="wk-logo"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M2 4.5 L4.8 12 L8 6.5 L11.2 12 L14 4.5"/></svg></span><span>WinKit</span></div><nav id="wk-nav"></nav><div class="wk-side-foot" id="wk-side-foot"></div></aside>\n<main id="wk-main"></main>\n</div>\n'
for s in scripts:
    html += '<script>\n' + s.replace('</script', '<\\/script') + '\n</script>\n'
html += '<script>WK.boot();</script>\n</body>\n</html>\n'
open(os.path.join(d, 'index.html'), 'w', encoding='utf-8').write(html)
print('index.html', len(html.encode('utf-8')) // 1024, 'KB from', len(pages), 'page files')
