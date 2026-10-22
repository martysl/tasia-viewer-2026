import { GiphyFetch } from '@giphy/js-fetch-api';
import { giphyResults, klipyResults, chatSelection } from './tasia_media_policy.mjs';

const el = id => document.getElementById(id);
let config, sdk, version = 0, offset = 0, busy = false, hasMore = false;
let activeQuery = '';
let abort;
const words = {
    en: { search: 'Search', trending: 'Trending', more: 'More', use: 'Insert link',
        loading: 'Loading…', empty: 'No results.', ready: 'Choose a result. Links are inserted, not sent.',
        missing: 'Enter the provider key in the viewer field above (this session only).',
        failed: 'Request failed. Check connection, key, quota and content permissions.',
        denied: 'This content type is unavailable for this key (or its quota is exhausted).',
        text: 'Animated text', stickers: 'Stickers', clips: 'Clips', memes: 'Memes' },
    pl: { search: 'Szukaj', trending: 'Popularne', more: 'Więcej', use: 'Wstaw link',
        loading: 'Wczytywanie…', empty: 'Brak wyników.', ready: 'Wybierz wynik. Link zostanie wstawiony, nie wysłany.',
        missing: 'Wpisz klucz dostawcy w polu viewera powyżej (tylko na tę sesję).',
        failed: 'Błąd żądania. Sprawdź połączenie, klucz, limit i uprawnienia treści.',
        denied: 'Ten typ treści jest niedostępny dla klucza lub wyczerpano limit.',
        text: 'Animowany tekst', stickers: 'Naklejki', clips: 'Klipy', memes: 'Memy' },
};
let text = words.en;

// The official SDK must not bypass timeout/redirect/credential policy. Its errors
// contain authenticated URLs: always catch them and display only fixed messages.
const networkFetch = window.fetch.bind(window);
window.fetch = async (input, options = {}) => {
    const url = new URL(typeof input === 'string' ? input : input.url);
    if (url.protocol !== 'https:' || !['api.giphy.com', 'api.klipy.com'].includes(url.hostname))
        throw new Error('Blocked request');
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 20000);
    const cancel = () => controller.abort();
    const callerSignal = options.signal || abort?.signal;
    if (callerSignal?.aborted) cancel();
    callerSignal?.addEventListener('abort', cancel, { once: true });
    try {
        const result = await networkFetch(input, { ...options, signal: controller.signal,
            credentials: 'omit', redirect: 'error', cache: 'no-store', referrerPolicy: 'no-referrer' });
        if (Number(result.headers.get('content-length')) > 2 * 1024 * 1024)
            throw new Error('Response too large');
        const reader = result.body?.getReader();
        if (!reader) throw new Error('Missing response');
        const chunks = []; let length = 0;
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            length += value.byteLength;
            if (length > 2 * 1024 * 1024) { await reader.cancel(); throw new Error('Response too large'); }
            chunks.push(value);
        }
        return new Response(new Blob(chunks), { status: result.status, headers: result.headers });
    } finally { clearTimeout(timer); callerSignal?.removeEventListener('abort', cancel); }
};

function status(message) { el('status').textContent = message; }
function controls() {
    el('searchButton').disabled = busy;
    el('trending').disabled = busy;
    el('more').disabled = busy;
    el('more').hidden = !hasMore;
}

function render(items) {
    for (const item of items) {
        const card = document.createElement('article');
        const media = document.createElement(item.video ? 'video' : 'img');
        if (item.video) {
            media.controls = true; media.muted = true; media.preload = 'none';
            media.poster = item.image; media.src = item.video;
        } else { media.src = item.image; media.alt = item.title; media.loading = 'lazy'; }
        const title = document.createElement('p'); title.textContent = item.title;
        const button = document.createElement('button'); button.type = 'button'; button.textContent = text.use;
        button.addEventListener('click', () => {
            // No global app command handler and no automatic chat send. A nonce
            // binds the fragment event to this exact local picker instance.
            location.hash = `select=${config.nonce}:${encodeURIComponent(chatSelection(item, config.provider))}`;
        });
        card.append(media, title, button); el('results').append(card);
    }
}

async function load(reset = true) {
    if (!config?.key) { status(text.missing); return; }
    if (reset) {
        ++version; offset = 0; hasMore = false;
        abort?.abort(); el('results').replaceChildren();
    } else if (busy || !hasMore || offset >= 240) return;
    const generation = version;
    const kind = el('kind').value;
    abort = new AbortController();
    busy = true; controls(); status(text.loading);
    try {
        let response, items, next;
        if (config.provider === 'giphy') {
            const options = { limit: 24, offset, rating: 'pg-13' };
            response = kind === 'emoji' ? await sdk.emoji(options) : kind === 'text'
                ? await sdk.animate(activeQuery || 'Hello', options)
                : activeQuery ? await sdk.search(activeQuery, { ...options, type: kind })
                : await sdk.trending({ ...options, type: kind });
            items = giphyResults(response);
            next = response.data.length === 24 && offset + 24 < Math.min(240, response.pagination?.total_count ?? 240);
        } else {
            const url = new URL(`https://api.klipy.com/api/v1/${encodeURIComponent(config.key)}/${kind}/${activeQuery ? 'search' : 'trending'}`);
            url.search = new URLSearchParams({ per_page: '24', page: String(offset / 24 + 1),
                content_filter: 'medium', locale: config.language,
                ...(activeQuery ? { q: activeQuery } : {}) }).toString();
            const result = await window.fetch(url.href, { signal: abort.signal });
            if (!result.ok) throw { status: result.status };
            response = await result.json(); items = klipyResults(response, kind);
            next = response.data.has_next === true && offset + 24 < 240;
        }
        if (generation !== version) return;
        hasMore = next; render(items); offset += 24;
        status(items.length ? text.ready : text.empty);
    } catch (error) {
        if (generation !== version) return;
        hasMore = false;
        status([401, 403, 429].includes(error?.status) ? text.denied : text.failed);
    } finally { if (generation === version) { busy = false; controls(); } }
}

window.tasiaMediaInit = value => {
    if (config || !['giphy', 'klipy'].includes(value?.provider) || typeof value.key !== 'string' ||
        !/^[0-9a-f-]{36}$/.test(value.nonce)) return;
    config = value; text = words[value.language] || words.en;
    document.documentElement.lang = value.language === 'pl' ? 'pl' : 'en';
    el('searchButton').textContent = text.search; el('trending').textContent = text.trending;
    el('more').textContent = text.more;
    el('attribution').textContent = value.provider === 'giphy' ? 'Powered By GIPHY — Web SDK' : 'Powered By KLIPY';
    const kinds = value.provider === 'giphy'
        ? [['gifs', 'GIF'], ['stickers', text.stickers], ['emoji', 'Emoji'], ['videos', text.clips], ['text', text.text]]
        : [['gifs', 'GIF'], ['clips', text.clips], ['stickers', text.stickers], ['memes', text.memes]];
    for (const [key, label] of kinds) el('kind').add(new Option(label, key));
    sdk = value.provider === 'giphy' && value.key ? new GiphyFetch(value.key) : null;
    el('search').addEventListener('submit', event => { event.preventDefault(); activeQuery = el('query').value.trim(); load(); });
    el('trending').addEventListener('click', () => { el('query').value = ''; activeQuery = ''; load(); });
    el('kind').addEventListener('change', () => load());
    el('more').addEventListener('click', () => load(false));
    load();
};
