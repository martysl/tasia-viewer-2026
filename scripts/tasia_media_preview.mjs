import { allowedMediaURL } from './tasia_media_policy.mjs';
const params = new URLSearchParams(location.hash.slice(1));
const provider = params.get('provider');
const source = params.get('source');
if (['giphy', 'klipy'].includes(provider) && source && source.length <= 2048) {
    const url = allowedMediaURL(source, provider);
    if (url) {
        const video = /\.(mp4|webm)$/i.test(new URL(url).pathname);
        const media = document.createElement(video ? 'video' : 'img');
        media.src = url;
        if (video) { media.controls = true; media.preload = 'none'; media.autoplay = false; }
        else media.alt = provider.toUpperCase();
        media.addEventListener('error', () => { document.getElementById('status').textContent = 'Media unavailable — open the provider link.'; });
        document.getElementById('preview').append(media);
    }
}
