# Converte a página para ASCII puro (evita problemas de acentuação dentro do plugin).
import re,sys
s=open(sys.argv[1],encoding='utf-8').read()
def js(t):return ''.join(c if ord(c)<128 else ('\\u%04x'%ord(c) if ord(c)<0x10000 else ''.join('\\u%04x'%u for u in (0xD800+((ord(c)-0x10000)>>10),0xDC00+((ord(c)-0x10000)&0x3FF)))) for c in t)
def html(t):return ''.join(c if ord(c)<128 else '&#x%x;'%ord(c) for c in t)
out=[];pos=0
for m in re.finditer(r'(<script>)([\s\S]*?)(</script>)|(<style>)([\s\S]*?)(</style>)',s):
    out.append(html(s[pos:m.start()]))
    if m.group(1):out.append(m.group(1)+js(m.group(2))+m.group(3))
    else:out.append(m.group(4)+js(m.group(5))+m.group(6))
    pos=m.end()
out.append(html(s[pos:]));r=''.join(out)
open(sys.argv[2],'w',encoding='ascii').write(r);print('ok',len(r))
