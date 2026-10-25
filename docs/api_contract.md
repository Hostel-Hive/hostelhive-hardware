# Hardware-to-Backend API Contract
**Endpoint:** `POST /api/v1/attendance/scan`  
**Reference:** HostelHive SRS (FR011, FR012, FR013) & SDS Section 6.3 (Hardware Security)

## Request Headers
| Header | Value | Description |
|:---|:---|:---|
| `Content-Type` | `application/json` | Request payload format |
| `X-Device-API-Key` | `<pre-shared-secret>` | Preshared device authentication key provisioned per node |
| `X-Node-ID` | `GATE_ENTRY_01` / `GATE_EXIT_01` | Unique hardware node identifier |

## Request Payload
```json
{
  "student_id": "HH-STU-2021-CS-045",
  "node_id": "GATE_ENTRY_01",
  "direction": "IN",
  "timestamp": "2026-10-08T15:30:00+05:30",
  "offline_synced": false
}
```

## Response Payloads
### 200 OK - Access Granted
```json
{
  "status": "success",
  "access": true,
  "student_name": "Kasun Perera",
  "room_number": "A-204",
  "message": "Welcome back, Kasun!"
}
```

### 403 Forbidden - Access Denied (Hostel Pass Suspended / Leave Not Approved)
```json
{
  "status": "rejected",
  "access": false,
  "message": "Access Denied: Unapproved Leave / Late Entry"
}
```

### 429 Too Many Requests - Scan Cooldown Active (FR014)
```json
{
  "status": "cooldown",
  "access": false,
  "message": "Duplicate scan ignored. Please wait 15 seconds."
}
```
