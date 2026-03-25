# Web socket communication protocol specification
Specification of the communication protocol between backend and frontend.
## PAK
### Prompt operation
Prompts user to select a PAK resource file and responds with path to the selected file.
#### Request
```json
{
  "endpoint": "pak",
  "payload": {
    "operation": "prompt"
  }
}
```
#### Response
```json
{
  "resource_path": "/path/to/pak"
}
```

### Read operation
Reads a PAK resource file and responds with asset listing.
##### Request
```json
{
  "endpoint": "pak",
  "payload": {
    "operation": "read",
    "resource_path": "/path/to/pak"
  }
}
```
#### Response
```json
{
  "assets": [
    {
      "timestamp": 0,
      "are_data_embedded": false,
      "are_data_compressed": false,
      "path": "/path/to/asset"
    }
  ]
}
```
### Write operation
Writes a PAK resource file.
##### Request
```json
{
  "endpoint": "pak",
  "payload": {
    "operation": "write",
    "resource_path": "/path/to/pak",
    "target_resource_path": "/path/to/new/pak"
  }
}
```
#### Response
```json
{}
```

### Invalidate operation
Invalidates a PAK resource file.
##### Request
```json
{
  "endpoint": "pak",
  "payload": {
    "operation": "invalidate",
    "resource_path": "/path/to/pak"
  }
}
```
#### Response
```json
{}
```

## Asset

### Read operation
Read an asset data from a resource file.
##### Request
```json
{
  "endpoint": "asset",
  "payload": {
    "operation": "read",
    "resource_path": "/path/to/pak",
    "asset_path": "/path/to/asset"
  }
}
```
#### Response
```json
{
  "data": [0, 0, 0, 0]
}
```

### Write operation
Writes an asset data to a resource file.
##### Request
```json
{
  "endpoint": "asset",
  "payload": {
    "operation": "write",
    "resource_path": "/path/to/pak",
    "asset_path": "/path/to/asset",
    "data": [0, 0, 0, 0]
  }
}
```
#### Response
```json
{}
```
