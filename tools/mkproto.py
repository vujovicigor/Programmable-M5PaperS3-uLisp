import re,sys
src,dst=sys.argv[1],sys.argv[2]
s=open(src).read()
lines=s.split('\n')
# top-level function definitions: start at col 0, end with ') {' on the same line
pat=re.compile(r'^((?:static\s+|inline\s+)*[A-Za-z_][\w\s\*&:<>,]*?[\s\*&])([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{')
protos=[]; seen=set()
for l in lines:
    m=pat.match(l)
    if not m: continue
    ret,name,args=m.groups()
    if ret.strip() in ('else','return','if') or name in ('setup','loop','if','while','for','switch'): continue
    p=f"{ret.strip()} {name} ({args});".replace('* ','*')
    if p not in seen: seen.add(p); protos.append(p)
# insert prototypes after the last top-level type definitions: right before first function definition
first=next(i for i,l in enumerate(lines) if pat.match(l))
out=lines[:first]+['// ---- auto-generated prototypes ----']+protos+['// ----']+lines[first:]
open(dst,'w').write('#include <Arduino.h>\n'+'\n'.join(out))
print(len(protos),'prototypes inserted before line',first)
