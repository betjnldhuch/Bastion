// Bastion Online — раздача файлов игры из закрытого бакета Cloudflare R2.
//
// Файл отдаётся только запросу с заголовком "X-Bastion-Key", равным секрету
// DOWNLOAD_KEY этого Worker. Без ключа — 404, как будто файла нет.
// Привязки (Settings → Bindings / Variables):
//   GAME         — R2 bucket binding на бакет с game.zip
//   DOWNLOAD_KEY — Secret, тот же ключ, что вшит в лаунчеры
// Поддерживаются GET, HEAD и Range (докачка), отдаются только файлы из ALLOWED.

const ALLOWED = /^[A-Za-z0-9._-]+\.zip$/;

function sameKey(a, b) {
  if (typeof a !== "string" || typeof b !== "string" || a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

export default {
  async fetch(request, env) {
    if (request.method !== "GET" && request.method !== "HEAD") {
      return new Response("Method not allowed", { status: 405, headers: { Allow: "GET, HEAD" } });
    }
    const name = decodeURIComponent(new URL(request.url).pathname.slice(1));
    if (!ALLOWED.test(name) || !env.DOWNLOAD_KEY || !sameKey(request.headers.get("X-Bastion-Key"), env.DOWNLOAD_KEY)) {
      return new Response("Not found", { status: 404 });
    }

    const object = request.method === "HEAD"
      ? await env.GAME.head(name)
      : await env.GAME.get(name, { range: request.headers, onlyIf: request.headers });
    if (object === null) return new Response("Not found", { status: 404 });

    const headers = new Headers();
    object.writeHttpMetadata(headers);
    headers.set("ETag", object.httpEtag);
    headers.set("Accept-Ranges", "bytes");
    headers.set("Cache-Control", "private, no-store");
    headers.set("Content-Disposition", `attachment; filename="${name}"`);

    if (request.method === "HEAD") {
      headers.set("Content-Length", String(object.size));
      return new Response(null, { headers });
    }
    if (!("body" in object)) return new Response(null, { status: 412, headers }); // If-Match не совпал

    if (object.range && request.headers.has("Range")) {
      const start = object.range.offset ?? (object.size - object.range.suffix);
      const length = object.range.length ?? (object.size - start);
      headers.set("Content-Range", `bytes ${start}-${start + length - 1}/${object.size}`);
      headers.set("Content-Length", String(length));
      return new Response(object.body, { status: 206, headers });
    }
    headers.set("Content-Length", String(object.size));
    return new Response(object.body, { headers });
  },
};
