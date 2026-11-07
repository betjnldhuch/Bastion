"""BB-коды форума: [b] [i] [u] [s] [quote] [code] [spoiler] [url] [img] [color] [center] [list]."""

import html
import re

MAX_NESTING = 5

_SIMPLE = {
    "b": ("<strong>", "</strong>"),
    "i": ("<em>", "</em>"),
    "u": ("<u>", "</u>"),
    "s": ("<s>", "</s>"),
    "center": ('<div class="bb-center">', "</div>"),
}

_URL_RE = re.compile(r"^https?://[^\s\"'<>]+$", re.I)
_COLOR_RE = re.compile(r"^(#[0-9a-f]{3}|#[0-9a-f]{6}|[a-z]{3,20})$", re.I)
_AUTOLINK_RE = re.compile(r"(?<![\"'=\]>])\b(https?://[^\s<\[\]\"']+)", re.I)


def _safe_url(url):
    url = html.unescape(url).strip()
    if _URL_RE.match(url):
        return html.escape(url, quote=True)
    return None


def _render_code(text):
    # Внутри [code] ничего не разбираем
    parts = re.split(r"\[code\](.*?)\[/code\]", text, flags=re.S | re.I)
    out = []
    for i, part in enumerate(parts):
        if i % 2:
            out.append(("code", part))
        else:
            out.append(("text", part))
    return out


def _inline(text):
    for _ in range(MAX_NESTING):
        before = text
        for tag, (open_, close) in _SIMPLE.items():
            text = re.sub(rf"\[{tag}\](.*?)\[/{tag}\]", rf"{open_}\1{close}", text, flags=re.S | re.I)

        def color(m):
            c = html.unescape(m.group(1))
            return f'<span style="color:{c}">{m.group(2)}</span>' if _COLOR_RE.match(c) else m.group(2)
        text = re.sub(r"\[color=([^\]]{1,20})\](.*?)\[/color\]", color, text, flags=re.S | re.I)

        def url_named(m):
            u = _safe_url(m.group(1))
            return f'<a href="{u}" rel="nofollow noopener" target="_blank">{m.group(2)}</a>' if u else m.group(2)
        text = re.sub(r"\[url=([^\]]+)\](.*?)\[/url\]", url_named, text, flags=re.S | re.I)

        def url_plain(m):
            u = _safe_url(m.group(1))
            return f'<a href="{u}" rel="nofollow noopener" target="_blank">{u}</a>' if u else m.group(1)
        text = re.sub(r"\[url\](.*?)\[/url\]", url_plain, text, flags=re.S | re.I)

        def img(m):
            u = _safe_url(m.group(1))
            return f'<img src="{u}" alt="" loading="lazy" referrerpolicy="no-referrer">' if u else m.group(1)
        text = re.sub(r"\[img\](.*?)\[/img\]", img, text, flags=re.S | re.I)

        def quote(m):
            who = m.group(1)
            head = f'<div class="bb-quote-head">{who} пишет:</div>' if who else ""
            return f'<blockquote class="bb-quote">{head}{m.group(2).strip()}</blockquote>'
        text = re.sub(r"\[quote(?:=([^\]]{1,40}))?\](.*?)\[/quote\]", quote, text, flags=re.S | re.I)

        def spoiler(m):
            title = m.group(1) or "Спойлер"
            return f'<details class="bb-spoiler"><summary>{title}</summary>{m.group(2).strip()}</details>'
        text = re.sub(r"\[spoiler(?:=([^\]]{1,60}))?\](.*?)\[/spoiler\]", spoiler, text, flags=re.S | re.I)

        def list_(m):
            items = [i.strip() for i in re.split(r"\[\*\]", m.group(1)) if i.strip()]
            return "<ul>" + "".join(f"<li>{i}</li>" for i in items) + "</ul>"
        text = re.sub(r"\[list\](.*?)\[/list\]", list_, text, flags=re.S | re.I)
        if text == before:
            break
    return text


def _autolink(text):
    # Ссылки, которые не стоят внутри атрибутов и тегов
    out = []
    for piece in re.split(r"(<[^>]+>)", text):
        if piece.startswith("<"):
            out.append(piece)
        else:
            out.append(_AUTOLINK_RE.sub(
                lambda m: f'<a href="{m.group(1)}" rel="nofollow noopener" target="_blank">{m.group(1)}</a>', piece))
    return "".join(out)


def render(text):
    """Текст сообщения -> безопасный HTML."""
    text = (text or "").replace("\r\n", "\n").replace("\r", "\n")
    html_parts = []
    for kind, part in _render_code(text):
        escaped = html.escape(part, quote=True)
        if kind == "code":
            html_parts.append(f"<pre class=\"bb-code\"><code>{escaped.strip(chr(10))}</code></pre>")
        else:
            rendered = _inline(escaped)
            rendered = _autolink_outside_anchors(rendered)
            rendered = rendered.replace("\n", "<br>")
            rendered = re.sub(r"(<br>)?(</?(?:blockquote|div|ul|li|details|summary)[^>]*>)(<br>)?", r"\2", rendered)
            html_parts.append(rendered)
    return "".join(html_parts)


def _autolink_outside_anchors(text):
    # Не трогаем то, что уже внутри <a>...</a>
    pieces = re.split(r"(<a [^>]*>.*?</a>)", text, flags=re.S)
    return "".join(p if p.startswith("<a ") else _autolink(p) for p in pieces)


def plain(text, limit=300):
    """Текст без BB-кодов (для news.txt и превью)."""
    text = re.sub(r"\[/?[a-z*]+(?:=[^\]]*)?\]", "", text or "", flags=re.I)
    text = re.sub(r"\s+", " ", text).strip()
    return text if len(text) <= limit else text[:limit - 1].rstrip() + "…"
