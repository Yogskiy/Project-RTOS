<?php
// htdocs/rfid/api/log_event.php
// POST {
//   "type": 0,          (EventType enum value)
//   "uid": "12345678",
//   "user_name": "...",
//   "result": 1,
//   "message": "..."
// }
// Response: { "success": true, "log_id": int }

require_once __DIR__ . '/../db.php';

requireMethod('POST');
$body = getRequestBody();

$event_type = (int)($body['type']      ?? 7);
$uid        = strtoupper(trim($body['uid']       ?? ''));
$user_name  = substr(trim($body['user_name'] ?? ''), 0, 50);
$result     = (int)($body['result']    ?? 0);
$message    = substr(trim($body['message']   ?? ''), 0, 200);

// uid is optional (empty for system-level events)
if ($uid !== '' && !preg_match('/^[0-9A-F]{8}$/', $uid)) {
    $uid = null;  // Don't reject; just store NULL
}

try {
    $db = getDB();

    $stmt = $db->prepare(
        'INSERT INTO event_log (event_type, uid, user_name, result, message)
         VALUES (:type, :uid, :user_name, :result, :message)'
    );
    $stmt->execute([
        ':type'      => $event_type,
        ':uid'       => ($uid !== '') ? $uid : null,
        ':user_name' => ($user_name !== '') ? $user_name : null,
        ':result'    => $result,
        ':message'   => ($message !== '') ? $message : null,
    ]);

    jsonResponse(['success' => true, 'log_id' => (int)$db->lastInsertId()]);

} catch (PDOException $e) {
    error_log('[rfid/log_event] DB error: ' . $e->getMessage());
    jsonResponse(['error' => 'Database error'], 500);
}