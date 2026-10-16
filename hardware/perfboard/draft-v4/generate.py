"""Generate the provisional v4 hole plan from the preserved v1 pin map."""
from pathlib import Path
import json,uuid,zipfile,csv
from datetime import datetime,timezone
D=Path(__file__).resolve().parent
p=json.loads((D.parent/'draft-v1/twoskulls-v1.json').read_text())
p.update(id=str(uuid.uuid4()),name='TwoSkulls v4 - north servo - RF clearance',createdAt=datetime.now(timezone.utc).isoformat(),updatedAt=datetime.now(timezone.utc).isoformat(),solderedLinks=[])
parts={v['ref']:v for v in p['parts']}
for ref,col,row in [('J3',26,6),('J4',12,23),('D1',6,24),('C1',12,14),('C2',12,16),('C3',3,20)]:parts[ref]['at']={'col':col,'row':row}
parts['J4']['value']='UNDERSIDE WIRE PADS - flush socket south'
p['kinds'][parts['J4']['kind']]['pins']=[{'name':'5V','col':0,'row':0},{'name':'GND','col':2,'row':0}]
parts['C1']['value']='470uF >=10V - UNDERSIDE laid flat north'
parts['C2']['value']='100nF - UNDERSIDE'
parts['C3']['value']='100uF >=10V - UNDERSIDE laid flat north'
for ref in ['C1','C3']:p['kinds'][parts[ref]['kind']]['height']=10 if ref=='C1' else 6.3
parts['J1']['value']='REMOVABLE WROVER - raised sockets'
p['board']['labels']={'letters':'cols','colsFrom':'left','rowsFrom':'top'}
# Diode band faces DOWN in this revision.
k=p['kinds'][parts['D1']['kind']]
k['pins']=[{'name':'A','col':0,'row':0},{'name':'K','col':0,'row':4}]
parts['D1']['value']='1N5819 - band DOWN toward MCU supply'
# Rails are segmented at every named same-net takeoff/pin.
paths={
'EXT_5V':[
 ['@12,8','C1.PLUS','C2.P','J4.5V'],
 ['@12,8','@17,8','@19,9','@23,9','@23,7','J3.5V'],
 ['J4.5V','@6,23','D1.A']],
'GND':[
 ['@14,9','@14,10','C1.MINUS','C2.N','@14,20','@14,21','J4.GND'],
 ['@14,10','@17,10','@19,11','@24,11','@24,8','J3.GND'],
 ['@14,20','J1.R20_GND'],
 ['@14,21','@5,21','@2,21','@2,9','J2.GND'],
 ['@5,21','C3.MINUS']],
'MCU_5V':[
 ['D1.K','@8,28','J1.L20_5V'],
 ['D1.K','@3,28','C3.PLUS']],
'EYE_LEFT':[['J1.L07_GPIO32','J2.LEFT']],
'EYE_RIGHT':[['J1.L08_GPIO33','J2.RIGHT']],
'SERVO_PWM':[['J1.R02_GPIO23','@20,2','@20,6','J3.SIG']]
}
colors={n['name']:n['colour'] for n in p['nets']}
p['nets']=[]
for name,chains in paths.items():
 wires=[edge for path in chains for edge in map(list,zip(path,path[1:]))]
 pins=list(dict.fromkeys(s for edge in wires for s in edge))
 p['nets'].append(dict(name=name,pins=pins,colour=colors[name],wires=wires))
(D/'twoskulls-v4.json').write_text(json.dumps(p,indent=2)+'\n')
(D/'intended-netpaths.json').write_text(json.dumps(paths,indent=2)+'\n')
with zipfile.ZipFile(D/'twoskulls-v4-import.zip','w',zipfile.ZIP_DEFLATED) as z:
 z.writestr('project.json',json.dumps(p))
 z.writestr('README.txt','TwoSkulls v4 design draft. See companion README and assembly drawing. Fit not yet verified.\n')
coords={}
for part in p['parts']:
 for pin in p['kinds'][part['kind']]['pins']:
  coords[part['ref']+'.'+pin['name']]=(part['at']['col']+pin['col'],part['at']['row']+pin['row'])
def xy(s):return tuple(map(int,s[1:].split(','))) if s.startswith('@') else coords[s]
def on(pt,a,b):
 x,y=pt;u,v=a;U,V=b
 return (x-u)*(V-v)==(y-v)*(U-u) and min(u,U)<=x<=max(u,U) and min(v,V)<=y<=max(v,V)
findings=[];crossings=[]
for n in p['nets']:
 seen={n['pins'][0]}
 while True:
  nxt=seen|{v for a,b in n['wires'] for u,v in [(a,b),(b,a)] if u in seen}
  if nxt==seen:break
  seen=nxt
 assert seen==set(n['pins'])
 for a,b in n['wires']:
  A=xy(a);B=xy(b)
  assert A[0]==B[0] or A[1]==B[1] or frozenset((a,b)) in {frozenset(('@17,8','@19,9')),frozenset(('@17,10','@19,11'))}
  for pin,pt in coords.items():
   if pin not in n['pins'] and on(pt,A,B):findings.append((n['name'],a,b,'foreign pin',pin))
  for other in p['nets']:
   if other==n:continue
   for j in other['pins']:
    if j.startswith('@') and on(xy(j),A,B):findings.append((n['name'],a,b,'foreign junction',j))
assert not findings,findings
with (D/'wiring.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['net','from','from_col_row','to','to_col_row'])
 for n in p['nets']:
  for a,b in n['wires']:w.writerow([n['name'],a,xy(a),b,xy(b)])
report=dict(parts=len(p['parts']),nets=len(p['nets']),valid_part_pins=len(coords),wire_segments=sum(len(n['wires']) for n in p['nets']),all_nets_connected=True,foreign_pin_or_junction_intersections=findings,physical_fit_verified=False,electrical_load_tested=False,insulated_flying_links=[['@17,8','@19,9'],['@17,10','@19,11']])
(D/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
