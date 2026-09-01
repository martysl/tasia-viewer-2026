<?php
declare(strict_types=1);

require_once __DIR__ . '/../../../config.php';
require_once __DIR__ . '/../../../db.php';

// ---------- CORS ----------
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(['success' => false, 'message' => 'POST required'], 405);
}

// ---------- Parse JSON body ----------
$body = json_decode((string)file_get_contents('php://input'), true);
if (!is_array($body)) {
    jsonResponse(['success' => false, 'message' => 'Invalid JSON body'], 400);
}

// ---------- Validate image ----------
$image_base64 = $body['image'] ?? '';
if (empty($image_base64)) {
    jsonResponse(['success' => false, 'message' => 'Missing image data'], 400);
}

$image_data = base64_decode($image_base64, true);
if ($image_data === false || $image_data === '') {
    jsonResponse(['success' => false, 'message' => 'Invalid base64 image data'], 400);
}

if (strlen($image_data) > MAX_FILE_SIZE) {
    jsonResponse(['success' => false, 'message' => 'Image too large (max ' . (MAX_FILE_SIZE / 1024 / 1024) . ' MB)'], 413);
}

// Detect MIME type (fallback if finfo extension is not available)
$mime = '';
if (class_exists('finfo')) {
    $finfo = new finfo(FILEINFO_MIME_TYPE);
    $mime = $finfo->buffer($image_data);
} else {
    // Magic byte detection
    if (strlen($image_data) >= 4) {
        $header = substr($image_data, 0, 4);
        if ($header === "\xff\xd8\xff\xe0" || $header === "\xff\xd8\xff\xe1") {
            $mime = 'image/jpeg';
        } elseif ($header === "\x89PNG") {
            $mime = 'image/png';
        } elseif ($header === "RIFF" && substr($image_data, 8, 4) === "WEBP") {
            $mime = 'image/webp';
        }
    }
}
if (!in_array($mime, ALLOWED_MIME_TYPES, true)) {
    jsonResponse(['success' => false, 'message' => 'Unsupported image type. Allowed: JPEG, PNG, WebP'], 415);
}

switch ($mime) {
    case 'image/jpeg': $ext = 'jpg'; break;
    case 'image/png':  $ext = 'png'; break;
    case 'image/webp': $ext = 'webp'; break;
    default:           $ext = 'jpg'; break;
}

// ---------- Create storage directories ----------
$date_dir = gmdate('Y/m/d');
$full_dir = STORAGE_DIR . '/' . $date_dir;
$thumb_dir = THUMBS_DIR . '/' . $date_dir;
foreach ([$full_dir, $thumb_dir] as $d) {
    if (!is_dir($d) && !mkdir($d, 0755, true) && !is_dir($d)) {
        jsonResponse(['success' => false, 'message' => 'Server error: cannot create storage'], 500);
    }
}

// ---------- Save image ----------
$token = generateToken();
$filename = $token . '.' . $ext;
$thumbname = $token . '_thumb.' . $ext;

$filepath = $full_dir . '/' . $filename;
$thumbpath = $thumb_dir . '/' . $thumbname;

if (file_put_contents($filepath, $image_data) === false) {
    jsonResponse(['success' => false, 'message' => 'Server error: cannot save image'], 500);
}
chmod($filepath, 0644);

// ---------- Generate thumbnail ----------
createThumbnail($filepath, $thumbpath, $mime);
chmod($thumbpath, 0644);

// ---------- Store metadata ----------
$db = getDB();
$stmt = $db->prepare('
    INSERT INTO posts
        (token, filename, thumbname, title, description, visibility, maturity,
         avatar_name, grid_name, region_name, position,
         viewer_ver, commit_sha, build_num, user_uuid,
         ip_addr, user_agent)
    VALUES
        (:token, :filename, :thumbname, :title, :description, :visibility, :maturity,
         :avatar_name, :grid_name, :region_name, :position,
         :viewer_ver, :commit_sha, :build_num, :user_uuid,
         :ip_addr, :user_agent)
');

$stmt->execute([
    ':token'       => $token,
    ':filename'    => $date_dir . '/' . $filename,
    ':thumbname'   => $date_dir . '/' . $thumbname,
    ':title'       => mb_substr(trim((string)($body['title'] ?? '')), 0, 255),
    ':description' => mb_substr(trim((string)($body['description'] ?? '')), 0, 1000),
    ':visibility'  => in_array($body['visibility'] ?? '', [VISIBILITY_PUBLIC, VISIBILITY_UNLISTED], true) ? $body['visibility'] : VISIBILITY_PUBLIC,
    ':maturity'    => in_array($body['maturity'] ?? '', ['general', 'moderate', 'restricted'], true) ? $body['maturity'] : 'general',
    ':avatar_name' => mb_substr(trim((string)($body['avatar_name'] ?? '')), 0, 255),
    ':grid_name'   => mb_substr(trim((string)($body['grid_name'] ?? '')), 0, 255),
    ':region_name' => mb_substr(trim((string)($body['region_name'] ?? '')), 0, 255),
    ':position'    => mb_substr(trim((string)($body['position'] ?? '')), 0, 255),
    ':viewer_ver'  => mb_substr(trim((string)($body['viewer_ver'] ?? '')), 0, 64),
    ':commit_sha'  => mb_substr(trim((string)($body['commit_sha'] ?? '')), 0, 40),
    ':build_num'   => mb_substr(trim((string)($body['build_num'] ?? '')), 0, 20),
    ':user_uuid'   => mb_substr(trim((string)($body['user_uuid'] ?? '')), 0, 64),
    ':ip_addr'     => $_SERVER['REMOTE_ADDR'] ?? '',
    ':user_agent'  => mb_substr(trim((string)($_SERVER['HTTP_USER_AGENT'] ?? '')), 0, 500),
]);

$postId = $db->lastInsertId();

$postUrl = BASE_URL . '/post.php?id=' . $token;

jsonResponse([
    'success'  => true,
    'post_url' => $postUrl,
    'message'  => 'Snapshot uploaded',
]);

// ---------- Helper: create thumbnail ----------
function createThumbnail(string $srcPath, string $dstPath, string $mime): void
{
    switch ($mime) {
        case 'image/jpeg': $img = @imagecreatefromjpeg($srcPath); break;
        case 'image/png':  $img = @imagecreatefrompng($srcPath); break;
        case 'image/webp': $img = @imagecreatefromwebp($srcPath); break;
        default:           $img = false; break;
    }

    if ($img === false) {
        // Copy original as fallback
        copy($srcPath, $dstPath);
        return;
    }

    $srcW = imagesx($img);
    $srcH = imagesy($img);

    // Calculate crop to fill square
    $size = min($srcW, $srcH);
    $cropX = (int)(($srcW - $size) / 2);
    $cropY = (int)(($srcH - $size) / 2);

    $thumb = imagecreatetruecolor(THUMB_WIDTH, THUMB_HEIGHT);
    if ($mime === 'image/png') {
        imagealphablending($thumb, false);
        imagesavealpha($thumb, true);
    }

    imagecopyresampled($thumb, $img, 0, 0, $cropX, $cropY, THUMB_WIDTH, THUMB_HEIGHT, $size, $size);
    imagedestroy($img);

    switch ($mime) {
        case 'image/jpeg': imagejpeg($thumb, $dstPath, 85); break;
        case 'image/png':  imagepng($thumb, $dstPath, 8); break;
        case 'image/webp': imagewebp($thumb, $dstPath, 85); break;
        default:           imagejpeg($thumb, $dstPath, 85); break;
    }
    imagedestroy($thumb);
}
