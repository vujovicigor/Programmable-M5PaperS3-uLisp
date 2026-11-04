import re,sys
src,dst=sys.argv[1],sys.argv[2]
s=open(src).read()

# Embed patched/library.lisp (next to the .ino) as uLisp's built-in LispLibrary
import os
libpath=os.path.join(os.path.dirname(src),'library.lisp')
if os.path.exists(libpath):
    lisp=open(libpath).read()
    out=[];instr=False;i=0
    while i<len(lisp):                       # drop ; comments outside strings
        c=lisp[i]
        if instr:
            out.append(c)
            if c=='\\' and i+1<len(lisp): out.append(lisp[i+1]); i+=1
            elif c=='"': instr=False
        elif c=='"': instr=True; out.append(c)
        elif c==';':
            while i<len(lisp) and lisp[i]!='\n': i+=1
            continue
        else: out.append(c)
        i+=1
    body='\n'.join(l.rstrip() for l in ''.join(out).split('\n') if l.strip())
    cstr=body.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n"\n"')
    marker='const char LispLibrary[] = "";'
    assert marker in s, 'LispLibrary marker not found'
    s=s.replace(marker,'const char LispLibrary[] =\n"'+cstr+'";')
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
