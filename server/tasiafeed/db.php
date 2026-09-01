<?php
declare(strict_types=1);

require_once __DIR__ . '/config.php';

function getDB(): PDO
{
    static $db = null;
    if ($db === null) {
        $db = new PDO('sqlite:' . DB_PATH);
        $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $db->exec('PRAGMA journal_mode=WAL');
        $db->exec('PRAGMA foreign_keys=ON');
        initDB($db);
    }
    return $db;
}

function initDB(PDO $db): void
{
    $db->exec('
        CREATE TABLE IF NOT EXISTS posts (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            token       TEXT NOT NULL UNIQUE,
            filename    TEXT NOT NULL,
            thumbname   TEXT NOT NULL,
            title       TEXT NOT NULL DEFAULT "",
            description TEXT NOT NULL DEFAULT "",
            visibility  TEXT NOT NULL DEFAULT "public",
            maturity    TEXT NOT NULL DEFAULT "general",
            avatar_name TEXT NOT NULL DEFAULT "",
            grid_name   TEXT NOT NULL DEFAULT "",
            region_name TEXT NOT NULL DEFAULT "",
            position    TEXT NOT NULL DEFAULT "",
            viewer_ver  TEXT NOT NULL DEFAULT "",
            commit_sha  TEXT NOT NULL DEFAULT "",
            build_num   TEXT NOT NULL DEFAULT "",
            user_uuid   TEXT NOT NULL DEFAULT "",
            ip_addr     TEXT NOT NULL DEFAULT "",
            user_agent  TEXT NOT NULL DEFAULT "",
            created_at  TEXT NOT NULL DEFAULT (CURRENT_TIMESTAMP),
            reported    INTEGER NOT NULL DEFAULT 0,
            hidden      INTEGER NOT NULL DEFAULT 0
        )
    ');

    $db->exec('
        CREATE TABLE IF NOT EXISTS reports (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            post_id     INTEGER NOT NULL REFERENCES posts(id),
            reason      TEXT NOT NULL DEFAULT "",
            reporter_ip TEXT NOT NULL DEFAULT "",
            created_at  TEXT NOT NULL DEFAULT (CURRENT_TIMESTAMP)
        )
    ');
}

function generateToken(): string
{
    return bin2hex(random_bytes(12));
}

function jsonResponse($data, int $code = 200)
{
    http_response_code($code);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($data, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE) . "\n";
    exit;
}

function htmlHeader(string $title): void
{
    header('Content-Type: text/html; charset=utf-8');
    ?><!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title><?= htmlspecialchars($title) ?> - TasiaFeed</title>
<style>
*, *::before, *::after { box-sizing: border-box; }
body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    max-width: 800px; margin: 0 auto; padding: 1em; background: #f5f5f5; color: #333;
}
h1 { color: #e74c8b; }
a { color: #e74c8b; text-decoration: none; }
a:hover { text-decoration: underline; }
.feed { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 1em; }
.feed-item { background: #fff; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.feed-item img { width: 100%; height: 200px; object-fit: cover; display: block; }
.feed-item .info { padding: 0.5em; }
.feed-item .info .title { font-weight: 600; }
.feed-item .info .meta { font-size: 0.85em; color: #666; }
.pagination { text-align: center; margin: 1em 0; }
.pagination a { display: inline-block; padding: 0.5em 1em; margin: 0 0.25em; background: #fff; border-radius: 4px; }
.post-image { max-width: 100%; height: auto; border-radius: 8px; }
.post-meta { background: #fff; padding: 1em; border-radius: 8px; margin-top: 1em; }
.btn { display: inline-block; padding: 0.5em 1em; background: #e74c8b; color: #fff; border: none; border-radius: 4px; cursor: pointer; text-decoration: none; font-size: 0.9em; }
.btn:hover { background: #d0407a; text-decoration: none; }
.btn-small { padding: 0.3em 0.6em; font-size: 0.8em; }
.admin-table { width: 100%; border-collapse: collapse; background: #fff; }
.admin-table th, .admin-table td { padding: 0.5em; border: 1px solid #ddd; text-align: left; }
.admin-table th { background: #f0f0f0; }
.notice { background: #fff3cd; border: 1px solid #ffeeba; padding: 1em; border-radius: 4px; margin: 1em 0; }
.error { background: #f8d7da; border: 1px solid #f5c6cb; padding: 1em; border-radius: 4px; margin: 1em 0; }
</style>
</head>
<body>
<h1><a href="<?= BASE_URL ?>/">TasiaFeed</a></h1>
<?php
}

function htmlFooter(): void
{
    echo '<p style="margin-top:2em;font-size:0.85em;color:#999;text-align:center">TasiaFeed &mdash; part of Tasia Viewer</p>';
    echo '</body></html>';
}
