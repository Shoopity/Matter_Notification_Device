
# Matter Cross-Node Binding Configuration Guide
*(WSL / chip-tool)*

---

> **⚠️ NOTE FOR WSL USERS**  
> `chip-tool` defaults to storing encryption keys in `/tmp`. If WSL fully restarts, your controller state is wiped. To make your pairing permanent, append `--storage-directory ~/chip_storage` to **ALL** `chip-tool` commands below.

---

## Step 1: Obtain Pairing Codes from Primary Ecosystem

After commissioning your devices to Google Home, you must generate a pairing code to allow them to be managed by a second ecosystem (`chip-tool`):

1. Open the **Google Home** app.
2. Open your target device and tap the **kebab menu (⋮)** in the upper right.
3. Select **Linked Matter apps & services**.
4. Tap **Link apps & services** > **Use pairing code** *(you may need to scroll right to see this option)*.

---

## Step 2: Commission Devices in `chip-tool`

Pair each device in WSL using its unique pairing code from Google Home and assign each a unique Node ID.

```bash
# Pair Node 101 (Generic Switch)
chip-tool pairing code 101 03845945480

# Pair Node 102 (Light)
chip-tool pairing code 102 84863258401
```

> **Note:** Pay close attention to which device you assign each Node ID to! These can be assigned however you want, e.g.:
> * **Node 101:** `generic_switch`
> * **Node 102:** `light`

---

## Step 3: Find Your `chip-tool` Fabric Index

Because `chip-tool` is acting as a secondary ecosystem, your fabric index will **not** be `1`. Run the following command to check your assigned Fabric Index:

```bash
chip-tool accesscontrol read acl 101 0
```

Look for the `FabricIndex` value assigned to your `chip-tool` admin entry near the bottom of the output (usually `2` or `3`):

```yaml
ACL: 1 entries
  [1]: {
    Privilege: 5
    AuthMode: 2
    Subjects: 1 entries
      [1]: 112233
    Targets: null
    FabricIndex: 2
  }
```

---

## Step 4: Update Access Control Lists (ACLs)

Update the ACLs on both devices so they permit cross-node communication with each other.

> **Important:** 
> * Replace `3` in the `fabricIndex` fields below with the actual Fabric Index you identified in **Step 3**.
> * Keep `112233` (the default `chip-tool` controller ID) as-is so you don't lock yourself out of admin rights.

```bash
# Allow Node 102 to send commands to Node 101
chip-tool accesscontrol write acl '[{"fabricIndex": 2, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 2, "privilege": 3, "authMode": 2, "subjects": [102], "targets": null}]' 101 0

# Allow Node 101 to send commands to Node 102
chip-tool accesscontrol write acl '[{"fabricIndex": 2, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 2, "privilege": 3, "authMode": 2, "subjects": [101], "targets": null}]' 102 0
```

---

## Step 5: Configure Routing (Bindings)

Now that both devices have permission to talk to each other, configure the bindings to route commands to the correct endpoints and clusters.

```bash
# Node 101 (Switch Device, Endpoint 1, the button) sends On/Off (Cluster 6) commands to Node 102 (Light device, Endpoint 1, the light)
chip-tool binding write binding '[{"fabricIndex": 2, "node": 102, "endpoint": 1, "cluster": 6}]' 101 1

# Node 102 (Light device, Endpoint 1, the light) sends On/Off (Cluster 6) state updates back to Node 101 (Switch Device, Endpoint 2, the light)
chip-tool binding write binding '[{"fabricIndex": 2, "node": 101, "endpoint": 2, "cluster": 6}]' 102 1
```
