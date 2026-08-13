// ===========================================================================
//  Panneau de parametres, construit depuis getParams() (JSON type).
// ===========================================================================
//  Reecriture JS du PropertyPanel de sinaia (cf. ANALYSE.md sec.6) : les
//  Parameter du moteur natif referencent des pointeurs crus vers les membres de
//  l'objet, ils ne traversent jamais la frontiere JS -- on lit du JSON et on
//  ecrit par nom via setParam.
// ===========================================================================

// Peuple `paramsEl` avec les widgets de l'objet `id`. `onChange` est appele
// apres chaque ecriture ; c'est au shell d'y planifier le re-rendu.
export function buildPanel(Module, paramsEl, id, onChange) {
  paramsEl.innerHTML = "";
  const params = JSON.parse(Module.getParams(id));

  // Les bornes d'un parametre peuvent dependre de la valeur d'un AUTRE : le
  // plafond de recursions du L-systeme depend du systeme choisi (2 pour Plant1,
  // 9 pour la courbe du dragon). Le panneau n'etant construit qu'une fois par
  // forme, il afficherait sinon les bornes du systeme selectionne a sa
  // construction. On relit donc getParams() apres chaque ecriture d'enum et on
  // remet les curseurs d'accord, en place -- pas de reconstruction, donc pas de
  // perte de focus sur la liste qu'on vient de manipuler.
  const refreshers = [];
  const refresh = () => {
    const cur = JSON.parse(Module.getParams(id));
    for (const r of refreshers) r(cur);
  };

  for (const p of params) {
    paramsEl.appendChild(buildParamWidget(Module, id, p, onChange, refreshers, refresh));
  }
}

function buildParamWidget(Module, id, p, onChange, refreshers, refresh) {
  const wrap = document.createElement("div");
  wrap.className = "param " + p.type;

  if (p.type === "bool") {
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = !!p.value;
    cb.id = "p_" + p.name;
    const row = document.createElement("div");
    row.className = "row";
    const lab = document.createElement("label");
    lab.className = "name";
    lab.htmlFor = cb.id;
    lab.textContent = p.name;
    row.append(cb, lab);
    wrap.appendChild(row);
    cb.addEventListener("change", () => {
      Module.setParam(id, p.name, cb.checked ? 1 : 0);
      refresh();   // meme raison que pour les listes : les bornes peuvent bouger
      onChange();
    });
    return wrap;
  }

  if (p.type === "enum") {
    const row = document.createElement("div");
    row.className = "row";
    const name = document.createElement("span");
    name.className = "name";
    name.textContent = p.name;
    row.appendChild(name);
    wrap.appendChild(row);
    const sel = document.createElement("select");
    p.choices.forEach((c, i) => {
      const o = document.createElement("option");
      o.value = i; o.textContent = c;
      if (i === p.value) o.selected = true;
      sel.appendChild(o);
    });
    sel.addEventListener("change", () => {
      Module.setParam(id, p.name, Number(sel.value));
      refresh();   // le choix peut avoir change les bornes d'un autre parametre
      onChange();
    });
    wrap.appendChild(sel);
    return wrap;
  }

  if (p.type === "string") {
    const row = document.createElement("div");
    row.className = "row";
    const name = document.createElement("span");
    name.className = "name";
    name.textContent = p.name;
    row.appendChild(name);
    wrap.appendChild(row);

    // Un parametre textuel multiligne (le texte a extruder) a besoin d'un
    // textarea : un <input> avalerait les retours a la ligne, qui ouvrent
    // justement une nouvelle ligne de texte.
    const field = document.createElement(p.multiline ? "textarea" : "input");
    if (p.multiline) field.rows = 3; else field.type = "text";
    field.value = p.value;
    field.spellcheck = false;

    // « input » et non « change » : la forme se reconstruit a la frappe, comme
    // un curseur se suit en direct. Le cout reste tenable parce que la police
    // est deja parsee -- seules la mise en page, l'aplatissement et la
    // tessellation sont rejoues.
    //
    // setParamString et NON setParam : une chaine n'a pas de valeur numerique,
    // et le pont refuse explicitement le second chemin (cf. wasm_api.cpp).
    field.addEventListener("input", () => {
      Module.setParamString(id, p.name, field.value);
      onChange();
    });
    wrap.appendChild(field);
    return wrap;
  }

  // int / float -> slider + valeur
  const isInt = p.type === "int";
  const row = document.createElement("div");
  row.className = "row";
  const name = document.createElement("span");
  name.className = "name";
  name.textContent = p.name;
  const val = document.createElement("span");
  val.className = "val";
  val.textContent = fmt(p.value, isInt);
  row.append(name, val);
  wrap.appendChild(row);

  const range = document.createElement("input");
  range.type = "range";
  range.min = p.min;
  range.max = p.max;
  range.step = isInt ? 1 : (p.max - p.min) / 200 || 0.001;
  range.value = p.value;
  range.addEventListener("input", () => {
    const v = Number(range.value);
    val.textContent = fmt(v, isInt);
    Module.setParam(id, p.name, v);
    onChange();
  });
  wrap.appendChild(range);

  // Remet ce curseur d'accord avec les bornes courantes. Une valeur retenue qui
  // depasse la nouvelle borne est REECRITE : Regenerate() la plafonne de son cote,
  // afficher 6 recursions sous une borne a 1 serait mensonger.
  refreshers.push((cur) => {
    const q = cur.find((x) => x.name === p.name);
    if (!q || q.type !== p.type) return;
    range.min = q.min;
    range.max = q.max;
    range.step = isInt ? 1 : (q.max - q.min) / 200 || 0.001;
    const v = Math.min(Math.max(q.value, q.min), q.max);
    if (v !== q.value) Module.setParam(id, p.name, v);
    range.value = v;
    val.textContent = fmt(v, isInt);
  });

  return wrap;
}

function fmt(v, isInt) {
  return isInt ? String(Math.round(v)) : (Math.round(v * 1000) / 1000).toString();
}
