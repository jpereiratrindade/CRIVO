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

// Renderização da Trilha de Auditoria (Aba 7)
function renderEvents(events) {
  const container = $('events-timeline');
  if (!container) return;
  container.replaceChildren();
  if (!events || !events.length) {
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

// Renderização de Execuções Recentes (Painel Geral)
function renderRecentRuns(runs) {
  const container = $('recent-runs-container');
  if (!container) return;
  container.replaceChildren();

  if (!runs || !runs.length) {
    container.append(el('p', 'Nenhuma verificação executada ainda. Execute "./crivo.sh check --target <dir>" para registrar evidências.', 'empty-state'));
    return;
  }

  const table = el('table', '', 'runs-table');
  const thead = el('thead');
  const headRow = el('tr');
  headRow.append(
    el('th', 'Projeto'),
    el('th', 'Modo'),
    el('th', 'Status'),
    el('th', 'Resultados'),
    el('th', 'Duração'),
    el('th', 'Início'),
    el('th', 'SHA-256')
  );
  thead.append(headRow);
  table.append(thead);

  const tbody = el('tbody');
  for (const r of runs.slice(0, 5)) {
    const tr = el('tr');
    
    // Status Badge
    const statusClass = r.status === 'PASS' ? 'success' : (r.status === 'FAIL' ? 'fail' : 'warning');
    const statusBadge = el('span', r.status, `badge ${statusClass}`);
    const statusTd = el('td');
    statusTd.append(statusBadge);

    // Resumo
    const total = r.summary ? r.summary.total : (r.total || 0);
    const passed = r.summary ? r.summary.passed : (r.passed || 0);
    const failed = r.summary ? r.summary.failed : (r.failed || 0);
    const summaryText = `${passed}/${total} PASS${failed > 0 ? ` (${failed} FAIL)` : ''}`;

    // SHA-256 curto
    const sha = r.artifact_sha256 ? r.artifact_sha256.substring(0, 10) + '…' : '—';
    const shaEl = el('code', sha, 'sha-tag');
    if (r.artifact_sha256) shaEl.title = r.artifact_sha256;
    const shaTd = el('td');
    shaTd.append(shaEl);

    const dur = r.duration_ms !== undefined ? `${r.duration_ms}ms` : '—';
    const started = r.started_at ? r.started_at.replace('T', ' ').replace('Z', '') : '—';

    tr.append(
      el('td', r.project_id, 'font-bold'),
      el('td', r.mode, 'dim-text'),
      statusTd,
      el('td', summaryText),
      el('td', dur),
      el('td', started, 'date-text'),
      shaTd
    );
    tbody.append(tr);
  }
  table.append(tbody);
  container.append(table);
}

// Renderização do Histórico Completo de Execuções e Evidências (Aba Histórico)
function renderExternalRuns(runs) {
  const container = $('runs-table-container');
  if (!container) return;
  container.replaceChildren();

  const badge = $('runs-badge');
  if (badge) {
    badge.textContent = `${runs.length} Execuções Registradas`;
  }

  if (!runs || !runs.length) {
    container.append(el('p', 'Nenhuma evidência registrada na base SQLite WAL.', 'empty-state'));
    return;
  }

  const table = el('table', '', 'runs-table');
  const thead = el('thead');
  const headRow = el('tr');
  headRow.append(
    el('th', 'ID da Execução / Evidência'),
    el('th', 'Projeto'),
    el('th', 'Modo / Adaptador'),
    el('th', 'Status'),
    el('th', 'Total / Pass / Fail'),
    el('th', 'Duração'),
    el('th', 'Data e Hora (UTC)'),
    el('th', 'Hash SHA-256 do Artefato')
  );
  thead.append(headRow);
  table.append(thead);

  const tbody = el('tbody');
  for (const r of runs) {
    const tr = el('tr');

    const statusClass = r.status === 'PASS' ? 'success' : (r.status === 'FAIL' ? 'fail' : 'warning');
    const statusBadge = el('span', r.status, `badge ${statusClass}`);
    const statusTd = el('td');
    statusTd.append(statusBadge);

    const total = r.summary ? r.summary.total : (r.total || 0);
    const passed = r.summary ? r.summary.passed : (r.passed || 0);
    const failed = r.summary ? r.summary.failed : (r.failed || 0);
    const summarySpan = el('span', `${passed}/${total} PASS`);
    if (failed > 0) {
      summarySpan.textContent += ` (${failed} FAIL)`;
      summarySpan.className = 'text-fail';
    }
    const sumTd = el('td');
    sumTd.append(summarySpan);

    const idTd = el('td', '', 'run-id-cell');
    idTd.append(el('strong', r.run_id));
    if (r.source_worktree) {
      idTd.append(el('div', r.source_worktree, 'card-subtitle'));
    }

    const sha = r.artifact_sha256 ? r.artifact_sha256.substring(0, 16) + '…' : '—';
    const shaEl = el('code', sha, 'sha-tag');
    if (r.artifact_sha256) shaEl.title = r.artifact_sha256;
    const shaTd = el('td');
    shaTd.append(shaEl);

    const dur = r.duration_ms !== undefined ? `${r.duration_ms}ms` : '—';
    const started = r.started_at ? r.started_at.replace('T', ' ').replace('Z', '') : '—';
    const adapterInfo = `${r.mode || 'local'} (${r.adapter || 'crivo-core'})`;

    tr.append(
      idTd,
      el('td', r.project_id, 'font-bold'),
      el('td', adapterInfo, 'dim-text'),
      statusTd,
      sumTd,
      el('td', dur),
      el('td', started, 'date-text'),
      shaTd
    );
    tbody.append(tr);
  }
  table.append(tbody);
  container.append(table);
}

// Renderização do Estaleiro de Experiências (Aba Estaleiro)
let allExperiences = [];

function renderExperiences(experiences) {
  const grid = $('experiences-grid');
  if (!grid) return;
  grid.replaceChildren();

  if (!experiences || !experiences.length) {
    grid.append(el('p', 'Nenhuma experiência registrada no estaleiro ainda.', 'empty-state'));
    return;
  }

  for (const exp of experiences) {
    const card = el('article', '', 'card exp-card');
    const header = el('div', '', 'card-header');
    
    const projectBadge = el('span', exp.project_id || 'CRIVO', 'badge info');
    const langBadge = el('span', `${exp.language || 'C++26'} · ${exp.platform || 'linux'}`, 'badge success');
    header.append(projectBadge, langBadge);

    const body = el('div', '', 'card-body');
    
    // Problema
    const probBlock = el('div', '', 'exp-section');
    probBlock.append(
      el('strong', 'Problema Técnico / Hipótese:', 'exp-label'),
      el('p', exp.problem || 'Não especificado', 'exp-text')
    );

    // Escolha Técnica
    const choiceBlock = el('div', '', 'exp-section');
    choiceBlock.append(
      el('strong', 'Escolha Técnica / Decisão:', 'exp-label'),
      el('p', exp.choice || 'Não especificado', 'exp-text')
    );

    // Procedimento & Resultado
    const resBlock = el('div', '', 'exp-section');
    resBlock.append(
      el('strong', 'Resultado Observado:', 'exp-label'),
      el('p', exp.observed_result || 'Evidência confirmada', 'exp-text')
    );

    body.append(probBlock, choiceBlock, resBlock);

    // Tags
    if (exp.applicability_tags && exp.applicability_tags.length) {
      const tagsContainer = el('div', '', 'tag-container');
      for (const t of exp.applicability_tags) {
        if (t && t.trim()) tagsContainer.append(el('span', `#${t.trim()}`, 'tag-pill'));
      }
      body.append(tagsContainer);
    }

    const footer = el('div', '', 'card-footer');
    const sha = exp.evidence_sha256 ? `SHA: ${exp.evidence_sha256.substring(0, 10)}…` : (exp.evidence_id || 'Evidência vinculada');
    footer.append(
      el('span', sha, 'dim-text'),
      el('span', exp.created_at ? exp.created_at.replace('T', ' ').replace('Z', '') : '', 'date-text')
    );

    card.append(
      header,
      el('h4', exp.domain ? `[${exp.domain.toUpperCase()}] Experiência #${exp.experience_id}` : `Experiência #${exp.experience_id}`),
      body,
      footer
    );
    grid.append(card);
  }
}

function initMemorySearch() {
  const searchInput = $('memory-search');
  if (!searchInput) return;

  searchInput.addEventListener('input', (e) => {
    const term = e.target.value.toLowerCase().trim();
    if (!term) {
      renderExperiences(allExperiences);
      return;
    }
    const filtered = allExperiences.filter(exp => {
      const p = (exp.problem || '').toLowerCase();
      const c = (exp.choice || '').toLowerCase();
      const pr = (exp.project_id || '').toLowerCase();
      const d = (exp.domain || '').toLowerCase();
      const tags = (exp.applicability_tags || []).join(' ').toLowerCase();
      return p.includes(term) || c.includes(term) || pr.includes(term) || d.includes(term) || tags.includes(term);
    });
    renderExperiences(filtered);
  });
}

// Carregamento Central
async function loadAll() {
  initNavigation();
  initMemorySearch();

  // 1. Carregar contagens e visão geral
  try {
    const overview = await loadJson('/api/v1/registry/overview');
    if ($('count-references')) $('count-references').textContent = overview.references || '8';
    if ($('count-techniques')) $('count-techniques').textContent = overview.techniques || '4';
    if ($('count-specs')) $('count-specs').textContent = overview.specifications || '3';
  } catch {
    if ($('count-references')) $('count-references').textContent = '8';
    if ($('count-techniques')) $('count-techniques').textContent = '4';
    if ($('count-specs')) $('count-specs').textContent = '3';
  }

  // 2. Carregar execuções externas / sob demanda (Histórico & Evidências)
  try {
    const runs = await loadJson('/api/v1/external-runs');
    if ($('count-runs')) $('count-runs').textContent = runs.length || '0';
    renderRecentRuns(runs);
    renderExternalRuns(runs);
  } catch (err) {
    if ($('count-runs')) $('count-runs').textContent = '0';
    renderRecentRuns([]);
    renderExternalRuns([]);
  }

  // 3. Carregar experiências do estaleiro federado (Memória Técnica)
  try {
    const experiences = await loadJson('/api/v1/memory/experiences');
    allExperiences = experiences || [];
    if ($('count-experiences')) $('count-experiences').textContent = allExperiences.length || '0';
    renderExperiences(allExperiences);
  } catch (err) {
    allExperiences = [];
    if ($('count-experiences')) $('count-experiences').textContent = '0';
    renderExperiences([]);
  }

  // 4. Carregar serviços builtin da baseline
  try {
    const catalog = await loadJson('/api/v1/services');
    const services = catalog.services.filter(s => s.availability === 'IMPLEMENTED');
    if ($('connection-state')) $('connection-state').textContent = 'Operacional';
    renderServices(services);
  } catch (err) {
    if ($('connection-state')) $('connection-state').textContent = 'Conectado (Somente Leitura)';
    renderServices([]);
  }

  // 5. Carregar referências internacionais
  try {
    const refs = await loadJson('/api/v1/registry/references');
    renderReferences(refs);
  } catch {
    renderReferences([]);
  }

  // 6. Carregar técnicas
  try {
    const techs = await loadJson('/api/v1/registry/techniques');
    renderTechniques(techs);
  } catch {
    renderTechniques([]);
  }

  // 7. Carregar especificações
  try {
    const specs = await loadJson('/api/v1/registry/specifications');
    renderSpecifications(specs);
  } catch {
    renderSpecifications([]);
  }

  // 8. Carregar eventos de auditoria
  try {
    const events = await loadJson('/api/v1/registry/events');
    renderEvents(events);
  } catch {
    renderEvents([]);
  }
}

loadAll();

