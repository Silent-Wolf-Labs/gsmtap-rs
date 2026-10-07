import { readFileSync } from 'node:fs';
import { jest } from '@jest/globals';
import { Base64InputModel } from '../../static/models/conversion/base64-input-model.js';
import { ConversionOutputModel } from '../../static/models/conversion/output-model.js';
import { createBase64Result } from '../../static/components/conversion/base64-result.js';
import { createConversionController } from '../../static/controllers/conversion-controller.js';
import { createConversionPanels } from '../../static/components/conversion/conversion-panel.js';

const result = {operation:'encode',success:true,returnValue:0,outputLength:4,outputText:'Zm9v',outputHex:'5A 6D 39 76',initialDestinationHex:'AA AA AA AA AA AA',finalDestinationHex:'5A 6D 39 76 00 AA',sizeProbe:false};
function render(value) { const view=createBase64Result(); const output=new ConversionOutputModel();output.setResult(value);view.update(output);return {view,output}; }

test('encoded text excludes NUL while table and buffer copy include preserved suffix',()=>{
  const {view}=render(result);
  expect(view.node.querySelector('[aria-label="Output text"]').textContent).toBe('Base64 text: Zm9v');
  expect(view.node.querySelectorAll('tbody tr')).toHaveLength(6);
  expect([...view.node.querySelectorAll('tbody tr')].at(-1).cells[3].textContent).toBe('No');
  expect(view.node.textContent).toContain('buffer includes it');
  expect([...view.node.querySelectorAll('button')].find(button=>button.textContent==='Copy final buffer').disabled).toBe(false);
});

test('size probe shows required capacity without a destination table or converted output',()=>{
  const {view}=render({...result,operation:'decode',success:false,returnValue:-105,outputLength:2,sizeProbe:true,outputText:null,outputHex:null,initialDestinationHex:'',finalDestinationHex:'',error:{code:'bufferTooSmall'}});
  expect(view.node.querySelector('.success').textContent).toContain('Required destination capacity: 2 bytes');
  expect(view.node.querySelector('table').closest('.conversion-buffer').hidden).toBe(true);
  expect([...view.node.querySelectorAll('button')].find(button=>button.textContent==='Copy final buffer').parentElement.hidden).toBe(true);
});

test('invalid input keeps buffer available and hides stale successful output; reset clears result',()=>{
  const {view,output}=render(result);
  output.setResult({...result,success:false,returnValue:-22,outputLength:99,outputText:null,outputHex:null,error:{code:'invalidInput',message:'Invalid Base64 input.'}});view.update(output);
  expect(view.node.querySelector('.error').textContent).toContain('Invalid Base64 input.');
  expect(view.node.querySelector('[aria-label="Output text"]').hidden).toBe(true);
  expect(view.node.querySelectorAll('tbody tr')).toHaveLength(6);
  output.reset();view.update(output);expect(view.node.textContent).toBe('No conversion result.');
});

test('binary decode shows hex and empty decode displays preserved length separately',()=>{
  const {view,output}=render({...result,operation:'decode',outputText:null,outputHex:'FF',outputLength:1});
  expect(view.node.querySelector('[aria-label="Output text"]').hidden).toBe(true);
  expect(view.node.querySelector('[aria-label="Output bytes"]').textContent).toBe('Output bytes: FF');
  output.setResult({...result,operation:'decode',outputText:'',outputHex:'',outputLength:3737844653});view.update(output);
  expect(view.node.textContent).toContain('Output-length value: 3737844653');
  expect(view.node.textContent).toContain('preserves the initial length');
});

test('Base64 model accepts length sentinel, bounds it, and omits destination for decode probes',()=>{
 const model=new Base64InputModel();model.update('initialOutputLength',3737844653);expect(model.validate()).toEqual({});
 model.update('initialOutputLength',4294967296);expect(model.validate()).toHaveProperty('initialOutputLength');
 model.update('initialOutputLength',0);model.update('operation','decode');model.update('sizeProbe',true);model.destination.update('capacity','bad');
 expect(model.validate()).toEqual({});expect(model.toRequest().destination).toBeNull();
 model.update('operation','encode');expect(model.toRequest().options).not.toHaveProperty('sizeProbe');
});

test('Base64 panel submits probe options and hides destination fields',async()=>{
 document.documentElement.innerHTML=readFileSync(new URL('../../static/index.html',import.meta.url),'utf8');
 const service={available:true,convert:jest.fn(async()=>({...result,operation:'decode',sizeProbe:true}))};
 let views;const controller=createConversionController({service,onChange:(tool,state)=>views.get(tool).update(state)});views=createConversionPanels({controller});
 controller.update('base64','operation','decode');controller.update('base64','source','TWE=');controller.update('base64','sizeProbe',true);
 const panel=document.querySelector('[data-workbench-view="base64"]');
 expect([...panel.querySelectorAll('fieldset')].find(node=>node.querySelector('legend').textContent==='Destination buffer').hidden).toBe(true);
 await controller.submit('base64');expect(service.convert).toHaveBeenCalledWith('base64',expect.objectContaining({operation:'decode',destination:null,options:{sizeProbe:true,initialOutputLength:0}}),expect.anything());
});
