'use strict';
const $ = (id) => document.getElementById(id);
const state = { tests: [] };
async function loadJson(path) {
  const response = await fetch(path,{cache:'no-store'});
  if (!response.ok) throw new Error(`Erro HTTP ${response.status}`);
  return response.json();
}
function element(tag, text, className) {
  const node=document.createElement(tag);
  node.textContent=String(text);
  if(className) node.className=className;
  return node;
}
function cell(row, value, className='') {
  const td=document.createElement('td');
  td.append(element('span',value,className)); row.append(td);
}
function renderTests() {
  const tbody=$('tests-body');tbody.replaceChildren();
  const filter=$('filter').value;
  const tests=state.tests.filter((t)=>!filter||t.category===filter);
  if(!tests.length) {const row=document.createElement('tr');const td=document.createElement('td');td.colSpan=5;td.textContent='Nenhum teste para este filtro';row.append(td);tbody.append(row);return;}
  for(const t of tests){const row=document.createElement('tr');cell(row,t.name);cell(row,`${t.category} / ${t.subcategory}`);cell(row,t.purpose);cell(row,t.profiles.join(', '));cell(row,t.status,`state ${t.status}`);tbody.append(row);}
}
function renderRuns(runs){const tbody=$('runs-body');tbody.replaceChildren();if(!runs.length){const row=document.createElement('tr');const td=document.createElement('td');td.colSpan=5;td.textContent='Nenhuma execução registrada. Execute: ./build/crivo run --profile core';row.append(td);tbody.append(row);return;}
  for(const r of runs){const row=document.createElement('tr');cell(row,r.created_at);cell(row,r.test_id);cell(row,r.profile);cell(row,r.status,`state ${r.status.toLowerCase()}`);cell(row,r.detail);tbody.append(row);}
}
async function loadDashboard(){try{
  const [overview,categories,tests,runs]=await Promise.all([
    loadJson('/api/v1/overview'),loadJson('/api/v1/categories'),loadJson('/api/v1/tests'),loadJson('/api/v1/runs')]);
  $('count-tests').textContent=overview.tests;
  $('count-ready').textContent=overview.implemented;
  $('count-passed').textContent=overview.passed;
  $('count-issues').textContent=overview.failed+overview.blocked;
  $('connection-state').textContent='Dados locais do executor';
  const grid=$('category-grid');grid.replaceChildren();
  const select=$('filter');
  for(const c of categories){
    const card=element('article','','category-card');
    card.append(element('h4',c.category),element('p',`${c.tests} catalogados · ${c.implemented} implementados`));grid.append(card);
    const option=document.createElement('option');option.value=c.category;option.textContent=c.category;select.append(option);
  }
  state.tests=tests;renderTests();renderRuns(runs);
}catch(error){$('connection-state').textContent=`Falha: ${error.message}`;}}
$('filter').addEventListener('change',renderTests);
loadDashboard();
