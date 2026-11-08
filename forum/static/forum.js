// Мелочи форума: кнопки BB-кодов, цитирование, подтверждение удаления.
(function () {
  function wrap(area, open, close) {
    var s = area.selectionStart, e = area.selectionEnd, v = area.value;
    area.value = v.slice(0, s) + open + v.slice(s, e) + close + v.slice(e);
    area.focus();
    area.selectionStart = s + open.length;
    area.selectionEnd = e + open.length;
  }

  document.querySelectorAll(".bb-bar").forEach(function (bar) {
    var area = document.getElementById(bar.dataset.for);
    bar.addEventListener("click", function (ev) {
      var btn = ev.target.closest("button[data-tag]");
      if (!btn || !area) return;
      var tag = btn.dataset.tag;
      if (tag === "url") {
        var url = prompt("Ссылка (https://...)");
        if (!url) return;
        wrap(area, "[url=" + url + "]", "[/url]");
      } else {
        wrap(area, "[" + tag + "]", "[/" + tag + "]");
      }
    });
  });

  document.querySelectorAll(".quote-btn").forEach(function (btn) {
    btn.addEventListener("click", function () {
      var area = document.getElementById("body");
      var src = document.getElementById(btn.dataset.src);
      if (!area || !src) return;
      var text = src.value.replace(/\[quote[^\]]*\][\s\S]*?\[\/quote\]/gi, "").trim();
      area.value += (area.value ? "\n" : "") + "[quote=" + btn.dataset.name.replace(/_/g, " ") + "]" + text + "[/quote]\n";
      area.focus();
      area.scrollIntoView({ block: "center" });
    });
  });

  document.addEventListener("submit", function (ev) {
    var form = ev.target;
    var btn = ev.submitter;
    var msg = (btn && btn.dataset.confirm) || form.dataset.confirm;
    if (msg && !confirm(msg)) ev.preventDefault();
  });
})();
