import assert from 'node:assert/strict';
import { test } from 'node:test';
import { readFileSync } from 'node:fs';
import { createHash, webcrypto } from 'node:crypto';
import vm from 'node:vm';
import { giphyResults, klipyResults, allowedMediaURL, shareURL, chatSelection } from '../../../scripts/tasia_media_policy.mjs';

const root = new URL('../../../', import.meta.url);
const bundleDir = new URL('indra/newview/skins/default/html/en/tasia_media/', root);
const gif = { id: 'test_id', title: '<img src=x onerror=alert(1)>', url: 'https://giphy.com/gifs/test_id',
    images: { fixed_width: { url: 'https://media.giphy.com/media/test/giphy.gif', width: '200', height: '200' } } };
const response = data => ({ data, meta: { status: 200, response_id: 'fixture' },
    pagination: { count: data.length, offset: 0, total_count: data.length } });

test('URL policy rejects credentials, lookalike domains, mixed content and secret-bearing share URLs', () => {
    for (const url of ['http://giphy.com/x', 'javascript:alert(1)', 'https://giphy.com.attacker.test/x',
        'https://user:password@giphy.com/x', 'https://giphy.com:444/x'])
        assert.equal(allowedMediaURL(url, 'giphy'), '');
    for (const url of ['https://giphy.com/x?api_key=fixture', 'https://giphy.com/x#key', 'https://media.giphy.com/x'])
        assert.equal(shareURL(url, 'giphy'), '');
    assert.equal(shareURL('https://klipy.com/clips/test-clip', 'klipy'), 'https://klipy.com/clips/test-clip');
});

test('GIPHY SDK GIF and actual resolution-keyed Clip assets normalize without losing playable video', () => {
    assert.equal(giphyResults(response([gif]))[0].image, gif.images.fixed_width.url);
    const clip = { ...gif, video: { assets: { '360p': { url: 'https://media.giphy.com/test.mp4' } } } };
    assert.equal(giphyResults(response([clip]))[0].video, clip.video.assets['360p'].url);
    const result = giphyResults(response([clip]))[0];
    assert.equal(chatSelection(result, 'giphy'), clip.url + ' ' + clip.video.assets['360p'].url);
    assert.deepEqual(giphyResults(response([{ ...gif, url: 'https://attacker.test/x' }])), []);
});

test('current KLIPY GIF and Clip response schema; rejects old schema', () => {
    const media = file => ({ result: true, data: { data: [{ slug: 'test-clip', title: 'test', file }] } });
    const gifs = klipyResults(media({ sm: { gif: { url: 'https://static.klipy.com/test.gif' } } }), 'gifs');
    assert.equal(gifs[0].page, 'https://klipy.com/gifs/test-clip');
    const clips = klipyResults(media({ mp4: 'https://static.klipy.com/test.mp4' }), 'clips');
    assert.equal(clips[0].video, 'https://static.klipy.com/test.mp4');
    assert.throws(() => klipyResults({ data: { items: [] } }, 'clips'));
    assert.deepEqual(klipyResults(media({ mp4: 'https://attacker.test/file' }), 'clips'), []);
});

test('bundle and source hashes match reproducible SDK manifest', () => {
    const manifest = JSON.parse(readFileSync(new URL('sdk-manifest.json', bundleDir)));
    const hash = file => createHash('sha256').update(readFileSync(file)).digest('hex');
    assert.equal(hash(new URL('picker.bundle.js', bundleDir)), manifest.bundle_sha256);
    assert.equal(hash(new URL('preview.bundle.js', bundleDir)), manifest.preview_bundle_sha256);
    for (const [path, digest] of Object.entries(manifest.sources)) assert.equal(hash(new URL(path, root)), digest);
    assert.equal(manifest.packages['@giphy/js-fetch-api'].version, '5.8.0');
});

test('chat preview bundle plays only allowlisted public assets, with controls and no autoplay/API key', () => {
    const bundle = readFileSync(new URL('preview.bundle.js', bundleDir), 'utf8');
    for (const source of ['https://static.klipy.com/clip.mp4', 'https://attacker.test/x.mp4']) {
        const preview = new Element();
        vm.runInNewContext(bundle, { URL, URLSearchParams,
            location: { hash: '#provider=klipy&source=' + encodeURIComponent(source) },
            document: { getElementById: () => preview, createElement: () => new Element() } });
        if (source.includes('attacker')) assert.equal(preview.children.length, 0);
        else {
            assert.equal(preview.children[0].src, source);
            assert.equal(preview.children[0].autoplay, false);
            assert.equal(preview.children[0].controls, true);
            assert.equal(preview.children[0].preload, 'none');
        }
    }
});

class Element {
    constructor() { this.listeners = {}; this.children = []; this.value = ''; this.textContent = ''; }
    addEventListener(event, handler) { this.listeners[event] = handler; }
    append(...children) { this.children.push(...children); }
    add(option) { this.children.push(option); if (this.children.length === 1) this.value = option.value; }
    replaceChildren(...children) { this.children = children; }
}

function harness(provider, responder) {
    const elements = Object.fromEntries(['search','kind','query','searchButton','trending','status','attribution','results','more']
        .map(name => [name, new Element()]));
    const calls = [];
    const sandbox = { URL, URLSearchParams, AbortController, Response, Blob, setTimeout, clearTimeout,
        crypto: webcrypto, location: { hash: '', search: '' }, Option: class { constructor(text, value) { this.text = text; this.value = value; } },
        document: { getElementById: id => elements[id], createElement: () => new Element(), documentElement: {} },
        fetch: async (url, options) => {
            calls.push({ url: new URL(url), options });
            const result = await responder(new URL(url), calls.length);
            return new Response(JSON.stringify(result.body), { status: result.status || 200, headers: { 'content-type': 'application/json' } });
        },
    };
    sandbox.window = sandbox;
    vm.runInNewContext(readFileSync(new URL('picker.bundle.js', bundleDir), 'utf8'), sandbox);
    sandbox.tasiaMediaInit({ provider, key: 'fixture-key-not-a-secret', nonce: '11111111-1111-1111-1111-111111111111', language: 'pl' });
    return { sandbox, elements, calls };
}
const settle = () => new Promise(resolve => setTimeout(resolve, 30));

test('actual SDK bundle loads GIFs, inserts a nonce-bound link, never auto-sends', async () => {
    const { sandbox, elements, calls } = harness('giphy', () => ({ body: response([gif]) }));
    await settle();
    assert.equal(calls[0].url.pathname, '/v1/gifs/trending');
    assert.equal(calls[0].options.redirect, 'error');
    assert.equal(calls[0].options.cache, 'no-store');
    assert.equal(elements.results.children.length, 1);
    const card = elements.results.children[0];
    assert.equal(card.children[1].textContent, gif.title); // text, not injected HTML
    card.children[2].listeners.click();
    assert.equal(sandbox.location.hash, 'select=11111111-1111-1111-1111-111111111111:' + encodeURIComponent(gif.url));
});

test('GIPHY SDK extra types and search use the actual SDK endpoint methods', async () => {
    const { elements, calls } = harness('giphy', () => ({ body: response([gif]) }));
    await settle();
    for (const [kind, path] of [['videos', '/v1/videos/trending'], ['emoji', '/v1/emoji'], ['text', '/v1/text/animate'], ['stickers', '/v1/stickers/trending']]) {
        elements.kind.value = kind; elements.kind.listeners.change(); await settle();
        assert.equal(calls.at(-1).url.pathname, path);
    }
    elements.query.value = 'cute cat'; elements.search.listeners.submit({ preventDefault() {} }); await settle();
    assert.equal(calls.at(-1).url.pathname, '/v1/stickers/search');
    assert.equal(calls.at(-1).url.searchParams.get('q'), 'cute cat');
});

test('KLIPY uses current app-key path, safe pagination and selected content type', async () => {
    const body = { result: true, data: { has_next: true, data: [{ slug: 'test', file: { mp4: 'https://static.klipy.com/test.mp4' } }] } };
    const { elements, calls } = harness('klipy', () => ({ body }));
    await settle();
    assert.equal(calls[0].url.pathname, '/api/v1/fixture-key-not-a-secret/gifs/trending');
    assert.equal(calls[0].url.searchParams.get('per_page'), '24');
    elements.more.listeners.click(); await settle();
    assert.equal(calls[1].url.searchParams.get('page'), '2');
    elements.kind.value = 'clips'; elements.kind.listeners.change(); await settle();
    assert.equal(calls.at(-1).url.pathname, '/api/v1/fixture-key-not-a-secret/clips/trending');
    assert.equal(calls.at(-1).url.searchParams.get('page'), '1');
});

test('denied requests show fixed messages, not key or backend error/URL', async () => {
    const { elements } = harness('giphy', () => ({ status: 403, body: { message: 'fixture-key-not-a-secret private backend text' } }));
    await settle();
    assert.match(elements.status.textContent, /niedostępny/);
    assert.doesNotMatch(elements.status.textContent, /fixture-key|backend/);
    assert.equal(elements.results.children.length, 0);
});

test('old responses cannot append to a newly selected category', async () => {
    let resolveOld;
    const { elements } = harness('klipy', (url, count) => count === 1
        ? new Promise(resolve => { resolveOld = resolve; })
        : { body: { result: true, data: { data: [{ slug: 'new', file: { mp4: 'https://static.klipy.com/new.mp4' } }] } } });
    elements.kind.value = 'clips'; elements.kind.listeners.change(); await settle();
    resolveOld({ body: { result: true, data: { data: [{ slug: 'old', file: { gif: 'https://static.klipy.com/old.gif' } }] } } });
    await settle();
    assert.equal(elements.results.children.length, 1);
    assert.equal(elements.results.children[0].children[0].src, 'https://static.klipy.com/new.mp4');
});
