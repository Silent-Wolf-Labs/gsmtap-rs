import { readFileSync } from 'node:fs';
import { jest } from '@jest/globals';
import { BitsInputModel } from '../../static/models/conversion/bits-input-model.js';
import { ConversionOutputModel } from '../../static/models/conversion/output-model.js';
import { createBitsResult } from '../../static/components/conversion/bits-result.js';
import { createConversionController } from '../../static/controllers/conversion-controller.js';
import { createConversionPanels } from '../../static/components/conversion/conversion-panel.js';

test('unpacked-bit format validates digits without rejecting raw nonbinary bytes', () => {
  const model = new BitsInputModel(); model.update('sourceEncoding','bits'); model.update('source','0 1\n1 0');
  expect(model.validate()).toEqual({});
  expect(model.toRequest().source).toEqual({encoding:'bits',data:'0 1\n1 0'});
  model.update('source','102');expect(model.validate()).toHaveProperty('source');
  model.update('operation','unpack');expect(model.validate()).toHaveProperty('sourceEncoding');
  model.update('sourceEncoding','hex');model.update('source','02 FF');expect(model.validate()).toEqual({});
});

test('zero-bit compatibility return is displayed without inventing output size', () => {
  const view=createBitsResult();const model=new ConversionOutputModel();
  model.setResult({operation:'pack-ext',success:true,returnValue:536870912,initialDestinationHex:'AA',finalDestinationHex:'AA',outputText:null});view.update(model);
  expect(view.node.textContent).toContain('Returned ending byte position: 536870912');
  expect(view.node.textContent).toContain('not the number of bytes written');
  expect(view.node.querySelectorAll('tbody tr')).toHaveLength(1);
  expect(view.node.querySelector('[aria-label="Complete destination (binary)"]').textContent).toContain('10101010');
});

test('unpacked output distinguishes written bits from preserved destination bytes', () => {
  const view=createBitsResult();const model=new ConversionOutputModel();
  model.setResult({operation:'unpack-ext',success:true,returnValue:3,initialDestinationHex:'AA AA AA AA',finalDestinationHex:'AA 00 01 AA',outputText:'0 1'});view.update(model);
  expect(view.node.querySelector('[aria-label="Unpacked output bits"]').textContent).toBe('Unpacked output bits: 0 1');
  expect(view.node.querySelectorAll('tbody tr')).toHaveLength(4);
  expect([...view.node.querySelectorAll('tbody tr')].at(-1).cells[3].textContent).toBe('No');
  expect(view.node.querySelector('[aria-label="Complete destination (binary)"]').hidden).toBe(true);
});

test('capacity errors leave a copyable buffer and reset clears the result', () => {
  const view=createBitsResult();const model=new ConversionOutputModel();
  model.setResult({operation:'pack',success:false,returnValue:null,initialDestinationHex:'AA',finalDestinationHex:'AA',error:{message:'Source too short'}});view.update(model);
  expect(view.node.querySelector('.error').textContent).toBe('Source too short');
  expect([...view.node.querySelectorAll('button')].find(button=>button.textContent==='Copy final buffer').disabled).toBe(false);
  model.reset();view.update(model);expect(view.node.textContent).toBe('No conversion result.');
});

test('Bits panel submits extended bit input and preserves options per operation', async () => {
  document.documentElement.innerHTML=readFileSync(new URL('../../static/index.html',import.meta.url),'utf8');
  const service={available:true,convert:jest.fn(async()=>({operation:'pack-ext',success:true,returnValue:1,initialDestinationHex:'AA',finalDestinationHex:'AB'}))};
  let views;const controller=createConversionController({service,onChange:(tool,state)=>views.get(tool).update(state)});
  views=createConversionPanels({controller});
  controller.update('bits','operation','pack-ext');controller.update('bits','sourceEncoding','bits');
  controller.update('bits','source','1');controller.update('bits','numBits','1');controller.update('bits','lsbMode',true);
  await controller.submit('bits');
  expect(service.convert).toHaveBeenCalledWith('bits',expect.objectContaining({source:{encoding:'bits',data:'1'},options:{numBits:1,inputOffset:0,outputOffset:0,lsbMode:true}}),expect.anything());
  const panel=document.querySelector('[data-workbench-view="bits"]');expect(panel.querySelector('.conversion-result').textContent).toContain('AB');
  controller.update('bits','operation','pack');expect(panel.querySelector('[name="lsbMode"]').closest('label').hidden).toBe(true);
});
