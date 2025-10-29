#!/usr/bin/env python3
"""
Quick LCARS Meshtastic Hardware Test
"""

from wizard_meshtastic_client import WizardMeshtasticClient

def test_hardware():
    print("🧪 Testing Meshtastic Hardware Detection...")
    print("=" * 50)
    
    try:
        client = WizardMeshtasticClient()
        nodes = client.discover_meshtastic_nodes()
        
        print(f"📡 Found {len(nodes)} Meshtastic nodes:")
        for node in nodes:
            print(f"  - Port: {node['port']}")
            print(f"    Description: {node['description']}")
            if 'node_info' in node and node['node_info']:
                info = node['node_info']
                if 'owner' in info:
                    print(f"    Owner: {info['owner']}")
                if 'model' in info:
                    print(f"    Model: {info['model']}")
                if 'battery' in info:
                    print(f"    Battery: {info['battery']}")
            print()
        
        if nodes:
            print("🧪 Testing network health...")
            health = client.get_network_health()
            print(f"Network Status: {health.get('status', 'unknown')}")
            print(f"Visible Mesh Nodes: {health.get('visible_mesh_nodes', 0)}")
        
        print("✅ Hardware test complete!")
        
    except Exception as e:
        print(f"❌ Error during test: {e}")

if __name__ == "__main__":
    test_hardware()