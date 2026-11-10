// Bastion Online — раздача файлов игры из закрытого бакета Cloudflare R2.
//
// Файл отдаётся только запросу с заголовком "X-Bastion-Key", равным секрету
// DOWNLOAD_KEY этого Worker. Без ключа — 404, как будто файла нет.
// Привязки (Settings → Bindings / Variables):
//   GAME         — R2 bucket binding на бакет с game.zip
//   DOWNLOAD_KEY — Secret, тот же ключ, что вшит в лаунчеры
// Поддерживаются GET, HEAD и Range (докачка), отдаются только файлы из ALLOWED.
//
// Копии базы сервера: PUT /backups/rp_backup_ГГГГ-ММ-ДД.db с заголовком
// "X-Bastion-Backup-Key", равным секрету BACKUP_KEY. Копии кладутся в папку
// backups/ того же бакета (или бакета BACKUPS, если такая привязка есть),
// хранится BACKUP_KEEP последних. Скачать копию через Worker НЕЛЬЗЯ ни с каким
// ключом (в базе хеши паролей игроков): её забирают в панели Cloudflare.
// BACKUP_KEY не вшивается в лаунчеры, он есть только на ПК с сервером.

const ALLOWED = /^[A-Za-z0-9._-]+\.zip$/;
const BACKUP_NAME = /^backups\/rp_backup_\d{4}-\d{2}-\d{2}\.db$/;
const BACKUP_KEEP = 30;
const BACKUP_MAX_BYTES = 95 * 1024 * 1024;

function sameKey(a, b) {
  if (typeof a !== "string" || typeof b !== "string" || a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

// Принять копию базы и оставить BACKUP_KEEP последних
async function putBackup(request, env) {
  const name = decodeURIComponent(new URL(request.url).pathname.slice(1));
  if (!BACKUP_NAME.test(name) || !env.BACKUP_KEY || !sameKey(request.headers.get("X-Bastion-Backup-Key"), env.BACKUP_KEY)) {
    return new Response("Not found", { status: 404 });
  }
  const size = Number(request.headers.get("Content-Length") || "0");
  if (!size || size > BACKUP_MAX_BYTES) return new Response("Bad size", { status: 413 });

  const bucket = env.BACKUPS || env.GAME;
  await bucket.put(name, request.body);

  // Имена содержат дату, поэтому сортировка по имени = сортировка по времени
  const listed = await bucket.list({ prefix: "backups/" });
  const old = listed.objects.map((o) => o.key).filter((k) => BACKUP_NAME.test(k)).sort().reverse().slice(BACKUP_KEEP);
  if (old.length > 0) await bucket.delete(old);

  return new Response("Saved " + name + "\n", { status: 201 });
}

export default {
  async fetch(request, env) {
    if (request.method === "PUT") return putBackup(request, env);
    if (request.method !== "GET" && request.method !== "HEAD") {
      return new Response("Method not allowed", { status: 405, headers: { Allow: "GET, HEAD, PUT" } });
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
