'use strict';
const $ = (id) => document.getElementById(id);

async function loadJson(path) {
  const response = await fetch(path,{cache:'no-store'});
  if (!response.ok) throw new Error(`Erro HTTP ${response.status}`);
  return response.json();
}

function element(tag,text,className) {
  const node=document.createElement(tag);
  node.textContent=String(text);
  if(className) node.className=className;
  return node;
}

const purposeLabels={contract:'Contrato',diagnostic:'Diagnóstico',conformance:'Verificação'};

function renderServices(services) {
  const grid=$('service-grid');
  grid.replaceChildren();
  if(!services.length) {
    grid.append(element('p','Nenhum serviço disponível neste ambiente.','empty-state'));
    return;
  }
  for(const service of services) {
    const card=element('article','', 'service-card');
    card.append(
      element('span','Disponível','availability'),
      element('h4',service.name),
      element('p',purposeLabels[service.purpose] || 'Verificação técnica','service-purpose'),
      element('p','Execução local · resultado objetivo','service-note')
    );
    grid.append(card);
  }
}

async function loadDashboard() {
  try {
    const catalog=await loadJson('/api/v1/services');
    const services=catalog.services.filter((service)=>service.availability==='IMPLEMENTED');
    $('service-count').textContent=services.length;
    $('connection-state').textContent='Operacional';
    renderServices(services);
  } catch(error) {
    $('connection-state').textContent='Indisponível';
    $('service-count').textContent='0';
    renderServices([]);
  }
}

loadDashboard();
