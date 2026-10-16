"""Generate the provisional v2 hole plan from the preserved v1 pin map."""
from pathlib import Path
import json,uuid,zipfile,csv
from datetime import datetime,timezone
D=Path(__file__).resolve().parent
p=json.loads((D.parent/'draft-v1/twoskulls-v1.json').read_text())
p.update(id=str(uuid.uuid4()),name='TwoSkulls v2 - straight rails - review draft',createdAt=datetime.now(timezone.utc).isoformat(),updatedAt=datetime.now(timezone.utc).isoformat(),solderedLinks=[])
parts={v['ref']:v for v in p['parts']}
for ref,col,row in [('J3',26,23),('J4',20,28),('D1',6,22),('C1',9,28),('C2',9,25),('C3',2,23)]:parts[ref]['at']={'col':col,'row':row}
parts['J4']['value']='WIRE LANDINGS ONLY - socket on raised bracket'
parts['J1']['value']='REMOVABLE WROVER - raised sockets'
# Diode band faces DOWN in this revision.
k=p['kinds'][parts['D1']['kind']]
k['pins']=[{'name':'A','col':0,'row':0},{'name':'K','col':0,'row':4}]
parts['D1']['value']='1N5819 - band DOWN toward MCU supply'
# Rails are segmented at every named same-net takeoff/pin.
paths={
'EXT_5V':[
 ['@9,6','@9,22','@9,24','C2.P','C1.PLUS'],
 ['@9,22','D1.A'],
 ['@9,24','@18,24','J3.5V'],
 ['@18,24','@18,28','J4.5V']],
'GND':[
 ['@11,6','@11,20','@11,21','C2.N','C1.MINUS','@11,30'],
 ['@11,20','J1.R20_GND'],
 ['@11,21','@4,21','@3,21','@3,9','J2.GND'],
 ['C2.N','J3.GND'],
 ['@4,21','C3.MINUS'],
 ['@11,30','@20,30','J4.GND']],
'MCU_5V':[
 ['D1.K','@8,26','J1.L20_5V'],
 ['D1.K','@2,26','C3.PLUS']],
'EYE_LEFT':[['J1.L07_GPIO32','J2.LEFT']],
'EYE_RIGHT':[['J1.L08_GPIO33','J2.RIGHT']],
'SERVO_PWM':[['J1.R02_GPIO23','@20,2','@20,23','J3.SIG']]
}
colors={n['name']:n['colour'] for n in p['nets']}
p['nets']=[]
for name,chains in paths.items():
 wires=[edge for path in chains for edge in map(list,zip(path,path[1:]))]
 pins=list(dict.fromkeys(s for edge in wires for s in edge))
 p['nets'].append(dict(name=name,pins=pins,colour=colors[name],wires=wires))
(D/'twoskulls-v2.json').write_text(json.dumps(p,indent=2)+'\n')
(D/'intended-netpaths.json').write_text(json.dumps(paths,indent=2)+'\n')
with zipfile.ZipFile(D/'twoskulls-v2-import.zip','w',zipfile.ZIP_DEFLATED) as z:
 z.writestr('project.json',json.dumps(p))
 z.writestr('README.txt','TwoSkulls v2 design draft. See companion README and assembly drawing. Fit not yet verified.\n')
coords={}
for part in p['parts']:
 for pin in p['kinds'][part['kind']]['pins']:
  coords[part['ref']+'.'+pin['name']]=(part['at']['col']+pin['col'],part['at']['row']+pin['row'])
def xy(s):return tuple(map(int,s[1:].split(','))) if s.startswith('@') else coords[s]
def on(pt,a,b):
 x,y=pt;u,v=a;U,V=b
 return (u==U==x and min(v,V)<=y<=max(v,V)) or (v==V==y and min(u,U)<=x<=max(u,U))
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
  assert A[0]==B[0] or A[1]==B[1]
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
report=dict(parts=len(p['parts']),nets=len(p['nets']),valid_part_pins=len(coords),wire_segments=sum(len(n['wires']) for n in p['nets']),all_nets_connected=True,foreign_pin_or_junction_intersections=findings,physical_fit_verified=False,electrical_load_tested=False)
(D/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
