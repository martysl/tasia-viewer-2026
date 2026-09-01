<?php
declare(strict_types=1);

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/db.php';

$authError = '';
$adminAuthenticated = false;

// Simple session-less auth
$secret = (string)($_GET['secret'] ?? $_POST['secret'] ?? '');
if ($secret !== '' && hash_equals(ADMIN_SECRET, $secret)) {
    $adminAuthenticated = true;
}

// Actions
if ($adminAuthenticated && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = (string)($_POST['action'] ?? '');

    if ($action === 'hide' && isset($_POST['post_id'])) {
        $db = getDB();
        $db->prepare('UPDATE posts SET hidden = 1 WHERE id = :id')->execute([':id' => (int)$_POST['post_id']]);
    }

    if ($action === 'unhide' && isset($_POST['post_id'])) {
        $db = getDB();
        $db->prepare('UPDATE posts SET hidden = 0 WHERE id = :id')->execute([':id' => (int)$_POST['post_id']]);
    }

    if ($action === 'delete' && isset($_POST['post_id'])) {
        $db = getDB();
        $stmt = $db->prepare('SELECT filename, thumbname FROM posts WHERE id = :id');
        $stmt->execute([':id' => (int)$_POST['post_id']]);
        $post = $stmt->fetch(PDO::FETCH_ASSOC);
        if ($post) {
            foreach ([STORAGE_DIR . '/' . $post['filename'], THUMBS_DIR . '/' . $post['thumbname']] as $f) {
                if (file_exists($f)) @unlink($f);
            }
            $db->prepare('DELETE FROM posts WHERE id = :id')->execute([':id' => (int)$_POST['post_id']]);
            $db->prepare('DELETE FROM reports WHERE post_id = :id')->execute([':id' => (int)$_POST['post_id']]);
        }
    }
}

// ---------- Render ----------
if (!$adminAuthenticated) {
    htmlHeader('Admin Login');
    ?>
    <form method="get" action="">
        <label>Admin Secret:<br>
            <input type="password" name="secret" size="40">
        </label>
        <button type="submit" class="btn">Login</button>
    </form>
    <?php
    htmlFooter();
    exit;
}

$db = getDB();

// Stats
$totalStmt = $db->query("SELECT COUNT(*) FROM posts"); $totalPosts = (int)$totalStmt->fetchColumn();
$repStmt = $db->query("SELECT COUNT(*) FROM posts WHERE reported > 0"); $reportedPosts = (int)$repStmt->fetchColumn();
$hidStmt = $db->query("SELECT COUNT(*) FROM posts WHERE hidden = 1"); $hiddenPosts = (int)$hidStmt->fetchColumn();

htmlHeader('Admin Panel');
?>

<div class="admin-stats" style="margin-bottom:1em">
    <strong>Stats:</strong>
    Total: <?= $totalPosts ?> |
    Reported: <?= $reportedPosts ?> |
    Hidden: <?= $hiddenPosts ?>
</div>

<h3>Reported Posts</h3>
<?php
$reported = $db->query('
    SELECT p.*, r.reason, r.reporter_ip, r.created_at AS report_date
    FROM posts p
    JOIN reports r ON r.post_id = p.id
    ORDER BY r.created_at DESC
    LIMIT 50
')->fetchAll(PDO::FETCH_ASSOC);

if (empty($reported)): ?>
<p>No reported posts.</p>
<?php else: ?>
<table class="admin-table">
<tr><th>ID</th><th>Token</th><th>Title</th><th>Avatar</th><th>Reason</th><th>Reporter</th><th>Reported</th><th>Hidden</th><th>Actions</th></tr>
<?php foreach ($reported as $p): ?>
<tr>
    <td><?= $p['id'] ?></td>
    <td><a href="post.php?id=<?= rawurlencode($p['token']) ?>"><?= htmlspecialchars(mb_substr($p['token'], 0, 12)) ?>...</a></td>
    <td><?= htmlspecialchars(mb_substr($p['title'] ?: 'Untitled', 0, 30)) ?></td>
    <td><?= htmlspecialchars($p['avatar_name']) ?></td>
    <td><?= htmlspecialchars($p['reason']) ?></td>
    <td><?= htmlspecialchars($p['reporter_ip']) ?></td>
    <td><?= htmlspecialchars(gmdate('Y-m-d', strtotime($p['report_date']))) ?></td>
    <td><?= $p['hidden'] ? 'Yes' : 'No' ?></td>
    <td>
        <form method="post" style="display:inline">
            <input type="hidden" name="secret" value="<?= htmlspecialchars($secret) ?>">
            <input type="hidden" name="post_id" value="<?= $p['id'] ?>">
            <?php if ($p['hidden']): ?>
                <button type="submit" name="action" value="unhide" class="btn btn-small">Unhide</button>
            <?php else: ?>
                <button type="submit" name="action" value="hide" class="btn btn-small">Hide</button>
            <?php endif; ?>
            <button type="submit" name="action" value="delete" class="btn btn-small" onclick="return confirm('Delete this post permanently?')">Delete</button>
        </form>
    </td>
</tr>
<?php endforeach; ?>
</table>
<?php endif; ?>

<h3>All Posts</h3>
<?php
$allPosts = $db->query('SELECT id, token, title, avatar_name, hidden, reported, created_at FROM posts ORDER BY created_at DESC LIMIT 50')->fetchAll(PDO::FETCH_ASSOC);
?>
<table class="admin-table">
<tr><th>ID</th><th>Token</th><th>Title</th><th>Avatar</th><th>Hidden</th><th>Reported</th><th>Date</th><th>Actions</th></tr>
<?php foreach ($allPosts as $p): ?>
<tr>
    <td><?= $p['id'] ?></td>
    <td><a href="post.php?id=<?= rawurlencode($p['token']) ?>"><?= htmlspecialchars(mb_substr($p['token'], 0, 12)) ?>...</a></td>
    <td><?= htmlspecialchars(mb_substr($p['title'] ?: 'Untitled', 0, 30)) ?></td>
    <td><?= htmlspecialchars($p['avatar_name']) ?></td>
    <td><?= $p['hidden'] ? 'Yes' : 'No' ?></td>
    <td><?= $p['reported'] ?></td>
    <td><?= htmlspecialchars(gmdate('Y-m-d', strtotime($p['created_at']))) ?></td>
    <td>
        <form method="post" style="display:inline">
            <input type="hidden" name="secret" value="<?= htmlspecialchars($secret) ?>">
            <input type="hidden" name="post_id" value="<?= $p['id'] ?>">
            <?php if ($p['hidden']): ?>
                <button type="submit" name="action" value="unhide" class="btn btn-small">Unhide</button>
            <?php else: ?>
                <button type="submit" name="action" value="hide" class="btn btn-small">Hide</button>
            <?php endif; ?>
            <button type="submit" name="action" value="delete" class="btn btn-small" onclick="return confirm('Delete this post permanently?')">Delete</button>
        </form>
    </td>
</tr>
<?php endforeach; ?>
</table>

<?php htmlFooter();
