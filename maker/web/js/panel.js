// ===========================================================================
//  Panneau de parametres, construit depuis getParams() (JSON type).
// ===========================================================================
//  Reecriture JS du PropertyPanel de sinaia (cf. ANALYSE.md sec.6) : les
//  Parameter de cgmesh referencent des pointeurs crus vers les membres de
//  l'objet, ils ne traversent jamais la frontiere JS -- on lit du JSON et on
//  ecrit par nom via setParam.
// ===========================================================================

// Peuple `paramsEl` avec les widgets de l'objet `id`. `onChange` est appele
// apres chaque ecriture ; c'est au shell d'y planifier le re-rendu.
export function buildPanel(Module, paramsEl, id, onChange) {
  paramsEl.innerHTML = "";
  const params = JSON.parse(Module.getParams(id));
  for (const p of params) {
    paramsEl.appendChild(buildParamWidget(Module, id, p, onChange));
  }
}

function buildParamWidget(Module, id, p, onChange) {
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
      onChange();
    });
    wrap.appendChild(sel);
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
  return wrap;
}

function fmt(v, isInt) {
  return isInt ? String(Math.round(v)) : (Math.round(v * 1000) / 1000).toString();
}
