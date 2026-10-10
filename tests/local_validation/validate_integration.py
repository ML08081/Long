"""Run the real native Loongson server against PTYs and TCP; never opens hardware."""
import os, pty, socket, struct, subprocess, tempfile, threading, time, json, signal
from pathlib import Path
import sys
ROOT=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
def xor(b):
 v=0
 for x in b: v^=x
 return v
def serial_frame(mark,p):
 b=bytes([0xaa,mark,len(p)])+p
 return b+bytes([xor(b)])
class EmulatedF4:
 def __init__(self,master):
  self.master=master; self.alive=True; self.buf=b''; self.cfg=None; self.drive=(0,0,0)
  self.env=True; self.status=False; self.dht_age=0; self.seq=1; self.history=[]
  os.set_blocking(master,False)
  self.thread=threading.Thread(target=self.run); self.thread.start()
 def run(self):
  deadline=0
  while self.alive:
   try: self.buf+=os.read(self.master,8192)
   except (BlockingIOError,OSError): pass
   while len(self.buf)>=2:
    if self.buf[0]!=0xaa: self.buf=self.buf[1:]; continue
    n=12 if self.buf[1]==0x63 else 7
    if len(self.buf)<n: break
    b,self.buf=self.buf[:n],self.buf[n:]
    if xor(b[:-1])!=b[-1]: continue
    if n==12:
     self.cfg=list(b[3:11]); self.history.append((time.monotonic(),self.cfg[:]))
    else: self.drive=struct.unpack('>hhB',b[1:6])
   now=time.monotonic()
   if now>=deadline:
    deadline=now+.1
    preview,flags,gear,seq=(self.cfg or [0,0,4,1])[:4]
    state=0 if preview==2 and not flags else 2
    reasons=(32 if flags&1 else 0)|(64 if flags&2 else 0)|(2 if preview==1 else 0)
    runtime=bytes([4,state,preview,reasons>>8,reasons&255,0,gear,seq,5,self.seq>>8,self.seq&255,self.dht_age>>8,self.dht_age&255,4,2,0])
    data=serial_frame(0x64,runtime) if self.status else b''
    data+=serial_frame(0x5a,struct.pack('>hhBHii',self.drive[0],self.drive[1],self.drive[2],100,123,456))
    if self.env: data+=serial_frame(0x5c,struct.pack('>HHbBBB',120,900,26,60,4|8|64,0))
    try: os.write(self.master,data)
    except OSError: pass
   time.sleep(.005)
 def close(self): self.alive=False; self.thread.join()
class Peer:
 def __init__(self,port):
  self.sock=socket.create_connection(('127.0.0.1',port),timeout=1); self.sock.settimeout(.1); self.buf=b''
 def send(self,typ,p): self.sock.sendall(struct.pack('<BBI',0xa5,typ,len(p))+p)
 def cmd(self,c,v): self.send(0x40,bytes([c,v]))
 def drive(self,s,t=0): self.send(0x41,struct.pack('<hh',s,t))
 def wait(self,typ,predicate=lambda p:True,seconds=3):
  until=time.monotonic()+seconds
  while time.monotonic()<until:
   while len(self.buf)>=6:
    assert self.buf[0]==0xa5
    t,n=struct.unpack('<BI',self.buf[1:6]); assert n<100000
    if len(self.buf)<6+n: break
    p,self.buf=self.buf[6:6+n],self.buf[6+n:]
    if t==typ and predicate(p): return p
   try: self.buf+=self.sock.recv(65536)
   except socket.timeout: pass
  raise AssertionError('Timed out waiting type '+hex(typ))
 def close(self): self.sock.close()
def until(fn,seconds=3):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if fn(): return
  time.sleep(.02)
 raise AssertionError('condition timeout')
with tempfile.TemporaryDirectory(prefix='patrol-integration-') as td:
 d=Path(td); reserve=socket.socket(); reserve.bind(('127.0.0.1',0)); port=reserve.getsockname()[1]; reserve.close()
 config={'camera':{'device':str(d/'no-camera')},'network':{'bind':'127.0.0.1','port':port},'link':{'type':'uart'},'serial':{'device':str(d/'control'),'thermal_device':''},'display':{'enabled':0},'unknown_preserved':{'key':'test'}}
 path=d/'config.json'; path.write_text(json.dumps(config)); original=path.read_bytes()
 log=open(d/'server.log','wb')
 app=subprocess.Popen([str(ROOT/'build-local-validation/bin/patrol_system'),'-c',str(path)],stdout=log,stderr=log)
 emulator=None; peer=None
 try:
  start=time.monotonic()
  # Listen must become available even when both the camera and initial F4 port are absent.
  for attempt in range(100):
   try: peer=Peer(port); break
   except OSError: time.sleep(.02)
  assert peer is not None and time.monotonic()-start<2
  peer.wait(1,lambda p:len(p)==42 and p[33]&1)
  master,slave=pty.openpty(); (d/'control').symlink_to(os.ttyname(slave)); emulator=EmulatedF4(master)
  peer.cmd(5,1)
  until(lambda:emulator.cfg is not None,7)
  time.sleep(.2); assert emulator.drive==(0,0,0) # F4 has not confirmed any speed yet
  emulator.status=True
  peer.wait(2,lambda p:len(p)==27 and p[1]==1,seconds=3)
  until(lambda:emulator.drive[2]==1 and emulator.drive[0]==500)
  peer.cmd(5,0); until(lambda:emulator.drive==(0,0,0))
  sensor=peer.wait(1,lambda p:p[34]>=0 and p[35]&4)
  assert struct.unpack_from('<h',sensor,4)[0]==260
  assert struct.unpack_from('<H',sensor,6)[0]==600
  for gear in range(15):
   peer.cmd(8,gear)
   peer.wait(2,lambda p:p[3]==gear and p[10]==gear and p[11]==p[25])
  assert (d/'config.json.speed').read_text().strip()=='1500'
  peer.cmd(8,255); time.sleep(.2)
  assert (d/'config.json.speed').read_text().strip()=='1500'
  assert path.read_bytes()==original
  peer.cmd(8,0); peer.wait(2,lambda p:p[10]==0)
  peer.drive(2000); until(lambda:emulator.drive[0]==100)
  for mode in (1,2,3):
   peer.cmd(5,mode); until(lambda:emulator.drive[2]==mode and emulator.drive[0]==100)
  peer.cmd(6,1); until(lambda:emulator.drive==(0,0,0))
  peer.cmd(8,14); peer.drive(2000); time.sleep(.7); assert emulator.drive==(0,0,0)
  peer.cmd(5,0); peer.drive(2000); until(lambda:emulator.drive[0]==1500)
  peer.drive(-2000); until(lambda:emulator.drive[0]==-1500)
  for mode in (1,2,3):
   peer.cmd(5,mode); until(lambda:emulator.drive[2]==mode and emulator.drive[0]==1500)
  peer.cmd(5,0); peer.drive(0); until(lambda:emulator.drive==(0,0,0))
  peer.cmd(8,15); time.sleep(.2); assert (d/'config.json.speed').read_text().strip()=='1500'
  peer.cmd(7,2); peer.wait(2,lambda p:p[6]==2 and p[5]==0)
  peer.send(0x42,bytes([1,90,2,4])+b'fire')
  peer.wait(2,lambda p:p[6]==2 and p[5]==2 and p[8]&32)
  until(lambda:emulator.cfg[1]==0,5) # visual alarm TTL, not held forever on stale recognition
  emulator.dht_age=80
  held=peer.wait(1,lambda p:not p[35]&4)
  assert held[33]&8 and struct.unpack_from('<hH',held,4)==(260,600)
  emulator.dht_age=0; emulator.seq+=1
  peer.wait(1,lambda p:p[35]&4)
  emulator.status=False
  peer.wait(1,lambda p:p[33]&4 and not p[35]&4,4)
  emulator.status=True
  peer.wait(1,lambda p:not p[33]&4 and p[35]&4)
  emulator.env=False
  held=peer.wait(1,lambda p:p[33]&2 and p[35]==0,4)
  assert held[33]&32 and struct.unpack_from('<hH',held,4)==(260,600) and struct.unpack_from('<H',held,8)[0]==120
  emulator.env=True
  peer.wait(1,lambda p:not p[33]&2 and p[35]&4)
  peer.close(); peer=None
  until(lambda:emulator.drive[0]==0 and emulator.cfg[0]==0,7)
  # A new session must not replay the previous manual drive or thermal OFF request.
  peer=Peer(port); peer.wait(2,lambda p:p[2]==0 and p[3]==14)
  assert emulator.drive[0]==0
  peer.close(); peer=None
  app.send_signal(signal.SIGTERM); app.wait(timeout=4); assert app.returncode==0
  # Restart reads the atomic speed file without touching the deployment configuration.
  app=subprocess.Popen([str(ROOT/'build-local-validation/bin/patrol_system'),'-c',str(path)],stdout=log,stderr=log)
  for attempt in range(100):
   try: peer=Peer(port); break
   except OSError: time.sleep(.02)
  assert peer is not None, 'server did not listen after restart'
  peer.wait(2,lambda p:p[3]==14)
  print('PASS: actual TCP/UART server startup <2s without devices, late F4 retry, 15 acknowledged gears, +/-1500 drive and all motion modes, start gated on F4 confirmation, stop, persistence/restart, alarm override/TTL, freshness and reconnect')
 finally:
  if peer: peer.close()
  if app.poll() is None:
   app.send_signal(signal.SIGTERM)
   try: app.wait(timeout=4)
   except subprocess.TimeoutExpired: app.kill(); app.wait()
  if emulator: emulator.close()
  log.close()
  if app.returncode not in (0,None): print((d/'server.log').read_text()[-6000:])
