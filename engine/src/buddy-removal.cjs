'use strict';

const RESTART_REASON='Buddy removed; restarting Pokémon GO';
const RPC={buddy_check:1350,buddy_remove:152};

class BuddyRemoval {
  constructor(enabled) {this.state=enabled===true?'check':'disabled';}
  get action() {return this.state==='check'?'buddy_check':this.state==='remove'?'buddy_remove':null;}
  skip(reason) {if(this.action){this.state='skipped';this.reason=reason;}}
  sent(kind) {
    if(this.action!==kind)throw Error('Unexpected buddy operation');
    this.state=kind==='buddy_check'?'checking':'removing';
  }
  settle(kind,result) {
    if(this.state!==(kind==='buddy_check'?'checking':'removing')||!Object.hasOwn(RPC,kind))
      throw Error('Unexpected buddy response');
    if(result!==1){this.state='failed';this.reason='server_result_'+result;return false;}
    this.state=kind==='buddy_check'?'remove':'removed';
    return this.state==='removed';
  }
  reject(reason) {this.state='failed';this.reason=reason;}
}

module.exports={BuddyRemoval,RESTART_REASON,RPC};
