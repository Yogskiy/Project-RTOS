<?php
// htdocs/rfid/api/verify.php
// POST { "uid": "12345678" }
// Response: { "authorized": true/false, "user_id": int, "user_name": string }

require_once __DIR__ . '/../db.php';

requireMethod('POST');
$body = getRequestBody();

$uid = strtoupper(trim($body['uid'] ?? ''));

// Basic format validation: exactly 8 hex characters
if (!preg_match('/^[0-9A-F]{8}$/', $uid)) {
    jsonResponse(['error' => 'Invalid UID format — must be 8 hex characters'], 400);
}

try {
    $db = getDB();

    $stmt = $db->prepare(
        'SELECT id, name FROM users
          WHERE uid = :uid AND is_active = 1
          LIMIT 1'
    );
    $stmt->execute([':uid' => $uid]);
    $row = $stmt->fetch();

    if ($row) {
        // Update last_seen timestamp
        $upd = $db->prepare('UPDATE users SET last_seen = NOW() WHERE id = :id');
        $upd->execute([':id' => $row['id']]);

        jsonResponse([
            'authorized' => true,
            'user_id'    => (int)$row['id'],
            'user_name'  => $row['name'],
        ]);
    } else {
        jsonResponse([
            'authorized' => false,
            'user_id'    => 0,
            'user_name'  => '',
        ]);
    }

} catch (PDOException $e) {
    error_log('[rfid/verify] DB error: ' . $e->getMessage());
    jsonResponse(['error' => 'Database error'], 500);
}