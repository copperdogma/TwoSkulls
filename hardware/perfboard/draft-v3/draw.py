"""Render a readable assembly concept; exact body and bracket dimensions pending."""
from pathlib import Path
import json,html
D=Path(__file__).resolve().parent;p=json.loads((D/'twoskulls-v3.json').read_text())
S=19;ox=58;oy=220
X=lambda c:ox+(c-1)*S
Y=lambda r:oy+(r-1)*S
out=['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="1040" viewBox="0 0 1400 1040">','<rect width="1400" height="1040" fill="#f5f7fa"/>']
def text(x,y,s,size=17,color='#183044',weight='normal'):
 out.append(f'<text x="{x}" y="{y}" font-family="Arial,sans-serif" font-size="{size}" font-weight="{weight}" fill="{color}">{html.escape(s)}</text>')
def line(x,y,xx,yy,c='#183044',w=3,dash=''):
 out.append(f'<path d="M{x},{y} L{xx},{yy}" fill="none" stroke="{c}" stroke-width="{w}" stroke-dasharray="{dash}"/>')
def rect(x,y,w,h,c,stroke='none',sw=1,rx=0,opacity=1):
 out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{c}" fill-opacity="{opacity}" stroke="{stroke}" stroke-width="{sw}"/>')
def circle(x,y,r,c,stroke='none',w=1):out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{c}" stroke="{stroke}" stroke-width="{w}"/>')
text(30,36,'TwoSkulls — v3: centered flush USB-C, flat underside components',26,weight='bold')
text(30,65,'Review draft • cutout dimensions, cable interference and riser height require a physical fit check',17,'#a44224')
text(30,103,'Plan shown through the board',21,weight='bold')
text(30,129,'Coordinates stay component-side; shaded parts are UNDERNEATH.',15)
rect(ox-15,oy-15,505,620,'#e6cb94','#98733d',1,8)
for c in range(1,27):
 text(X(c)-4,oy-62,str(c),11)
 for r in range(1,33):circle(X(c),Y(r),1.6,'#a18a61')
for r in range(1,33):text(24,Y(r)+4,str(r),11)
# Carrier and protruding antenna are illustrative, not a measured body offset.
rect(X(7.5),Y(-1.5),11*19,23*19,'#8093a6','#576d81',1,3,.13)
rect(X(7.5),Y(-1.5),11*19,2.4*19,'#d9e3eb','#576d81',1,3,.7)
text(X(8),Y(-.15),'Antenna overhang*',13,weight='bold')
text(X(8.8),Y(2),'ESP32 on raised sockets',13)
text(X(8.8),Y(3),'Removable from top',13)
# Show the two actual proposed underside body envelopes.
rect(X(11.03)-5,Y(9.2)-5,3.94*19+10,6.3*19+10,'#f0f5f8','#7d8994',1,5,.75)
rect(X(11.03),Y(9.2),3.94*19,6.3*19,'#c6d4dd','#183044',2,13)
rect(X(14.5),Y(9.6),.25*19,5.4*19,'#7c909f')
text(X(11.3),Y(11),'C1 470µF',12,weight='bold')
text(X(11.4),Y(12),'flat below',12)
text(X(11.4),Y(13),'on support',12)
rect(X(2.76),Y(14.8),2.48*19,4.7*19,'#c6d4dd','#183044',1.5,10)
text(X(3),Y(16),'C3',12,weight='bold')
text(X(2.9),Y(17),'flat',12)
# Socket flange lies on board, with its PCB descending through a measured-later slot.
rect(X(9.04),Y(24.68),7.92*19,2.64*19,'#9fb0bb','#183044',1.5,3)
out.append(f'<rect x="{X(11)}" y="{Y(25)}" width="{4*19}" height="{2*19}" fill="#edf6f0" stroke="#268a74" stroke-width="2" stroke-dasharray="4 3"/>')
rect(X(11.4),Y(25.5),3.2*19,1*19,'#203c4e','#183044',1,4)
text(X(9.1),Y(28.7),'Flush power USB-C ↑',13,weight='bold')
text(X(9.1),Y(29.7),'PCB slot* through board',12,'#166c5c')
# ESP32 USB port and a possible plug body projected onto the plan.
rect(X(11.6),Y(20.5),2.8*19,.8*19,'#9fb0bb','#183044',1.5,3)
out.append(f'<rect x="{X(10.8)}" y="{Y(21.4)}" width="{4.4*19}" height="{5.2*19}" fill="none" stroke="#cc7832" stroke-width="2" stroke-dasharray="6 4"/>')
text(X(15.7),Y(21.3),'ESP USB',11)
# Real electrical holes and wires.
coords={}
for part in p['parts']:
 for pin in p['kinds'][part['kind']]['pins']:
  coords[part['ref']+'.'+pin['name']]=(part['at']['col']+pin['col'],part['at']['row']+pin['row'])
def xy(s):return tuple(map(int,s[1:].split(','))) if s.startswith('@') else coords[s]
for n in p['nets']:
 for a,b in n['wires']:
  x,y=xy(a);xx,yy=xy(b)
  line(X(x),Y(y),X(xx),Y(yy),n['colour'],3.5 if x==xx and x in (12,14) else 2.4)
# All cross-net intersections are insulated bridges.
bridges={}
for i,n in enumerate(p['nets']):
 for other in p['nets'][i+1:]:
  for a,b in n['wires']:
   A,B=xy(a),xy(b)
   for c,d in other['wires']:
    C,E=xy(c),xy(d)
    if A[0]==B[0] and C[1]==E[1]:v0,v1,h0,h1,vc,hc=A,B,C,E,n['colour'],other['colour']
    elif A[1]==B[1] and C[0]==E[0]:v0,v1,h0,h1,vc,hc=C,E,A,B,other['colour'],n['colour']
    else:continue
    col,row=v0[0],h0[1]
    if min(v0[1],v1[1])<=row<=max(v0[1],v1[1]) and min(h0[0],h1[0])<=col<=max(h0[0],h1[0]):bridges[(col,row)]=(vc,hc)
for (c,r),(vc,hc) in bridges.items():
 circle(X(c),Y(r),5,'#f5f7fa');line(X(c),Y(r)-6,X(c),Y(r)+6,vc,3)
 out.append(f'<path d="M{X(c)-8},{Y(r)} Q{X(c)},{Y(r)-10} {X(c)+8},{Y(r)}" fill="none" stroke="{hc}" stroke-width="2.4"/>')
(D/'insulated-crossings.json').write_text(json.dumps([{'col':c,'row':r} for c,r in sorted(bridges)],indent=2)+'\n')
# Components and pin marks; all discrete bodies here are underside.
rect(X(12.1),Y(18)-8,34,16,'#e5b774','#714f21',1,3);text(X(12.3),Y(18)+4,'C2',11)
rect(X(6)-9,Y(25),18,31,'#293b48','#183044',1,2);rect(X(6)-9,Y(26.3),18,5,'#c4cbd1');text(X(4.4),Y(26),'D1',12)
for part in p['parts']:
 for pin in p['kinds'][part['kind']]['pins']:
  c,r=coords[part['ref']+'.'+pin['name']];circle(X(c),Y(r),3.5,'white','#183044',1)
text(X(12)-5,Y(16)+15,'+',13);text(X(14)-4,Y(16)+15,'−',13)
text(X(3)-4,Y(20)+15,'+',13);text(X(5)-4,Y(20)+15,'−',13)
text(X(10.5),Y(23)-10,'J4 underside wire pads',11,weight='bold')
text(X(1)+6,Y(7)-7,'J2 eyes',12,weight='bold')
text(X(24.1),Y(21)-13,'J3 servo',12,weight='bold')
text(X(24),Y(21)+4,'SIG',10);text(X(24),Y(22)+4,'+5',10);text(X(24),Y(23)+4,'GND',10)
for c,r in [(2,3),(2,31),(25,31)]:circle(X(c),Y(r),4/2.54*S,'#f7f5eb','#183044',1);circle(X(c),Y(r),5,'white','#183044',1)
text(55,871,'Red: external +5V rail, column 12',16,'#ae3329','bold')
text(55,897,'Blue: ground rail, column 14',16,'#226da0','bold')
text(55,923,'Orange: ESP32 supply after diode',15,'#b46b16')
text(55,949,'Arches = insulated crossings, not solder joints.',15)
text(55,975,'*Slot / antenna offsets are illustrative; measure first.',14,'#a44224')
# SIDE VIEW: north to left, south to right. Flush flange, PCB through perfboard.
text(650,111,'Side view: flush socket and possible cable interference',21,weight='bold')
text(650,141,'NORTH / antenna',14);text(1190,141,'SOUTH / power inlet',14)
rect(700,333,620,10,'#bc9854')
# Cut gap in perfboard for socket PCB.
rect(1080,331,22,14,'#f5f7fa')
# Raised sockets and carrier.
rect(728,241,23,90,'#35495a');rect(956,241,23,90,'#35495a')
rect(660,229,335,12,'#287d68','#183044',1)
rect(980,228,25,18,'#9db1bf','#183044',1)
# Flush socket flange and vertical PCB.
rect(1053,328,78,5,'#9fb0bb','#183044',1)
rect(1071,322,43,7,'#9fb0bb','#183044',1,2)
rect(1082,334,18,54,'#39836a','#183044',1)
line(1087,388,1041,346,'#c0392b',3);line(1096,388,1060,350,'#2980b9',3)
# Illustrative plugged cable bodies: dashed, not measured.
out.append('<rect x="1006" y="222" width="134" height="28" rx="6" fill="#f5c28b" fill-opacity=".22" stroke="#c7772d" stroke-width="2" stroke-dasharray="6 4"/>')
out.append('<rect x="1065" y="186" width="54" height="128" rx="6" fill="#f5c28b" fill-opacity=".22" stroke="#c7772d" stroke-width="2" stroke-dasharray="6 4"/>')
rect(1066,223,52,26,'#e9a359',opacity=.45)
text(650,189,'ESP32 programming cable →',15,'#a96324');line(882,185,1030,217,'#a96324',1)
text(1143,205,'Power cable ↓',15,'#a96324');line(1145,210,1119,224,'#a96324',1)
text(650,288,'Raised removable ESP32',16)
text(1140,363,'Flange rests on board',15);line(1138,352,1126,333,'#65798b',1)
text(650,397,'Slot lets connector PCB hang underneath; retain the flange.',16)
text(650,429,'Dashed plug bodies show the intended interference only.',16,'#a44224')
text(650,456,'Test actual cables in both insertion orders before relying on it.',16,'#a44224')
# UNDERSIDE side section, capacitor lying flat and clearance to skull.
text(650,515,'Underside: capacitor lies flat, protected by risers',21,weight='bold')
rect(700,558,620,10,'#bc9854')
circle(907,576,5,'#c0392b');circle(935,576,5,'#2980b9')
rect(833,583,247,12,'#e5eaf0','#70838f',1,5)
rect(839,601,235,63,'#c6d4dd','#183044',1.5,23)
line(1069,608,1100,575,'#183044',2);line(1072,620,1119,576,'#183044',2)
text(875,640,'Flat capacitor body',16)
rect(740,568,14,163,'#35495a');rect(1255,568,14,163,'#35495a')
rect(716,732,63,14,'#ce9b6a',rx=6);rect(1231,732,63,14,'#ce9b6a',rx=6)
out.append('<path d="M675 755 Q1000 795 1340 755" fill="none" stroke="#78848c" stroke-width="9"/>')
text(1100,622,'Insulate and support',15)
text(1100,644,'body above wiring;',15)
text(1100,666,'do not load its leads.',15)
text(650,831,'Clearance = capacitor diameter + support/wiring + shell-flex gap.',16)
text(650,861,'Provisional C1 diameter 10 mm; C3 diameter 6.3 mm.',16)
text(650,891,'Actual connector tail depth may require taller risers.',16)
text(650,939,'Side views are conceptual, not dimensioned fabrication drawings.',14,'#52687a')
text(30,1020,'26 × 32 holes · 2.54 mm pitch · assumed 25.4 mm socket-row spacing · no verified cutout template or cable interlock',15)
out.append('</svg>');s='\n'.join(out);(D/'assembly-plan.svg').write_text(s)
Path('/tmp/twoskulls-v3-square.svg').write_text(s.replace('height="1040" viewBox="0 0 1400 1040"','height="1400" viewBox="0 0 1400 1400"',1))
