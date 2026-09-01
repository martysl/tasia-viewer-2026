<?php
declare(strict_types=1);

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/db.php';

$token = trim((string)($_GET['id'] ?? ''));
if ($token === '') {
    http_response_code(404);
    echo 'Missing post ID';
    exit;
}

$db = getDB();
$stmt = $db->prepare('SELECT * FROM posts WHERE token = :token AND hidden = 0');
$stmt->execute([':token' => $token]);
$post = $stmt->fetch(PDO::FETCH_ASSOC);

if (!$post) {
    http_response_code(404);
    htmlHeader('Not Found');
    echo '<div class="error">Post not found.</div>';
    htmlFooter();
    exit;
}

// Handle report
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['report'])) {
    $reason = mb_substr(trim((string)($_POST['reason'] ?? '')), 0, 500);
    $rStmt = $db->prepare('INSERT INTO reports (post_id, reason, reporter_ip) VALUES (:pid, :reason, :ip)');
    $rStmt->execute([':pid' => $post['id'], ':reason' => $reason, ':ip' => $_SERVER['REMOTE_ADDR'] ?? '']);
    $db->prepare('UPDATE posts SET reported = reported + 1 WHERE id = :id')->execute([':id' => $post['id']]);
    $reported = true;
}

$title = $post['title'] ?: 'Untitled';
htmlHeader($title);
?>

<div class="post-image">
    <a href="<?= UPLOADS_URL . '/' . htmlspecialchars($post['filename']) ?>">
        <img src="<?= UPLOADS_URL . '/' . htmlspecialchars($post['filename']) ?>" alt="<?= htmlspecialchars($title) ?>" class="post-image">
    </a>
</div>

<div class="post-meta">
    <h2><?= htmlspecialchars($title) ?></h2>
    <?php if ($post['description']): ?>
        <p><?= nl2br(htmlspecialchars($post['description'])) ?></p>
    <?php endif; ?>

    <table>
        <?php if ($post['avatar_name']): ?><tr><th>Avatar</th><td><?= htmlspecialchars($post['avatar_name']) ?></td></tr><?php endif; ?>
        <?php if ($post['grid_name']): ?><tr><th>Grid</th><td><?= htmlspecialchars($post['grid_name']) ?></td></tr><?php endif; ?>
        <?php if ($post['region_name']): ?><tr><th>Region</th><td><?= htmlspecialchars($post['region_name']) ?></td></tr><?php endif; ?>
        <?php if ($post['position']): ?><tr><th>Position</th><td><?= htmlspecialchars($post['position']) ?></td></tr><?php endif; ?>
        <?php if ($post['viewer_ver']): ?><tr><th>Viewer</th><td><?= htmlspecialchars($post['viewer_ver']) ?></td></tr><?php endif; ?>
        <?php if ($post['commit_sha']): ?><tr><th>Commit</th><td><?= htmlspecialchars($post['commit_sha']) ?></td></tr><?php endif; ?>
        <?php if ($post['build_num']): ?><tr><th>Build</th><td><?= htmlspecialchars($post['build_num']) ?></td></tr><?php endif; ?>
        <tr><th>Date</th><td><?= htmlspecialchars(gmdate('Y-m-d H:i:s', strtotime($post['created_at']))) ?> UTC</td></tr>
        <tr><th>Visibility</th><td><?= htmlspecialchars($post['visibility']) ?></td></tr>
    </table>

    <?php if (isset($reported) && $reported): ?>
        <div class="notice">Thank you. This post has been reported.</div>
    <?php else: ?>
        <form method="post" style="margin-top:1em">
            <input type="hidden" name="report" value="1">
            <label>Report this post:<br>
                <input type="text" name="reason" placeholder="Reason (optional)" size="40" maxlength="500">
            </label>
            <button type="submit" class="btn btn-small">Report</button>
        </form>
    <?php endif; ?>
</div>

<p><a href="<?= BASE_URL ?>/">&laquo; Back to feed</a></p>

<?php htmlFooter();
