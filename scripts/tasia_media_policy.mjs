// Pure response normalization/URL policy shared by the picker and offline tests.
export function allowedMediaURL(value, provider) {
    try {
        const u = new URL(value);
        const domain = provider === 'giphy' ? 'giphy.com' : 'klipy.com';
        return u.protocol === 'https:' && !u.username && !u.password && (!u.port || u.port === '443') &&
            !['api.giphy.com', 'api.klipy.com'].includes(u.hostname) &&
            (u.hostname === domain || u.hostname.endsWith('.' + domain)) ? u.href : '';
    } catch { return ''; }
}

export function shareURL(value, provider) {
    const valid = allowedMediaURL(value, provider);
    if (!valid || valid.length > 2048) return '';
    const u = new URL(valid);
    if (u.search || u.hash || /\s/.test(value)) return '';
    return (provider === 'giphy' ? ['giphy.com', 'www.giphy.com'] : ['klipy.com']).includes(u.hostname) ? u.href : '';
}

// Share the ordinary provider page, plus a public media URL where the recipient
// otherwise needs an API key to resolve it. Other viewers still see normal links.
export function chatSelection(item, provider) {
    const asset = item.video || (provider === 'klipy' ? item.image : '');
    return item.page + (asset ? ' ' + asset : '');
}

function safeID(value) { return typeof value === 'string' && /^[A-Za-z0-9_-]{1,200}$/.test(value) ? value : ''; }

export function giphyResults(response) {
    if (!Array.isArray(response?.data)) throw new Error('Invalid response');
    return response.data.slice(0, 24).map(item => {
        const images = item.images || {};
        const video = item.video?.assets;
        const clip = video?.['360p']?.url || video?.['480p']?.url || video?.source?.url || '';
        return {
            title: String(item.title || 'GIPHY'),
            page: shareURL(item.url || `https://giphy.com/gifs/${safeID(item.id)}`, 'giphy'),
            image: allowedMediaURL(images.fixed_width?.url || images.preview_gif?.url || images.original?.url, 'giphy'),
            video: allowedMediaURL(clip, 'giphy'),
        };
    }).filter(item => item.page && (item.image || item.video));
}

export function klipyResults(response, kind) {
    if (response?.result !== true || !Array.isArray(response.data?.data)) throw new Error('Invalid response');
    return response.data.data.slice(0, 24).map(item => {
        const file = item.file || {};
        // GIFs use resolution -> format -> url; clips use format -> URL.
        const preview = file.sm || file.md || file.hd || file;
        const url = value => typeof value === 'string' ? value : value?.url || '';
        return {
            title: String(item.title || 'KLIPY'),
            page: shareURL(item.url || `https://klipy.com/${kind}/${safeID(item.slug)}`, 'klipy'),
            image: allowedMediaURL(url(preview.gif) || url(preview.webp) || url(preview.jpg), 'klipy'),
            video: kind === 'clips' || !url(preview.gif) && !url(preview.webp) && !url(preview.jpg)
                ? allowedMediaURL(url(preview.mp4) || url(file.mp4), 'klipy') : '',
        };
    }).filter(item => item.page && (item.image || item.video));
}
