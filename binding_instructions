# ==============================================================================
# MATTER CROSS-NODE BINDING CONFIGURATION GUIDE (WSL / chip-tool)
# ==============================================================================
# NOTE FOR WSL USERS: chip-tool defaults to storing encryption keys in /tmp. 
# If WSL fully restarts, your controller state is wiped. To make your pairing 
# permanent, append `--storage-directory ~/chip_storage` to ALL commands below.
# ==============================================================================

# After the devices have been commissioned to Google Home, you need to allow them to be managed by a second ecosystem.
# In the Google Home app, open the device > tap the kebab (three dots) in the upper right > tap Linked Matter apps & services.
# Tap Link apps & services > Use pairing code (you might have to scroll right to see this option).

# In WSL, type: chip-tool pairing code <NodeID> <11-digit number from the Google Home app>, e.g.:
chip-tool pairing code 101 03845945480

# That device is now Node 101. Repeat for the other device, assigning a new Node ID and using its unique pairing code, e.g.:
chip-tool pairing code 102 84863258401

# Pay attention to which device you assigned the node number.
# In this example I made node 101 the generic_switch and 102 the light.
# This'll be important later when you're creating the actual binding.

# Find your chip-tool Fabric Index. Because this is a secondary ecosystem, your fabric index will NOT be 1.
# Run this read command and look at the "FabricIndex" value in the output:
chip-tool accesscontrol read acl 101 0

# The initial output will look like this near the end of the log. 
# You are looking for the "FabricIndex" assigned to your chip-tool admin entry (usually 2 or 3):
#   ACL: 1 entries
#     [1]: {
#       Privilege: 5
#       AuthMode: 2
#       Subjects: 1 entries
#         [1]: 112233
#       Targets: null
#       FabricIndex: 3
#     }

# Update the ACLs on both devices so they permit cross-node commands.
# IMPORTANT: Replace "3" in the fabricIndex fields below with the actual Fabric Index you found in the previous step.
# Note: "112233" is the default chip-tool controller ID. Leave this so you don't lock yourself out of admin rights.
chip-tool accesscontrol write acl '[{"fabricIndex": 3, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 3, "privilege": 3, "authMode": 2, "subjects": [102], "targets": null}]' 101 0
chip-tool accesscontrol write acl '[{"fabricIndex": 3, "privilege": 5, "authMode": 2, "subjects": [112233], "targets": null}, {"fabricIndex": 3, "privilege": 3, "authMode": 2, "subjects": [101], "targets": null}]' 102 0

# Now both devices have permission to talk to each other. Finally, you need to configure the routing (Bindings) 
# to tell the devices exactly which endpoints and clusters to send commands to.
# Node 101 (Endpoint 1, Switch) sends On/Off (Cluster 6) commands to Node 102 (Endpoint 1, Light):
chip-tool binding write binding '[{"fabricIndex": 3, "node": 102, "endpoint": 1, "cluster": 6}]' 101 1

# Node 102 (Endpoint 1, Light) sends On/Off (Cluster 6) state updates back to Node 101 (Endpoint 2, Light):
chip-tool binding write binding '[{"fabricIndex": 3, "node": 101, "endpoint": 2, "cluster": 6}]' 102 1
