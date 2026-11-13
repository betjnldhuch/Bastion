// Карта сайта: дома, бизнесы и территории банд из /api/map.json.
// Мир San Andreas: x и y от -3000 до 3000, y растёт на север.
(function () {
  var canvas = document.getElementById("map");
  if (!canvas) return;
  var ctx = canvas.getContext("2d");
  var info = document.getElementById("map-info");
  var count = document.getElementById("map-count");
  var sel = document.getElementById("map-server");
  var f = {
    houses: document.getElementById("f-houses"), biz: document.getElementById("f-biz"),
    turfs: document.getElementById("f-turfs"), free: document.getElementById("f-free")
  };
  var C = { house: "#3fbf6a", houseOwned: "#e0533f", biz: "#ff7a1a", bizOwned: "#a24a10", text: "#eeeef2" };
  var ZONES = [
    ["Los Santos", 1800, -1500], ["San Fierro", -2150, 350], ["Las Venturas", 1900, 1800],
    ["Red County", 900, -300], ["Flint County", -400, -1600], ["Whetstone", -2100, -2100],
    ["Tierra Robada", -1100, 1900], ["Bone County", 300, 1500]
  ];

  var data = { servers: [] }, srv = null, sel_item = null;
  var view = { scale: 1, x: 0, y: 0 }, size = 0, dpr = 1;
  var img = null, imgReady = false;

  if (canvas.dataset.image) {
    img = new Image();
    img.onload = function () { imgReady = true; draw(); };
    img.src = canvas.dataset.image;
  }

  function resize() {
    var r = canvas.parentNode.getBoundingClientRect();
    dpr = window.devicePixelRatio || 1;
    var w = Math.max(200, r.width), h = Math.max(300, Math.min(window.innerHeight - 240, w));
    canvas.style.height = h + "px";
    canvas.width = w * dpr; canvas.height = h * dpr;
    size = Math.min(w, h);
    if (!view.init) { view.x = (w - size) / 2; view.y = (h - size) / 2; view.init = true; }
    draw();
  }

  // мир -> экран (CSS-пиксели)
  function sx(x) { return view.x + (x + 3000) / 6000 * size * view.scale; }
  function sy(y) { return view.y + (3000 - y) / 6000 * size * view.scale; }

  function visible() {
    if (!srv) return { houses: [], biz: [], turfs: [] };
    var free = f.free.checked;
    return {
      houses: f.houses.checked ? srv.houses.filter(function (h) { return !free || !h.owner; }) : [],
      biz: f.biz.checked ? srv.businesses.filter(function (b) { return !free || !b.owner; }) : [],
      turfs: f.turfs.checked ? srv.turfs.filter(function (t) { return !free || !t.faction; }) : []
    };
  }

  function background() {
    var s = size * view.scale, x0 = sx(-3000), y0 = sy(3000);
    if (imgReady) { ctx.drawImage(img, x0, y0, s, s); return; }
    ctx.fillStyle = "#14202b"; ctx.fillRect(x0, y0, s, s);
    ctx.strokeStyle = "rgba(255,255,255,.06)"; ctx.lineWidth = 1;
    for (var g = -3000; g <= 3000; g += 500) {
      ctx.beginPath(); ctx.moveTo(sx(g), y0); ctx.lineTo(sx(g), y0 + s); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(x0, sy(g)); ctx.lineTo(x0 + s, sy(g)); ctx.stroke();
    }
    ctx.fillStyle = "rgba(238,238,242,.28)"; ctx.textAlign = "center";
    ctx.font = Math.max(11, 13 * Math.sqrt(view.scale)) + "px 'Russo One', sans-serif";
    ZONES.forEach(function (z) { ctx.fillText(z[0].toUpperCase(), sx(z[1]), sy(z[2])); });
  }

  function draw() {
    if (!size) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.fillStyle = "#0d0e11"; ctx.fillRect(0, 0, canvas.width, canvas.height);
    background();
    var v = visible();
    v.turfs.forEach(function (t) {
      var x = sx(t.box[0]), y = sy(t.box[3]), w = sx(t.box[2]) - x, h = sy(t.box[1]) - y;
      ctx.globalAlpha = .28; ctx.fillStyle = t.faction ? t.color : "#808491"; ctx.fillRect(x, y, w, h);
      ctx.globalAlpha = .9; ctx.strokeStyle = t.faction ? t.color : "#808491"; ctx.lineWidth = sel_item === t ? 3 : 1;
      ctx.strokeRect(x, y, w, h); ctx.globalAlpha = 1;
    });
    var r = Math.max(3, Math.min(9, 3 * Math.sqrt(view.scale)));
    v.houses.forEach(function (h) { dot(h, r, h.owner ? C.houseOwned : C.house, false); });
    v.biz.forEach(function (b) { dot(b, r + 1, b.owner ? C.bizOwned : C.biz, true); });
    if (count) {
      count.textContent = "Домов: " + v.houses.length + " · бизнесов: " + v.biz.length + " · территорий: " + v.turfs.length;
    }
  }

  function dot(o, r, color, square) {
    var x = sx(o.x), y = sy(o.y);
    ctx.fillStyle = color; ctx.strokeStyle = sel_item === o ? "#fff" : "rgba(0,0,0,.7)";
    ctx.lineWidth = sel_item === o ? 2.5 : 1;
    ctx.beginPath();
    if (square) ctx.rect(x - r, y - r, r * 2, r * 2); else ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fill(); ctx.stroke();
  }

  function money(n) { return "$" + String(n).replace(/\B(?=(\d{3})+(?!\d))/g, " "); }
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]; }); }
  function who(name) { return name ? "Владелец: <b>" + esc(name.replace(/_/g, " ")) + "</b>" : "<b class=\"free\">Свободен</b>"; }

  function show(o, kind) {
    sel_item = o;
    if (!o) { info.hidden = true; draw(); return; }
    var html;
    if (kind === "house") html = "<h4>Дом №" + o.id + "</h4><div>" + who(o.owner) + "</div><div>Цена: " + money(o.price) + "</div>";
    else if (kind === "biz") html = "<h4>" + esc(o.name) + "</h4><div class=\"muted\">" + esc(o.type) + " · №" + o.id + "</div><div>" + who(o.owner) + "</div><div>Цена: " + money(o.price) + "</div>";
    else html = "<h4>" + esc(o.name || "Территория №" + o.id) + "</h4><div>" + (o.faction ? "Контролирует: <b style=\"color:" + esc(o.color) + "\">" + esc(o.faction) + "</b>" : "<b class=\"free\">Ничья</b>") + "</div>";
    info.innerHTML = html + "<button type=\"button\" class=\"map-close\" aria-label=\"Закрыть\">×</button>";
    info.hidden = false;
    draw();
  }
  info.addEventListener("click", function (ev) { if (ev.target.closest(".map-close")) show(null); });

  function pick(px, py) {
    var v = visible(), best = null, kind = "", bd = 14 * 14;
    function test(list, k) {
      list.forEach(function (o) {
        var dx = sx(o.x) - px, dy = sy(o.y) - py, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = o; kind = k; }
      });
    }
    test(v.biz, "biz"); test(v.houses, "house");
    if (best) return show(best, kind);
    for (var i = v.turfs.length - 1; i >= 0; i--) {
      var t = v.turfs[i];
      if (px >= sx(t.box[0]) && px <= sx(t.box[2]) && py >= sy(t.box[3]) && py <= sy(t.box[1])) return show(t, "turf");
    }
    show(null);
  }

  function zoom(k, cx, cy) {
    var ns = Math.max(1, Math.min(12, view.scale * k));
    k = ns / view.scale;
    view.x = cx - (cx - view.x) * k; view.y = cy - (cy - view.y) * k; view.scale = ns;
    clamp(); draw();
  }
  function clamp() {
    var w = canvas.width / dpr, h = canvas.height / dpr, s = size * view.scale;
    // карта не уезжает за край: хотя бы треть окна остаётся на ней
    view.x = Math.min(Math.max(view.x, w / 3 - s), w * 2 / 3);
    view.y = Math.min(Math.max(view.y, h / 3 - s), h * 2 / 3);
  }
  function local(ev) { var r = canvas.getBoundingClientRect(); return [ev.clientX - r.left, ev.clientY - r.top]; }

  canvas.addEventListener("wheel", function (ev) {
    ev.preventDefault(); var p = local(ev); zoom(ev.deltaY < 0 ? 1.25 : 0.8, p[0], p[1]);
  }, { passive: false });

  var pointers = {}, drag = null, pinch = 0;
  canvas.addEventListener("pointerdown", function (ev) {
    canvas.setPointerCapture(ev.pointerId);
    pointers[ev.pointerId] = local(ev);
    drag = { p: local(ev), moved: false };
  });
  canvas.addEventListener("pointermove", function (ev) {
    if (!pointers[ev.pointerId]) return;
    var p = local(ev), ids = Object.keys(pointers);
    if (ids.length === 2) {
      pointers[ev.pointerId] = p;
      var a = pointers[ids[0]], b = pointers[ids[1]], d = Math.hypot(a[0] - b[0], a[1] - b[1]);
      if (pinch) zoom(d / pinch, (a[0] + b[0]) / 2, (a[1] + b[1]) / 2);
      pinch = d; if (drag) drag.moved = true; return;
    }
    var old = pointers[ev.pointerId]; pointers[ev.pointerId] = p;
    if (drag && (drag.moved || Math.abs(p[0] - drag.p[0]) + Math.abs(p[1] - drag.p[1]) > 5)) {
      drag.moved = true; view.x += p[0] - old[0]; view.y += p[1] - old[1]; clamp(); draw();
    }
  });
  function up(ev) {
    if (!pointers[ev.pointerId]) return;
    delete pointers[ev.pointerId]; pinch = 0;
    if (drag && !drag.moved && !Object.keys(pointers).length) { var p = local(ev); pick(p[0], p[1]); }
    if (!Object.keys(pointers).length) drag = null;
  }
  canvas.addEventListener("pointerup", up);
  canvas.addEventListener("pointercancel", up);
  document.getElementById("zoom-in").addEventListener("click", function () { zoom(1.5, canvas.width / dpr / 2, canvas.height / dpr / 2); });
  document.getElementById("zoom-out").addEventListener("click", function () { zoom(1 / 1.5, canvas.width / dpr / 2, canvas.height / dpr / 2); });

  Object.keys(f).forEach(function (k) { f[k].addEventListener("change", function () { show(null); }); });
  if (sel) sel.addEventListener("change", function () { pickServer(sel.value); });
  function pickServer(key) {
    srv = data.servers.filter(function (s) { return s.key === key; })[0] || data.servers[0] || null;
    show(null);
  }

  window.addEventListener("resize", resize);
  resize();
  fetch("/api/map.json").then(function (r) { return r.json(); }).then(function (d) {
    data = d; pickServer(sel ? sel.value : "");
  }).catch(function () { if (count) count.textContent = "Не удалось загрузить данные карты"; });
})();
