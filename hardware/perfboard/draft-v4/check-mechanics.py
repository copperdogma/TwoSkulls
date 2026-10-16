"""Nominal 2D checks only; does not measure the actual assembly."""
from pathlib import Path
import json,math
D=Path(__file__).resolve().parent;pitch=2.54
pt=lambda c,r:((c-1)*pitch,(r-1)*pitch)
ant=(pt(13,-1.5)[0]-9,pt(13,-1.5)[1],pt(13,.9)[0]+9,pt(13,.9)[1])
def pdist(p,r):return math.hypot(max(r[0]-p[0],0,p[0]-r[2]),max(r[1]-p[1],0,p[1]-r[3]))
def rdist(a,b):return math.hypot(max(a[0]-b[2],b[0]-a[2],0),max(a[1]-b[3],b[1]-a[3],0))
def plug(row):
 x,y=pt(26,row+1);return(x-2,y-5,x+2,y+5)
rowgaps={r:pdist(pt(25,3),plug(r))-4 for r in range(1,7)}
assert all(rowgaps[r]<0 for r in range(1,6));assert rowgaps[6]>0
c1=(pt(11.03,7.2)[0],pt(11.03,7.2)[1],pt(14.97,13.5)[0],pt(14.97,13.5)[1])
r={'antenna_assumption':{'width_mm':18,'center_col':13,'north_row':-1.5,'south_row':.9,'position_measured':False},'target_clearance_mm':15,'NE_reserved_mount':{'col':25,'row':3,'radius_mm':4,'status':'optional reserve; v3 three actual mounts retained'},'servo_housing_assumption':{'col_center':26,'pin_origin_row':6,'body_center_row':7,'across_mm':4,'along_mm':10,'measured':False},'row_search_housing_to_NE_clearance_mm':rowgaps,'nominal_clearances_mm':{'servo_to_antenna':rdist(plug(6),ant),'servo_to_NE_reserve':rowgaps[6],'NW_mount_to_antenna':pdist(pt(2,3),ant)-4,'NE_mount_to_antenna':pdist(pt(25,3),ant)-4,'C1_body_to_antenna':rdist(c1,ant)},'signal_polyline_length_mm':{'v3':27*pitch,'v4':12*pitch},'limits':['Fixed GPIO23 escape wire begins within15mm buffer; route away from antenna and test RF','Cable constrained east/south; swept cable volume is unmeasured','Flying links require vertical clearance below soldered header tails','Small NW/cap margins depend on actual antenna position','No fabrication fit or RF acceptance established']}
assert min(r['nominal_clearances_mm'][k] for k in ['servo_to_antenna','NW_mount_to_antenna','NE_mount_to_antenna','C1_body_to_antenna'])>=15
(D/'mechanical-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r['nominal_clearances_mm'],indent=2))
