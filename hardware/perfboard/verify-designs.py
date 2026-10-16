"""Read-only consistency checks for saved perfboard drafts; no hardware acceptance."""
from pathlib import Path
import json,zipfile,csv,ast,struct,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parent
base=None
for version in range(1,5):
 d=root/f'draft-v{version}';p=json.loads((d/f'twoskulls-v{version}.json').read_text())
 coords={}
 for part in p['parts']:
  assert part['rotation']==0
  for pin in p['kinds'][part['kind']]['pins']:
   s=part['ref']+'.'+pin['name'];xy=(part['at']['col']+pin['col'],part['at']['row']+pin['row']);assert s not in coords;coords[s]=xy
   assert 1<=xy[0]<=p['board']['cols'] and 1<=xy[1]<=p['board']['rows']
 def xy(s):return tuple(map(int,s[1:].split(','))) if s.startswith('@') else coords[s]
 def on(pt,a,b):return (pt[0]-a[0])*(b[1]-a[1])==(pt[1]-a[1])*(b[0]-a[0]) and min(a[0],b[0])<=pt[0]<=max(a[0],b[0]) and min(a[1],b[1])<=pt[1]<=max(a[1],b[1])
 expected=json.loads((d/'intended-netpaths.json').read_text());expected=expected.get('paths',expected)
 membership={}
 for n in p['nets']:
  membership[n['name']]={s for s in n['pins'] if not s.startswith('@')}
  assert all(xy(s) for s in n['pins'])
  want={frozenset((a,b)) for chain in expected[n['name']] for a,b in zip(chain,chain[1:])}
  assert {frozenset(w) for w in n['wires']}==want
  seen={n['pins'][0]}
  while True:
   nxt=seen|{v for a,b in n['wires'] for u,v in [(a,b),(b,a)] if u in seen}
   if nxt==seen:break
   seen=nxt
  assert seen==set(n['pins'])
  for a,b in n['wires']:
   for pin,pt in coords.items():assert pin in n['pins'] or not on(pt,xy(a),xy(b)),(version,n['name'],a,b,pin)
   for other in p['nets']:
    if other==n:continue
    for j in other['pins']:
     if j.startswith('@'):assert not on(xy(j),xy(a),xy(b)),(version,n['name'],a,b,j)
 if base is None:base=membership
 assert base==membership
 for z in d.glob('*.zip'):
  with zipfile.ZipFile(z) as f:
   assert f.testzip() is None
   q=json.loads(f.read('project.json'))
  assert all(p['board'][k]==q['board'][k] for k in ['cols','rows','pitch','margin'])
  for k in ['kinds','parts','nets','keepouts','notes','solderedLinks']:assert p[k]==q[k],(z,k)
 with (d/'wiring.csv').open() as f:
  records=list(csv.reader(f))[1:]
  actual={(row[0],row[1],ast.literal_eval(row[2]),row[3],ast.literal_eval(row[4])) for row in records}
  wanted={(n['name'],a,xy(a),b,xy(b)) for n in p['nets'] for a,b in n['wires']}
  assert actual==wanted
 for img in d.glob('*.png'):
  data=img.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';w,h=struct.unpack('>II',data[16:24]);assert w>500 and h>500
 for svg in d.glob('*.svg'):ET.parse(svg)
 print(f'v{version}: {len(p["parts"])} parts, {len(p["nets"])} connected nets, {sum(len(n["wires"]) for n in p["nets"])} segments; ZIP/CSV/pins/figures pass')
print('All four drafts preserve electrical terminal membership; no physical or load test implied.')

trial=root/'tool-trial'
p=json.loads((trial/'perfboarder-trial.json').read_text())
q=json.loads((trial/'reopened-project.json').read_text())
with zipfile.ZipFile(trial/'perfboarder-trial.zip') as z:
 assert z.testzip() is None
 saved=json.loads(z.read('project.json'))
for k in ['board','kinds','parts','nets','keepouts','notes','solderedLinks']:
 assert p[k]==q[k]==saved[k],('trial',k)
print('Tool trial: saved export/reimport data match.')
