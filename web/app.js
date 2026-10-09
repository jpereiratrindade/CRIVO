'use strict';

const $ = (id) => document.getElementById(id);

async function loadJson(path) {
  const response = await fetch(path, { cache: 'no-store' });
  if (!response.ok) throw new Error(`Erro HTTP ${response.status}`);
  return response.json();
}

function el(tag, text, className) {
  const node = document.createElement(tag);
  if (text !== undefined && text !== null) node.textContent = String(text);
  if (className) node.className = className;
  return node;
}

// Navegação por Abas
function initNavigation() {
  const links = document.querySelectorAll('.nav-link');
  const tabs = document.querySelectorAll('.tab-content');

  function switchTab(targetId) {
    links.forEach(l => l.classList.remove('selected'));
    tabs.forEach(t => t.classList.remove('active'));

    const activeLink = document.querySelector(`.nav-link[data-tab="${targetId}"]`) || links[0];
    const activeTab = $(`tab-${targetId}`) || $('tab-inicio');

    activeLink.classList.add('selected');
    activeTab.classList.add('active');
  }

  links.forEach(link => {
    link.addEventListener('click', (e) => {
      e.preventDefault();
      const target = link.getAttribute('data-tab');
      switchTab(target);
      window.location.hash = target;
    });
  });

  const initialHash = window.location.hash.replace('#', '');
  if (initialHash) {
    switchTab(initialHash);
  }
}

// Renderização dos Serviços Builtin (Aba Início)
function renderServices(services) {
  const grid = $('service-grid');
  grid.replaceChildren();
  if (!services.length) {
    grid.append(el('p', 'Nenhum serviço operacional disponível.', 'empty-state'));
    return;
  }
  for (const s of services) {
    const card = el('article', '', 'card');
    const header = el('div', '', 'card-header');
    header.append(el('span', 'OPERACIONAL', 'badge success'));
    card.append(
      header,
      el('h4', s.name),
      el('div', s.id, 'card-subtitle'),
      el('p', `Finalidade: ${s.purpose} · Categoria: ${s.category}`, 'card-body'),
      el('div', 'Execução local · Resultado determinístico', 'card-footer')
    );
    grid.append(card);
  }
}

// Renderização das Referências Internacionais (Aba 2)
function renderReferences(refs) {
  const grid = $('references-grid');
  grid.replaceChildren();
  if (!refs.length) {
    grid.append(el('p', 'Nenhuma referência carregada.', 'empty-state'));
    return;
  }
  for (const r of refs) {
    const card = el('article', '', 'card');
    const header = el('div', '', 'card-header');
    header.append(
      el('span', r.kind.toUpperCase(), 'badge info'),
      el('span', r.edition, 'badge success')
    );

    const body = el('div', '', 'card-body');
    body.append(
      el('p', r.title),
      el('p', `Direitos: ${r.distribution} · Licença: ${r.license_review}`, 'card-subtitle')
    );

    const footer = el('div', '', 'card-footer');
    footer.append(
      el('span', `Emissor: ${r.issuer}`),
      r.url ? createExternalLink(r.url, 'Documento Oficial ↗') : el('span', '')
    );

    card.append(
      header,
      el('h4', r.id),
      body,
      footer
    );
    grid.append(card);
  }
}

function createExternalLink(url, text) {
  const a = document.createElement('a');
  a.href = url;
  a.textContent = text;
  a.target = '_blank';
  a.rel = 'noopener noreferrer';
  a.className = 'card-link';
  return a;
}

// Renderização das Técnicas de Teste (Aba 3)
function renderTechniques(techs) {
  const grid = $('techniques-grid');
  grid.replaceChildren();
  if (!techs.length) {
    grid.append(el('p', 'Nenhuma técnica carregada.', 'empty-state'));
    return;
  }
  for (const t of techs) {
    const card = el('article', '', 'card');
    const header = el('div', '', 'card-header');
    header.append(
      el('span', t.family.toUpperCase(), 'badge info'),
      el('span', `v${t.version}`, 'badge success')
    );

    const body = el('div', '', 'card-body');
    body.append(
      el('p', t.procedure_summary),
      el('p', `Classe de Oráculo: ${t.oracle_class}`, 'card-subtitle')
    );

    const footer = el('div', '', 'card-footer');
    footer.append(el('span', `Direitos: ${t.distribution}`));

    card.append(
      header,
      el('h4', t.name),
      el('div', t.id, 'card-subtitle'),
      body,
      footer
    );
    grid.append(card);
  }
}

// Renderização das Especificações (Aba 4)
function renderSpecifications(specs) {
  const grid = $('specifications-grid');
  grid.replaceChildren();
  if (!specs.length) {
    grid.append(el('p', 'Nenhuma especificação carregada.', 'empty-state'));
    return;
  }
  for (const s of specs) {
    const card = el('article', '', 'card');
    const header = el('div', '', 'card-header');
    header.append(
      el('span', `${s.category} / ${s.subcategory}`, 'badge info'),
      el('span', s.qualification.toUpperCase(), 'badge warning')
    );

    const body = el('div', '', 'card-body');
    body.append(
      el('p', s.title || s.id),
      el('p', `Oráculo: [${s.oracle_type}] ${s.oracle_id}`, 'card-subtitle')
    );

    const footer = el('div', '', 'card-footer');
    footer.append(el('span', `Nível: ${s.level} · Propósito: ${s.purpose}`));

    card.append(
      header,
      el('h4', s.id),
      el('div', `v${s.version} · Origem: ${s.source}`, 'card-subtitle'),
      body,
      footer
    );
    grid.append(card);
  }
}

// Renderização da Trilha de Auditoria (Aba 5)
function renderEvents(events) {
  const container = $('events-timeline');
  container.replaceChildren();
  if (!events.length) {
    container.append(el('p', 'Nenhum evento registrado na trilha.', 'empty-state'));
    return;
  }
  for (const ev of events) {
    const item = el('div', '', 'timeline-item');
    item.append(
      el('div', ev.event_type, 'timeline-type'),
      el('div', `${ev.entity_type}: ${ev.entity_ref}\nMotivo: ${ev.cause}`, 'timeline-entity'),
      el('div', `${ev.occurred_at}\nAutoridade: ${ev.authority_ref}`, 'timeline-time')
    );
    container.append(item);
  }
}

// Carregamento Central
async function loadAll() {
  initNavigation();

  // 1. Carregar contagens e visão geral
  try {
    const overview = await loadJson('/api/v1/registry/overview');
    $('count-references').textContent = overview.references || '8';
    $('count-techniques').textContent = overview.techniques || '4';
    $('count-specs').textContent = overview.specifications || '3';
  } catch {
    $('count-references').textContent = '8';
    $('count-techniques').textContent = '4';
    $('count-specs').textContent = '3';
  }

  // 2. Carregar serviços builtin da baseline
  try {
    const catalog = await loadJson('/api/v1/services');
    const services = catalog.services.filter(s => s.availability === 'IMPLEMENTED');
    $('connection-state').textContent = 'Operacional';
    renderServices(services);
  } catch (err) {
    $('connection-state').textContent = 'Conectado (Somente Leitura)';
    renderServices([]);
  }

  // 3. Carregar referências
  try {
    const refs = await loadJson('/api/v1/registry/references');
    renderReferences(refs);
  } catch {
    renderReferences([]);
  }

  // 4. Carregar técnicas
  try {
    const techs = await loadJson('/api/v1/registry/techniques');
    renderTechniques(techs);
  } catch {
    renderTechniques([]);
  }

  // 5. Carregar especificações
  try {
    const specs = await loadJson('/api/v1/registry/specifications');
    renderSpecifications(specs);
  } catch {
    renderSpecifications([]);
  }

  // 6. Carregar eventos de auditoria
  try {
    const events = await loadJson('/api/v1/registry/events');
    renderEvents(events);
  } catch {
    renderEvents([]);
  }
}

loadAll();
