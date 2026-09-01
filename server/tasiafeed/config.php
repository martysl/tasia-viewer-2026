<?php
declare(strict_types=1);

// TasiaFeed Configuration
// Upload this directory to: https://apps.easierit.org/igrid/feed/

// --- Paths ---
define('STORAGE_DIR', __DIR__ . '/uploads');
define('THUMBS_DIR', __DIR__ . '/thumbs');
define('DB_PATH', __DIR__ . '/feed.sqlite');

// --- URLs (no trailing slashes) ---
define('BASE_URL', 'https://apps.easierit.org/igrid/feed');
define('UPLOADS_URL', BASE_URL . '/uploads');
define('THUMBS_URL', BASE_URL . '/thumbs');

// --- Limits ---
define('MAX_FILE_SIZE', 20 * 1024 * 1024);       // 20 MB
define('ALLOWED_MIME_TYPES', ['image/jpeg', 'image/png', 'image/webp']);
define('ALLOWED_EXTENSIONS', ['jpg', 'jpeg', 'png', 'webp']);
define('THUMB_WIDTH', 280);
define('THUMB_HEIGHT', 280);
define('ITEMS_PER_PAGE', 30);

// --- Auth ---
// Simple shared secret for admin access. Change this in production.
define('ADMIN_SECRET', 'tasia123');

// --- Post visibility ---
define('VISIBILITY_PUBLIC', 'public');
define('VISIBILITY_UNLISTED', 'unlisted');
