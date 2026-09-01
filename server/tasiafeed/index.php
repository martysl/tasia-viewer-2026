<?php
declare(strict_types=1);

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/db.php';

$page = max(1, (int)($_GET['page'] ?? 1));
$offset = ($page - 1) * ITEMS_PER_PAGE;

$db = getDB();

// Count total
$totalStmt = $db->query('SELECT COUNT(*) FROM posts WHERE visibility = "public" AND hidden = 0');
$total = (int)$totalStmt->fetchColumn();
$maxPage = max(1, (int)ceil($total / ITEMS_PER_PAGE));

// Fetch posts
$stmt = $db->prepare('
    SELECT token, thumbname, title, avatar_name, grid_name, created_at
    FROM posts
    WHERE visibility = "public" AND hidden = 0
    ORDER BY created_at DESC
    LIMIT :limit OFFSET :offset
');
$stmt->bindValue(':limit', ITEMS_PER_PAGE, PDO::PARAM_INT);
$stmt->bindValue(':offset', $offset, PDO::PARAM_INT);
$stmt->execute();
$posts = $stmt->fetchAll(PDO::FETCH_ASSOC);

htmlHeader('Feed');

if (empty($posts)): ?>
<div class="notice">No snapshots yet. Be the first!</div>
<?php else: ?>
<div class="feed">
<?php foreach ($posts as $post): ?>
    <div class="feed-item">
        <a href="post.php?id=<?= rawurlencode($post['token']) ?>">
            <img src="<?= THUMBS_URL . '/' . htmlspecialchars($post['thumbname']) ?>" alt="" loading="lazy">
        </a>
        <div class="info">
            <div class="title"><a href="post.php?id=<?= rawurlencode($post['token']) ?>"><?= htmlspecialchars($post['title'] ?: 'Untitled') ?></a></div>
            <div class="meta">by <?= htmlspecialchars($post['avatar_name'] ?: 'Unknown') ?> on <?= htmlspecialchars($post['grid_name'] ?: 'Unknown') ?></div>
            <div class="meta"><?= htmlspecialchars(gmdate('Y-m-d H:i', strtotime($post['created_at']))) ?></div>
        </div>
    </div>
<?php endforeach; ?>
</div>

<div class="pagination">
<?php if ($page > 1): ?>
    <a href="?page=<?= $page - 1 ?>">&laquo; Previous</a>
<?php endif; ?>
    <span>Page <?= $page ?> of <?= $maxPage ?></span>
<?php if ($page < $maxPage): ?>
    <a href="?page=<?= $page + 1 ?>">Next &raquo;</a>
<?php endif; ?>
</div>
<?php endif;

htmlFooter();
