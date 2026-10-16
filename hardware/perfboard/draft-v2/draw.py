"""Render a readable assembly concept; exact body and bracket dimensions pending."""
from pathlib import Path
import json,html
D=Path(__file__).resolve().parent;p=json.loads((D/'twoskulls-v2.json').read_text())
S=19;ox=58;oy=140
X=lambda c:ox+(c-1)*S
Y=lambda r:oy+(r-1)*S
out=['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="950" viewBox="0 0 1400 950">','<rect width="1400" height="950" fill="#f5f7fa"/>']
def text(x,y,s,size=17,color='#183044',weight='normal'):
 out.append(f'<text x="{x}" y="{y}" font-family="Arial,sans-serif" font-size="{size}" font-weight="{weight}" fill="{color}">{html.escape(s)}</text>')
def line(x,y,xx,yy,c='#183044',w=3,dash=''):
 out.append(f'<path d="M{x},{y} L{xx},{yy}" fill="none" stroke="{c}" stroke-width="{w}" stroke-dasharray="{dash}"/>')
def rect(x,y,w,h,c,stroke='none',sw=1,rx=0,opacity=1):
 out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{c}" fill-opacity="{opacity}" stroke="{stroke}" stroke-width="{sw}"/>')
def circle(x,y,r,c,stroke='none',w=1):out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{c}" stroke="{stroke}" stroke-width="{w}"/>')
text(30,36,'TwoSkulls — draft v2: straight rails under removable ESP32',27,weight='bold')
text(30,65,'Planning drawing • component-side coordinates • physical fit still requires actual parts',17,'#a44224')
text(30,94,'Board seen through the ESP32; coloured wiring is on the underside.',16)
rect(ox-15,oy-15,505,620,'#e6cb94','#98733d',1,8)
for c in range(1,27):
 text(X(c)-4,oy-23,str(c),11)
 for r in range(1,33):circle(X(c),Y(r),1.6,'#a18a61')
for r in range(1,33):text(24,Y(r)+4,str(r),11)
# Module is ghosted to expose the physical rail proposal.
rect(X(7.5),Y(1),11*19,20.5*19,'#8093a6','#576d81',1,3,.14)
text(X(9),Y(1)+13,'antenna ↑',12)
text(X(9),Y(3),'ESP32 removable',14)
text(X(9),Y(4),'on raised sockets',14)
text(X(9),Y(5),'No rail near antenna',12)
# Proposed programming-cable corridor below the carrier.
out.append(f'<rect x="{X(12.5)}" y="{Y(22)}" width="{4*19}" height="{10*19}" fill="none" stroke="#788998" stroke-dasharray="5 3"/>')
out.append(f'<text transform="translate({X(14.5)},{Y(23)}) rotate(90)" font-family="Arial,sans-serif" font-size="12" fill="#52687a">Programming USB cable</text>')
# Raised USB bracket: narrower than former reserve to leave servo header free.
rect(X(19.3),Y(22),5.4*19,9.7*19,'#d6eee7','#268a74',2,3,.6)
text(X(19.6),Y(22)+16,'RAISED',12,'#166c5c','bold')
text(X(19.6),Y(22)+30,'USB-C',12,'#166c5c','bold')
# Opening/flange top view. Long flange is along rows; pins are flying leads.
rect(X(20.2),Y(24),2.64*19,7.91*19,'#9baeb9','#183044',1.5,3)
rect(X(20.9),Y(26),1.2*19,3.0*19,'#203a4d','#183044',1,4)
text(X(20.3),Y(29)+12,'UP',12,'#183044','bold')
coords={}
for part in p['parts']:
 for pin in p['kinds'][part['kind']]['pins']:
  coords[part['ref']+'.'+pin['name']]=(part['at']['col']+pin['col'],part['at']['row']+pin['row'])
def xy(s):return tuple(map(int,s[1:].split(','))) if s.startswith('@') else coords[s]
# Draw paths; rails are deliberately heavier.
for n in p['nets']:
 for a,b in n['wires']:
  x,y=xy(a);xx,yy=xy(b)
  line(X(x),Y(y),X(xx),Y(yy),n['colour'],4 if x==xx and x in (9,11) else 2.4)
# Every cross-net intersection is an insulated bridge, never a solder junction.
bridges={}
for i,n in enumerate(p['nets']):
 for other in p['nets'][i+1:]:
  for a,b in n['wires']:
   A,B=xy(a),xy(b)
   for c,d in other['wires']:
    C,E=xy(c),xy(d)
    if A[0]==B[0] and C[1]==E[1]:
     v0,v1,h0,h1,vc,hc=A,B,C,E,n['colour'],other['colour']
    elif A[1]==B[1] and C[0]==E[0]:
     v0,v1,h0,h1,vc,hc=C,E,A,B,other['colour'],n['colour']
    else: continue
    col,row=v0[0],h0[1]
    if min(v0[1],v1[1])<=row<=max(v0[1],v1[1]) and min(h0[0],h1[0])<=col<=max(h0[0],h1[0]):
     bridges[(col,row)]=(vc,hc)
for (c,r),(vertical,horizontal) in bridges.items():
 circle(X(c),Y(r),5,'#f5f7fa')
 line(X(c),Y(r)-6,X(c),Y(r)+6,vertical,3)
 out.append(f'<path d="M{X(c)-8},{Y(r)} Q{X(c)},{Y(r)-10} {X(c)+8},{Y(r)}" fill="none" stroke="{horizontal}" stroke-width="2.4"/>')
(D/'insulated-crossings.json').write_text(json.dumps([{'col':c,'row':r,'vertical_colour':v,'horizontal_colour':h} for (c,r),(v,h) in sorted(bridges.items())],indent=2)+'\n')
# Component bodies outside the module.
circle(X(10),Y(28),5/2.54*S,'#d5dee5','#183044',2)
text(X(10)-26,Y(28)-4,'C1 470µF',11,weight='bold');text(X(10)-24,Y(28)+12,'+    −',14)
rect(X(10)-22,Y(25)-10,44,20,'#e5b774','#714f21',1,3)
text(X(10)-8,Y(25)+4,'C2',11)
circle(X(3),Y(23),3.15/2.54*S,'#d5dee5','#183044',1);text(X(3)-10,Y(23)+4,'C3',12)
rect(X(6)-10,Y(23.4),20,32,'#293b48','#183044',1,2)
rect(X(6)-10,Y(24.6),20,5,'#c4cbd1');text(X(4.4),Y(24),'D1',12)
for part in p['parts']:
 for pin in p['kinds'][part['kind']]['pins']:
  c,r=coords[part['ref']+'.'+pin['name']]
  circle(X(c),Y(r),3.5,'white','#183044',1)
# Socket landings are specifically differentiated from the raised connector.
text(X(16.3),Y(28)-10,'J4 wire pads',12,weight='bold')
text(X(24.3),Y(23)-12,'J3',12,weight='bold')
text(X(24.2),Y(23)+4,'SIG',10);text(X(24.2),Y(24)+4,'+5',10);text(X(24.2),Y(25)+4,'GND',10)
text(X(1)+6,Y(7)-7,'J2 eyes',12,weight='bold')
# Proposed three mounting points.
for c,r in [(2,3),(2,31),(25,31)]:
 circle(X(c),Y(r),4/2.54*S,'#f7f5eb','#183044',1)
 circle(X(c),Y(r),5,'white','#183044',1)
# Rail labels and USB access corridor label.
text(65,785,'Red: external +5V rail (column 9)',16,'#ae3329','bold')
text(65,810,'Blue: common ground rail (column 11)',16,'#226da0','bold')
text(65,835,'Orange: separate ESP32 supply after diode',15,'#b46b16')
text(65,860,'C1 / C2 bridge the rails below the module.',15)
text(65,885,'Arched crossings are insulated; no solder joint.',15)
# RIGHT: unambiguous side profile of the real arrangement.
text(650,115,'USB-C: opening up, cable enters vertically',22,weight='bold')
# Skull support and perfboard.
out.append('<path d="M680 443 Q1015 487 1350 443" fill="none" stroke="#78848c" stroke-width="9"/>')
rect(720,426,75,15,'#ce9b6a',rx=5);rect(1230,426,75,15,'#ce9b6a',rx=5)
line(758,426,758,396,'#3d5364',10);line(1268,426,1268,396,'#3d5364',10)
rect(685,387,660,10,'#bc9854')
# Cradle independently holds flange, PCB tail hangs inside it.
line(940,386,940,290,'#268a74',10);line(1180,386,1180,290,'#268a74',10)
rect(930,280,260,10,'#268a74')
rect(958,271,205,8,'#8ca0ad','#183044',1)
rect(1024,264,73,15,'#9db1bf','#183044',2,4)
# Tiny PCB below flange, both power leads carry no socket insertion load.
rect(1047,280,26,60,'#3a846b','#183044',1)
line(1052,340,1040,386,'#c0392b',3);line(1067,340,1085,386,'#2980b9',3)
# Plug poised above opening; clear motion arrow.
rect(1031,176,58,32,'#a5b8c3','#183044',1,4)
rect(1038,208,44,30,'#d2dce1','#183044',1,3)
line(1060,145,1060,176,'#344e63',9)
line(1120,205,1120,251,'#183044',3);line(1113,240,1120,251,'#183044',3);line(1127,240,1120,251,'#183044',3)
text(1170,220,'Plug moves down',15)
text(650,274,'Flange horizontal',15);line(802,270,955,275,'#65798b',1)
text(650,320,'Bracket takes cable force',15);line(843,316,934,318,'#65798b',1)
text(650,357,'Connector PCB hangs below',15);line(869,353,1045,323,'#65798b',1)
text(1110,355,'Flexible wires',15)
text(650,507,'Bracket height and flange hole centres: measure before building.',16,'#a44224')
# SECOND side view: ESP32 on raised headers with underside rails.
text(650,556,'ESP32 remains removable',22,weight='bold')
rect(750,605,460,12,'#287d68','#183044',1)
rect(750,617,25,55,'#35495a');rect(1185,617,25,55,'#35495a')
text(1225,644,'Raised sockets',15)
rect(700,672,580,10,'#bc9854')
# Rails shown below perfboard cross-section.
circle(905,692,6,'#c0392b');circle(963,692,6,'#2980b9')
text(835,727,'Rails on underside of perfboard',16)
text(650,777,'Capacitors sit beyond the ESP32 body, not squeezed underneath.',16)
text(650,805,'No new rail or wire beneath the antenna end.',16)
text(650,833,'Leave the centre programming-USB cable path open.',16)
text(650,872,'Side views are conceptual, not dimensioned fabrication drawings.',14,'#52687a')
text(30,930,'26 × 32 holes · 2.54 mm grid · socket spacing assumed 25.4 mm · verify body, plug, SD and bracket clearances',15)
out.append('</svg>')
s='\n'.join(out);(D/'assembly-plan.svg').write_text(s)
# Quick Look's square preview avoids its nonsquare clipping issue.
Path('/tmp/twoskulls-v2-square.svg').write_text(s.replace('height="950" viewBox="0 0 1400 950"','height="1400" viewBox="0 0 1400 1400"',1))
