'use strict';

const POKEMON='Holoholo.Rpc.PokemonProto';
const MULTI='Niantic.Holoholo.UI.PokemonInventoryMultiSelect';
const LIST='Niantic.Holoholo.UI.PokemonInventoryListLine';
const CACHE_SIZE=128,MAX_SELECTION=10000;
const MANUAL_PROFILE=Object.freeze({mode:'ebpm',selection:'cdjt',batch:'cdio'});

function formatIvs(data) {
  if(data.egg!==false||![data.attack,data.defense,data.stamina].every(value=>Number.isInteger(value)&&value>=0&&value<=15))return null;
  return `${Math.floor(((data.attack+data.defense+data.stamina)/45)*100)}% ${data.attack}/${data.defense}/${data.stamina}`;
}
function checkedMethod(type,name,parameters,result) {
  const method=type.tryMethod(name,parameters.length);
  if(!method||method.isStatic||method.returnType.name!==result||method.virtualAddress.isNull()
    ||method.parameters.length!==parameters.length||method.parameters.some((parameter,index)=>parameter.type.name!==parameters[index]))
    throw Error('Incompatible inventory method: '+type.fullName+'.'+name);
  return method;
}
function checkedField(type,name,expected) {
  const field=type.tryField(name);
  if(!field||field.isStatic||field.offset<16||field.type.name!==expected)
    throw Error('Incompatible inventory field: '+type.fullName+'.'+name);
  return field;
}

function install(runtime,options,report) {
  const hooks=[],cache=new Map(),references=new Set(),batches=new Set(),errors=[];
  let closed=false,names=false,manual=false,nameReads=0,selectionChecks=0,manualBatches=0,failures=0;
  let manualCompletions=0,manualSuccessfulBatches=0;
  let operations=0;
  let selectionDisabled,clonePokemon;
  function enabled(feature) {return !closed&&(feature==='names'?names:manual)&&runtime.enabled()&&runtime.sameThread();}
  function error(feature,phase,cause) {
    if(feature==='names')names=false;else manual=false;
    failures++;report('inventory_tools_error',{feature,phase,message:String(cause),farmingContinues:true});
  }
  function hold(value,label) {
    const reference=runtime.retain(value,'inventory:'+label);references.add(reference);return reference;
  }
  function free(reference) {
    if(!reference||!references.has(reference))return;
    try {reference.free();references.delete(reference);}
    catch(cause){names=false;manual=false;errors.push(String(cause));report('inventory_tools_error',{phase:'release',message:String(cause)});}
  }
  function pokemon(value) {
    if(!runtime.valid(value)||value.class.fullName!==POKEMON)throw Error('Invalid inventory Pokemon');
    return value;
  }
  function blocked(value) {
    const mon=pokemon(value);
    const favorite=mon.field('favorite_').value,egg=mon.field('isEgg_').value,fusion=mon.field('isFusion_').value;
    if([favorite,egg,fusion].some(value=>typeof value!=='boolean'))throw Error('Invalid inventory protection');
    if(favorite||egg||fusion)return true;
    const limited=mon.field('limitedPokemonIdentifier_').value;
    if(runtime.stringContent(limited))return true;
    let checked=mon,reference=null;
    try {
      const originalDisplay=mon.field('pokemonDisplay_').value;
      const hasDisplay=runtime.valid(originalDisplay);
      if(hasDisplay&&originalDisplay.class.fullName!=='Holoholo.Rpc.PokemonDisplayProto')throw Error('Invalid selected Pokemon display');
      const shiny=hasDisplay?originalDisplay.field('Shiny').value:false,lucky=mon.field('isLucky_').value;
      if(typeof shiny!=='boolean'||typeof lucky!=='boolean')throw Error('Invalid special selection flags');
      if(shiny||lucky) {
        checked=pokemon(clonePokemon.bind(mon).invoke());
        if(checked.handle.equals(mon.handle))throw Error('Inventory clone aliases original');
        reference=hold(checked,'eligibility-copy');
        checked.field('isLucky_').value=false;
        if(shiny) {
          const display=checked.field('pokemonDisplay_').value;
          if(!runtime.valid(display)||display.handle.equals(originalDisplay.handle))throw Error('Display clone aliases original');
          display.field('Shiny').value=false;
        }
      }
      const disabled=selectionDisabled.bind(checked).invoke(runtime.masterData());
      if(typeof disabled!=='boolean')throw Error('Invalid game selection result');
      if(disabled)return true;
    }finally{free(reference);}
    const id=mon.field('id_').value,text=id.toString();
    if(!/^[1-9][0-9]{0,19}$/.test(text)||BigInt(text)>18446744073709551615n)throw Error('Invalid selected Pokemon identity');
    const buddy=runtime.isBuddy(id);
    if(typeof buddy!=='boolean')throw Error('Invalid buddy selection result');
    return buddy;
  }
  function stringAddress(text) {
    let reference=cache.get(text);
    if(reference){cache.delete(text);cache.set(text,reference);return runtime.address(reference);}
    const value=runtime.string(text);reference=hold(value,'iv-label');cache.set(text,reference);
    if(cache.size>CACHE_SIZE){const [key,old]=cache.entries().next().value;cache.delete(key);free(old);}
    return runtime.address(reference);
  }
  if(options.showIvNames)try {
    const type=runtime.resolve('holo-protos',POKEMON);
    const ivFields=['individualAttack_','individualDefense_','individualStamina_'].map(name=>checkedField(type,name,'System.Int32'));
    const egg=checkedField(type,'isEgg_','System.Boolean');
    const getter=checkedMethod(type,'get_Nickname',[],'System.String');
    names=true;
    hooks.push(runtime.attach(getter.virtualAddress,{
      onEnter(args) {
        this.silphIvText=null;
        if(!enabled('names'))return;
        try {
          const mon=pokemon(runtime.object(args[0]));
          this.silphIvText=formatIvs({attack:ivFields[0].bind(mon).value,defense:ivFields[1].bind(mon).value,
            stamina:ivFields[2].bind(mon).value,egg:egg.bind(mon).value});
        }catch(cause){error('names','read',cause);}
      },
      onLeave(result) {
        if(!this.silphIvText||!enabled('names'))return;
        operations++;
        try {result.replace(stringAddress(this.silphIvText));nameReads++;}
        catch(cause){error('names','display',cause);}
        finally{operations--;}
      },
    }));
  }catch(cause){error('names','profile',cause);}
  if(options.extendedManualTransfer)try {
    const type=runtime.resolve('holo-protos',POKEMON),multi=runtime.resolve('holo-game',MULTI),list=runtime.resolve('holo-game',LIST);
    for(const name of ['favorite_','isEgg_','isFusion_','isLucky_'])checkedField(type,name,'System.Boolean');
    checkedField(type,'id_','System.UInt64');checkedField(type,'limitedPokemonIdentifier_','System.String');
    checkedField(type,'pokemonDisplay_','Holoholo.Rpc.PokemonDisplayProto');
    checkedField(runtime.resolve('holo-protos','Holoholo.Rpc.PokemonDisplayProto'),'Shiny','System.Boolean');
    selectionDisabled=checkedMethod(type,'IsSelectionDisabled',['IGameMasterData'],'System.Boolean');
    clonePokemon=checkedMethod(type,'Clone',[],POKEMON);
    const mode=checkedField(multi,MANUAL_PROFILE.mode,'Niantic.Holoholo.UI.SelectMode');
    if(!mode.type.class.isEnum||mode.type.class.baseType?.name!=='System.Int32')throw Error('Incompatible selection mode');
    const selected=checkedField(list,'<SelectedPokemon>k__BackingField','System.Collections.Generic.List<Holoholo.Rpc.PokemonProto>');
    checkedField(list,'<IsRemoteTrade>k__BackingField','System.Boolean');
    const gate=checkedMethod(multi,MANUAL_PROFILE.selection,[POKEMON],'System.Boolean');
    const completed=checkedMethod(multi,'TransferComplete',['System.Boolean'],'System.Void');
    const batch=checkedMethod(list,MANUAL_PROFILE.batch,['System.Int32',selected.type.name,'Niantic.Holoholo.UI.IModalButton[]','Niantic.Holoholo.UI.IModalButton[]'],'System.Void');
    manual=true;
    const installed=[];
    try {
      installed.push(runtime.attach(gate.virtualAddress,{
        onEnter(args) {
          this.silphBlocked=null;
          if(!enabled('manual'))return;
          operations++;
          try {
            const controller=runtime.object(args[0]);
            if(!runtime.valid(controller)||controller.class.fullName!==MULTI)return;
            if(runtime.number(controller.field(mode.name).value)!==0)return;
            this.silphBlocked=blocked(runtime.object(args[1]));
          }catch(cause){error('manual','selection',cause);}
          finally{operations--;}
        },
        onLeave(result) {
          if(this.silphBlocked===null||!enabled('manual'))return;
          try {result.replace(runtime.pointer(this.silphBlocked?1:0));selectionChecks++;}
          catch(cause){error('manual','selection-result',cause);}
        },
      }));
      installed.push(runtime.attach(batch.virtualAddress,{
        onEnter(args) {
          this.silphBatch=null;
          if(!enabled('manual'))return;
          let reference=null;
          operations++;
          try {
            const controller=runtime.object(args[0]);
            if(!runtime.valid(controller)||controller.class.fullName!==LIST||controller.field('<IsRemoteTrade>k__BackingField').value!==false)return;
            const selection=controller.field(selected.name).value;
            if(!runtime.valid(selection)||selection.class.type.name!==selected.type.name)throw Error('Invalid manual selection list');
            const count=selection.method('get_Count',0).invoke();
            if(!Number.isInteger(count)||count<0||count>MAX_SELECTION)throw Error('Invalid manual selection count');
            const candidates=[],ids=new Set();
            for(let index=0;index<count;index++) {
              const mon=pokemon(selection.method('get_Item',1).invoke(index));
              if(blocked(mon))continue;
              const id=mon.field('id_').value.toString();
              if(!/^[1-9][0-9]{0,19}$/.test(id))throw Error('Invalid selected Pokemon identity');
              if(!ids.has(id)){ids.add(id);candidates.push(mon);}
            }
            if(!enabled('manual'))throw Error('Manual inventory tools inactive');
            const replacement=selection.class.alloc();reference=hold(replacement,'manual-selection');
            checkedMethod(replacement.class,'.ctor',[],'System.Void').bind(replacement).invoke();
            const add=checkedMethod(replacement.class,'Add',[POKEMON],'System.Void').bind(replacement);
            for(const mon of candidates)add.invoke(mon);
            args[2]=replacement.handle;this.silphBatch=reference;batches.add(reference);reference=null;manualBatches++;
            report('manual_transfer_selection',{selected:count,eligible:candidates.length,excluded:count-candidates.length,
              batch:manualBatches,source:'USER_INVENTORY_SELECTION',confirmation:'GAME_DIALOG',automaticTransfer:false});
          }catch(cause){free(reference);error('manual','batch',cause);}
          finally{operations--;}
        },
        onLeave() {
          const reference=this.silphBatch;this.silphBatch=null;batches.delete(reference);free(reference);
        },
      }));
      installed.push(runtime.attach(completed.virtualAddress,{
        onEnter(args) {
          if(!enabled('manual'))return;
          try {
            const controller=runtime.object(args[0]);
            if(!runtime.valid(controller)||controller.class.fullName!==MULTI)return;
            const result=args[1].toInt32();
            if(result!==0&&result!==1)throw Error('Invalid manual completion result');
            manualCompletions++;if(result===1)manualSuccessfulBatches++;
            report('manual_transfer_result',{success:result===1,source:'GAME_UI_CALLBACK',
              completion:manualCompletions,manualSuccessfulBatches,serverReceiptRead:false,automaticTransfer:false});
          }catch(cause){error('manual','completion',cause);}
        },
      }));
      hooks.push(...installed);
    }catch(cause){hooks.push(...installed);throw cause;}
  }catch(cause){error('manual','profile',cause);}
  report('inventory_tools_profile',{ivNames:names,extendedManualTransfer:manual,temporaryNames:true,
    integerPercent:true,automaticTransfersChanged:false,cacheLimit:CACHE_SIZE,
    manualProfile:manual?MANUAL_PROFILE:null});
  function snapshot() {
    return {ivNames:names&&!closed,extendedManualTransfer:manual&&!closed,nameReads,selectionChecks,manualBatches,manualCompletions,manualSuccessfulBatches,
      cachedLabels:cache.size,references:references.size,inFlightBatches:batches.size,activeOperations:operations,failures};
  }
  function close() {
    closed=true;
    for(const listener of [...hooks])try {listener.detach();hooks.splice(hooks.indexOf(listener),1);}catch(cause){errors.push(String(cause));}
    for(const reference of [...references])if(!batches.has(reference))free(reference);
    cache.clear();
    return {passed:hooks.length===0&&references.size===0&&operations===0&&errors.length===0,remainingHooks:hooks.length,
      remainingReferences:references.size,errors:[...errors],nameReads,selectionChecks,manualBatches,manualCompletions,manualSuccessfulBatches,failures};
  }
  return {snapshot,close,get busy(){return batches.size>0||operations>0;}};
}

module.exports={POKEMON,MULTI,LIST,CACHE_SIZE,MAX_SELECTION,formatIvs,install};
