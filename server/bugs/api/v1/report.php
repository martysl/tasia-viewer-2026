<?php
declare(strict_types=1);

// Tasia Viewer crash report receiver.
// Upload to: https://apps.easierit.org/igrid/bugs/api/v1/report
//
// Expected multipart fields from the Linux crash logger:
// - upload_file_minidump: crash minidump file
// - product: viewer product name
// - version: viewer version

$supportEmail = '009daw+viewersupport@gmail.com';
$storageRoot = __DIR__ . '/../../../crash_reports';
$maxBytes = 64 * 1024 * 1024;

function fail(int $code, string $message)
{
    http_response_code($code);
    header('Content-Type: text/plain; charset=utf-8');
    echo $message . "\n";
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    fail(405, 'POST required');
}

if (!isset($_FILES['upload_file_minidump'])) {
    fail(400, 'Missing upload_file_minidump');
}

$file = $_FILES['upload_file_minidump'];
if (!is_array($file) || ($file['error'] ?? UPLOAD_ERR_NO_FILE) !== UPLOAD_ERR_OK) {
    fail(400, 'Upload failed: ' . (string)($file['error'] ?? 'unknown'));
}

if (($file['size'] ?? 0) <= 0 || ($file['size'] ?? 0) > $maxBytes) {
    fail(413, 'Invalid minidump size');
}

$product = preg_replace('/[^A-Za-z0-9_.-]/', '_', (string)($_POST['product'] ?? 'Tasia-Releasex64'));
$version = preg_replace('/[^A-Za-z0-9_.-]/', '_', (string)($_POST['version'] ?? 'unknown'));
$stamp = gmdate('Ymd_His');
$token = bin2hex(random_bytes(8));

$dir = $storageRoot . '/' . gmdate('Y/m/d');
if (!is_dir($dir) && !mkdir($dir, 0750, true) && !is_dir($dir)) {
    fail(500, 'Unable to create storage directory');
}

$base = $stamp . '_' . $product . '_' . $version . '_' . $token;
$dumpPath = $dir . '/' . $base . '.dmp';
$metaPath = $dir . '/' . $base . '.json';

if (!move_uploaded_file((string)$file['tmp_name'], $dumpPath)) {
    fail(500, 'Unable to store minidump');
}
chmod($dumpPath, 0640);

$meta = [
    'received_utc' => gmdate('c'),
    'product' => $product,
    'version' => $version,
    'remote_addr' => $_SERVER['REMOTE_ADDR'] ?? null,
    'user_agent' => $_SERVER['HTTP_USER_AGENT'] ?? null,
    'file' => basename($dumpPath),
    'size' => (int)$file['size'],
];
file_put_contents($metaPath, json_encode($meta, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n");
chmod($metaPath, 0640);

// Best-effort email notification. The dump is stored on disk; it is not attached.
$subject = 'Tasia Viewer crash report ' . $product . ' ' . $version;
$body = "A Tasia Viewer crash report was received.\n\n"
    . "Product: {$product}\n"
    . "Version: {$version}\n"
    . "File: {$dumpPath}\n"
    . "Size: " . (int)$file['size'] . " bytes\n"
    . "Remote: " . ($_SERVER['REMOTE_ADDR'] ?? 'unknown') . "\n";
@mail($supportEmail, $subject, $body, 'From: crashraport@apps.easierit.org');

http_response_code(200);
header('Content-Type: text/plain; charset=utf-8');
echo "OK\n";
