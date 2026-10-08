// Own single-attach startup gate. No game calls or native references here.
class StartupGate {
  constructor(sessionId, playerId) {
    if(typeof sessionId!=='string'||!/^[A-Za-z0-9_-]{8,96}$/.test(sessionId)
      ||typeof playerId!=='string'||!playerId||playerId.length>256)throw Error('Invalid startup identity');
    this.sessionId=sessionId;this.playerId=playerId;this.epoch=0;this.active=false;this.reset();
  }
  reset() {this.start=null;this.last=null;this.samples=0;this.proof=null;this.epoch++;}
  observe(now, foreground, references) {
    if(this.active)return null;
    if(!Number.isFinite(now)||!foreground||!Number.isInteger(references)||references<4){this.reset();return null;}
    if(this.last!==null&&(now<this.last||now-this.last>1500))this.reset();
    if(this.start===null)this.start=now;
    if(this.last===null||now-this.last>=500){this.last=now;this.samples++;}
    if(this.proof&&now-this.proof.createdAt>5000){this.reset();return null;}
    if(!this.proof&&now-this.start>=10000&&this.samples>=20){
      this.proof=Object.freeze({sessionId:this.sessionId,playerId:this.playerId,
        token:this.sessionId+':'+this.epoch,createdAt:now,elapsedMs:now-this.start,
        samples:this.samples,cachedReferences:references,gameRequestsSent:0});
      return this.proof;
    }
    return null;
  }
  accept(command, now) {
    const p=this.proof;
    if(this.active||!p||command?.sessionId!==p.sessionId||command.playerId!==p.playerId||command.token!==p.token
      ||now<p.createdAt||now-p.createdAt>5000||this.last===null||now-this.last>1500)return false;
    this.active=true;this.proof=null;return true;
  }
}
module.exports={StartupGate};
