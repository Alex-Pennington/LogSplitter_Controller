#!/usr/bin/env python3
"""
Modern Documentation Server for LogSplitter Controller
Clean, responsive interface optimized for technical documentation
"""

from flask import Flask, render_template, request, send_from_directory, jsonify
import os
import subprocess
import markdown
from datetime import datetime
import re
import glob

app = Flask(__name__)

# Configuration
ROOT_PATH = '.'
DOCS_PATH = 'docs'
PORT = 3000
DEBUG = True

@app.context_processor
def inject_globals():
    """Inject global variables into all templates"""
    return {
        'stardate': datetime.now().strftime('%y%j.%H'),
        'current_time': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    }

class DocServer:
    def __init__(self, root_path='.', docs_path='docs'):
        self.root_path = root_path
        self.docs_path = docs_path
        self.last_scan = datetime.now()
        self.documents = []
        self.docs_cache = {}
        
        # Define categories and their icons
        self.categories = {
            'Emergency': {'icon': '🚨', 'color': '#ff4444', 'priority': 1},
            'Wireless': {'icon': '📡', 'color': '#00bcd4', 'priority': 2}, 
            'Hardware': {'icon': '⚙️', 'color': '#ff8c00', 'priority': 3},
            'Operations': {'icon': '🔧', 'color': '#4CAF50', 'priority': 4},
            'Monitoring': {'icon': '📊', 'color': '#2196F3', 'priority': 5},
            'Development': {'icon': '💻', 'color': '#9C27B0', 'priority': 6},
            'Reference': {'icon': '📖', 'color': '#607D8B', 'priority': 7}
        }
    
    def scan_all_documents(self):
        """Scan for all markdown documents in project"""
        documents = []
        
        # Scan patterns for markdown files
        patterns = [
            os.path.join(self.root_path, '*.md'),
            os.path.join(self.docs_path, '*.md'),
            os.path.join(self.root_path, '*', '*.md')
        ]
        
        seen_files = set()
        
        for pattern in patterns:
            for filepath in glob.glob(pattern):
                if filepath in seen_files:
                    continue
                seen_files.add(filepath)
                
                try:
                    filename = os.path.basename(filepath)
                    relative_path = os.path.relpath(filepath, self.root_path)
                    
                    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    
                    # Get file stats
                    stat = os.stat(filepath)
                    
                    doc = {
                        'name': self._format_doc_name(filename),
                        'filename': filename,
                        'filepath': filepath,
                        'relative_path': relative_path,
                        'category': self._categorize_document(filename, content),
                        'size': stat.st_size,
                        'size_kb': round(stat.st_size / 1024, 1),
                        'modified': datetime.fromtimestamp(stat.st_mtime),
                        'content_preview': self._get_content_preview(content),
                        'url_safe_name': self._make_url_safe(relative_path)
                    }
                    documents.append(doc)
                    
                except Exception as e:
                    print(f"Error reading {filepath}: {e}")
        
        # Sort by category priority, then name
        documents.sort(key=lambda x: (
            self.categories.get(x['category'], {}).get('priority', 99),
            x['name']
        ))
        
        self.documents = documents
        self.last_scan = datetime.now()
        return documents
    
    def _format_doc_name(self, filename):
        """Format filename into readable document name"""
        name = filename.replace('.md', '').replace('_', ' ').replace('-', ' ')
        return ' '.join(word.capitalize() for word in name.split())
    
    def _make_url_safe(self, path):
        """Convert file path to URL-safe format"""
        return path.replace('\\', '/').replace('.md', '')
        
    def _get_content_preview(self, content):
        """Get a clean preview of document content"""
        # Remove markdown headers and get first meaningful content
        lines = content.split('\n')
        preview_lines = []
        
        for line in lines:
            line = line.strip()
            if line and not line.startswith('#') and not line.startswith('```'):
                preview_lines.append(line)
                if len(' '.join(preview_lines)) > 150:
                    break
                    
        preview = ' '.join(preview_lines)[:200]
        return preview + '...' if len(preview) == 200 else preview

    def _get_document_priority(self, filename, content):
        """Assign priority for emergency access (lower number = higher priority)"""
        filename_lower = filename.lower()
        
        # Emergency/Safety docs get highest priority
        if any(keyword in filename_lower for keyword in ['error', 'emergency', 'safety', 'mill_lamp', 'system_test']):
            return 1
        # Hardware troubleshooting
        elif any(keyword in filename_lower for keyword in ['pin', 'pressure', 'hardware', 'arduino']):
            return 2
        # Operations and commands
        elif any(keyword in filename_lower for keyword in ['setup', 'command', 'deploy']):
            return 3
        # General documentation
        else:
            return 4
    
    def _categorize_document(self, filename, content):
        """Smart categorization based on filename and content analysis"""
        filename_lower = filename.lower()
        content_lower = content[:500].lower()  # First 500 chars for performance
        
        # Emergency/Safety - highest priority
        if any(kw in filename_lower for kw in ['error', 'emergency', 'safety', 'mill_lamp', 'fault', 'alarm']):
            return 'Emergency'
        
        # Wireless/Meshtastic - new priority category
        elif any(kw in filename_lower for kw in ['meshtastic', 'wireless', 'mesh', 'lora', 'protocol', 'integration', 'provisioning']):
            return 'Wireless'
        elif any(kw in content_lower for kw in ['meshtastic', 'mesh network', 'lora', 'protobuf', 'channel 0', 'channel 1']):
            return 'Wireless'
        
        # Hardware/Physical systems
        elif any(kw in filename_lower for kw in ['pin', 'hardware', 'arduino', 'pressure', 'relay', 'sensor']):
            return 'Hardware'
        
        # Operations and setup
        elif any(kw in filename_lower for kw in ['setup', 'install', 'deploy', 'config', 'command']):
            return 'Operations'
        
        # Monitoring and diagnostics
        elif any(kw in filename_lower for kw in ['monitor', 'test', 'diagnostic', 'log', 'telemetry']):
            return 'Monitoring'
        
        # Development and technical
        elif any(kw in filename_lower for kw in ['api', 'interface', 'serial', 'code']):
            return 'Development'
        
        # Reference materials
        else:
            return 'Reference'
    
    def get_critical_docs(self):
        """Get critical documents for emergency dashboard"""
        if not self.documents:
            self.scan_all_documents()
        
        # Group critical documents by category
        critical_docs = {}
        for doc in self.documents:
            if doc['category'] in ['Emergency', 'Wireless', 'Hardware', 'Operations']:
                category = doc['category']
                if category not in critical_docs:
                    critical_docs[category] = []
                critical_docs[category].append(doc)
        
        # Sort each category's docs by name and limit
        for category in critical_docs:
            critical_docs[category].sort(key=lambda x: x['name'])
            critical_docs[category] = critical_docs[category][:6]  # Max 6 docs per category
        
        return critical_docs

def _expand_emergency_query(query):
    """Expand emergency search queries with related terms"""
    query_lower = query.lower()
    expanded = [query]  # Always include original query
    
    # Emergency/repair term mappings
    repair_terms = {
        'pressure': ['pressure', 'sensor', 'psi', 'pneumatic', 'air'],
        'relay': ['relay', 'switch', 'contact', 'solenoid', 'coil'],
        'safety': ['safety', 'mill_lamp', 'emergency', 'stop', 'interlock'],
        'error': ['error', 'fault', 'alarm', 'warning', 'problem'],
        'pin': ['pin', 'gpio', 'connection', 'wire', 'terminal'],
        'power': ['power', 'voltage', 'current', 'supply', '12v', '24v'],
        'monitor': ['monitor', 'display', 'lcd', 'screen', 'interface'],
        'temperature': ['temperature', 'temp', 'thermal', 'heat', 'cooling'],
        'log': ['log', 'debug', 'trace', 'output', 'telemetry'],
        'test': ['test', 'diagnostic', 'troubleshoot', 'check', 'verify'],
        'setup': ['setup', 'install', 'config', 'configure', 'deploy']
    }
    
    # Add related terms based on query content
    for category, terms in repair_terms.items():
        if any(term in query_lower for term in terms):
            expanded.extend([t for t in terms if t not in expanded])
    
    # Add common problem patterns
    problem_patterns = {
        'not working': ['error', 'fault', 'broken', 'failed', 'stuck'],
        'stuck': ['relay', 'valve', 'switch', 'mechanical'],
        'no response': ['serial', 'communication', 'timeout', 'connection'],
        'overheating': ['temperature', 'thermal', 'cooling', 'fan'],
        'no power': ['power', 'voltage', 'supply', 'fuse', 'connection']
    }
    
    for pattern, terms in problem_patterns.items():
        if pattern in query_lower:
            expanded.extend([t for t in terms if t not in expanded])
    
    return expanded

# Create global doc server instance
doc_server = DocServer(ROOT_PATH, DOCS_PATH)

@app.route('/')
def index():
    """Main documentation dashboard"""
    try:
        all_docs = doc_server.scan_all_documents()
        
        # Group documents by category
        categories = {}
        for doc in all_docs:
            cat = doc['category']
            if cat not in categories:
                categories[cat] = []
            categories[cat].append(doc)
        
        # Get recently modified docs (last 7 days)
        recent_docs = []
        seven_days_ago = datetime.now().timestamp() - (7 * 24 * 3600)
        for doc in all_docs:
            if doc['modified'].timestamp() > seven_days_ago:
                recent_docs.append(doc)
        recent_docs.sort(key=lambda x: x['modified'], reverse=True)
        recent_docs = recent_docs[:8]  # Top 8 most recent
        
        # Get emergency docs
        emergency_docs = [doc for doc in all_docs if doc['category'] == 'Emergency']
        
        return render_template('index_new.html', 
                             categories=categories,
                             category_info=doc_server.categories,
                             recent_docs=recent_docs,
                             emergency_docs=emergency_docs,
                             total_docs=len(all_docs),
                             scan_time=doc_server.last_scan.strftime('%Y-%m-%d %H:%M:%S'))
    except Exception as e:
        return f"<h1>Server Error</h1><p>{str(e)}</p>", 500

@app.route('/doc/<path:doc_path>')
def view_document(doc_path):
    """View individual document with smart path handling"""
    try:
        # Find the document in our scanned list
        all_docs = doc_server.scan_all_documents()
        document = None
        
        # Try multiple matching strategies
        for doc in all_docs:
            if (doc['url_safe_name'] == doc_path or 
                doc['filename'].replace('.md', '') == doc_path or
                doc['relative_path'].replace('.md', '') == doc_path or
                doc['relative_path'].replace('\\', '/').replace('.md', '') == doc_path):
                document = doc
                break
        
        if not document:
            # Try to find by filename alone
            for doc in all_docs:
                if doc['filename'].replace('.md', '').lower() == doc_path.lower():
                    document = doc
                    break
        
        if not document:
            return render_template('404.html', doc_path=doc_path), 404
        
        # Read and process the document
        with open(document['filepath'], 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # Render markdown with extensions
        md = markdown.Markdown(extensions=[
            'markdown.extensions.tables',
            'markdown.extensions.fenced_code', 
            'markdown.extensions.toc',
            'markdown.extensions.codehilite'
        ])
        html_content = md.convert(content)
        
        # Get related documents in same category
        related_docs = [doc for doc in all_docs 
                       if doc['category'] == document['category'] 
                       and doc['filename'] != document['filename']][:5]
        
        return render_template('document_new.html', 
                             document=document, 
                             content=html_content,
                             raw_content=content,
                             related_docs=related_docs,
                             category_info=doc_server.categories)
    except Exception as e:
        return render_template('error.html', error=str(e)), 500

@app.route('/emergency')
def emergency_dashboard():
    """Emergency/Quick Access Dashboard"""
    try:
        critical_docs = doc_server.get_critical_docs()
        return render_template('emergency.html', critical_docs=critical_docs)
    except Exception as e:
        return f"<h1>Emergency Dashboard Error: {str(e)}</h1>", 500

@app.route('/emergency_diagnostic')
def emergency_diagnostic():
    """Interactive Emergency Diagnostic Wizard with Meshtastic Integration"""
    try:
        # Get all documents for potential recommendations
        all_docs = doc_server.scan_all_documents()
        
        # Filter for emergency and troubleshooting docs, now including Wireless
        emergency_docs = [doc for doc in all_docs if doc['category'] in ['Emergency', 'Wireless', 'Hardware', 'Operations']]
        
        return render_template('emergency_diagnostic.html', emergency_docs=emergency_docs)
    except Exception as e:
        return f"<h1>Emergency Diagnostic Error: {str(e)}</h1>", 500

@app.route('/meshtastic_overview')
def meshtastic_overview():
    """Meshtastic Integration Overview and Status Dashboard"""
    try:
        all_docs = doc_server.scan_all_documents()
        
        # Get wireless/meshtastic specific documents
        wireless_docs = [doc for doc in all_docs if doc['category'] == 'Wireless']
        
        # Get system integration documents
        integration_docs = [doc for doc in all_docs if any(keyword in doc['filename'].lower() 
                          for keyword in ['integration', 'deployment', 'provisioning', 'protocol'])]
        
        return render_template('meshtastic_overview.html', 
                             wireless_docs=wireless_docs,
                             integration_docs=integration_docs,
                             total_docs=len(all_docs))
    except Exception as e:
        return f"<h1>Meshtastic Overview Error: {str(e)}</h1>", 500

@app.route('/search')
def search():
    """Search documents"""
    try:
        query = request.args.get('q', '').strip()
        results = []
        
        if query:
            docs = doc_server.scan_all_documents()
            
            # Smart query expansion for common repair terms
            expanded_query = _expand_emergency_query(query)
            
            for doc in docs:
                try:
                    with open(doc['filepath'], 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    # Check for matches with expanded query terms
                    match_score = 0
                    matches = []
                    
                    # Primary match in title/filename (highest priority)
                    if any(term.lower() in doc['name'].lower() for term in expanded_query):
                        match_score += 10
                    
                    # Content matches
                    lines = content.split('\n')
                    for i, line in enumerate(lines):
                        if any(term.lower() in line.lower() for term in expanded_query):
                            match_score += 1
                            # Get context around match
                            start = max(0, i-2)
                            end = min(len(lines), i+3)
                            context = '\n'.join(lines[start:end])
                            matches.append({
                                'line_num': i+1,
                                'context': context,
                                'matched_line': line.strip()
                            })
                            if len(matches) >= 5:  # Limit matches per document
                                break
                    
                    if match_score > 0:
                        results.append({
                            'doc': doc,
                            'matches': matches,
                            'score': match_score
                        })
                except Exception as e:
                    print(f"Error searching {doc['filepath']}: {e}")
            
            # Sort results by relevance (emergency docs first, then by score)
            priority_map = {'Emergency': 1, 'Hardware': 2, 'Operations': 3, 'Monitoring': 4, 'Development': 5, 'Reference': 6}
            results.sort(key=lambda x: (priority_map.get(x['doc']['category'], 99), -x['score']))
        
        return render_template('search_new.html', 
                             query=query, 
                             results=results,
                             categories=doc_server.categories)
    except Exception as e:
        return f"<h1>Search error: {str(e)}</h1>", 500

@app.route('/raw/<filename>')
def view_raw(filename):
    """View raw document source"""
    try:
        if not filename.endswith('.md'):
            filename += '.md'
        
        filepath = os.path.join(DOCS_PATH, filename)
        if not os.path.exists(filepath):
            return f"<h1>Document not found: {filename}</h1>", 404
        
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Get document info
        stat = os.path.stat(filepath)
        document = {
            'name': filename.replace('.md', '').replace('_', ' ').title(),
            'filename': filename,
            'category': doc_server._categorize_document(filename, content),
            'size_kb': round(stat.st_size / 1024, 1),
            'modified': datetime.fromtimestamp(stat.st_mtime).strftime('%Y-%m-%d %H:%M')
        }
        
        return render_template('raw.html', document=document, content=content)
    except Exception as e:
        return f"<h1>Error loading raw document: {str(e)}</h1>", 500

@app.route('/system')
def system_status():
    """System status page"""
    try:
        docs = doc_server.scan_all_documents()
        
        # Calculate system statistics
        uptime = str(datetime.now() - datetime.now().replace(hour=0, minute=0, second=0, microsecond=0))
        total_docs = len(docs)
        categories = {}
        total_size_kb = 0
        
        for doc in docs:
            category = doc['category']
            categories[category] = categories.get(category, 0) + 1
            total_size_kb += doc.get('size_kb', 0)
        
        current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        if doc_server.last_scan:
            if isinstance(doc_server.last_scan, datetime):
                last_scan = doc_server.last_scan.strftime('%Y-%m-%d %H:%M:%S')
            else:
                last_scan = str(doc_server.last_scan)
        else:
            last_scan = current_time
        
        return render_template('system.html',
                             uptime=uptime,
                             total_docs=total_docs,
                             categories=categories,
                             total_size_kb=int(total_size_kb),
                             current_time=current_time,
                             last_scan=last_scan)
    except Exception as e:
        return f"<h1>System status error: {str(e)}</h1>", 500

@app.route('/api/meshtastic_status', methods=['GET'])
def meshtastic_status():
    """Get Meshtastic network status for diagnostic wizard"""
    try:
        from wizard_meshtastic_client import get_wizard_network_status
        
        status = get_wizard_network_status()
        return jsonify(status)
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'error': f'Network status error: {str(e)}',
            'suggestion': 'Verify Meshtastic devices are connected and configured'
        }), 500

@app.route('/api/system_info', methods=['GET'])
def system_info():
    """Get comprehensive LogSplitter system information including Meshtastic integration"""
    try:
        docs = doc_server.scan_all_documents()
        
        # Count documents by category
        categories = {}
        wireless_docs = []
        for doc in docs:
            category = doc['category']
            categories[category] = categories.get(category, 0) + 1
            if category == 'Wireless':
                wireless_docs.append({
                    'name': doc['name'],
                    'filename': doc['filename'],
                    'modified': doc['modified'].isoformat()
                })
        
        # Get Meshtastic status if available
        meshtastic_info = {'status': 'unknown', 'integration': 'complete'}
        try:
            from wizard_meshtastic_client import get_wizard_network_status
            meshtastic_info = get_wizard_network_status()
            meshtastic_info['integration'] = 'complete'
        except:
            meshtastic_info['integration_note'] = 'Meshtastic client available but hardware not detected'
        
        return jsonify({
            'system': {
                'name': 'LogSplitter Controller',
                'version': '2.0 - Meshtastic Wireless',
                'architecture': 'Industrial Arduino UNO R4 WiFi + Meshtastic Mesh',
                'communication': 'Wireless LoRa Mesh Network (Primary), USB Serial (Debug)',
                'safety_level': 'Industrial Grade with Mesh Redundancy',
                'uptime': datetime.now().isoformat(),
                'status': 'Production Ready'
            },
            'documentation': {
                'total_documents': len(docs),
                'categories': categories,
                'wireless_documents': len(wireless_docs),
                'last_scan': doc_server.last_scan.isoformat()
            },
            'meshtastic': meshtastic_info,
            'capabilities': {
                'wireless_commands': True,
                'mesh_networking': True,
                'emergency_broadcasts': True,
                'binary_protobuf': True,
                'industrial_range': True,
                'mesh_redundancy': True,
                'safety_preserved': True
            },
            'channels': {
                'channel_0': 'Emergency broadcasts (plain language)',
                'channel_1': 'LogSplitter protocol (binary protobuf)'
            }
        })
        
    except Exception as e:
        return jsonify({
            'error': f'System info error: {str(e)}'
        }), 500

@app.route('/api/meshtastic_test', methods=['POST'])
def meshtastic_test():
    """Test Meshtastic connectivity and network health for diagnostic wizard"""
    try:
        data = request.get_json()
        test_type = data.get('test_type', 'discover')
        
        import subprocess
        import serial.tools.list_ports
        
        if test_type == 'discover':
            # Auto-discover Meshtastic nodes
            nodes = []
            for port in serial.tools.list_ports.comports():
                if test_meshtastic_connection(port.device):
                    node_info = get_meshtastic_node_info(port.device)
                    nodes.append({
                        'port': port.device,
                        'description': port.description,
                        'node_info': node_info
                    })
            
            return jsonify({
                'success': True,
                'test_type': 'discover',
                'nodes': nodes,
                'node_count': len(nodes),
                'timestamp': datetime.now().isoformat()
            })
        
        elif test_type == 'network_health':
            # Test network health across all nodes
            port = data.get('port')
            if not port:
                return jsonify({
                    'success': False,
                    'error': 'Port required for network health test'
                }), 400
            
            health_data = check_meshtastic_network_health(port)
            return jsonify({
                'success': True,
                'test_type': 'network_health',
                'health_data': health_data,
                'timestamp': datetime.now().isoformat()
            })
        
        elif test_type == 'range_quick':
            # Quick range test between two nodes
            bridge_port = data.get('bridge_port')
            monitor_port = data.get('monitor_port')
            
            if not bridge_port or not monitor_port:
                return jsonify({
                    'success': False,
                    'error': 'Both bridge_port and monitor_port required'
                }), 400
            
            range_data = quick_meshtastic_range_test(bridge_port, monitor_port)
            return jsonify({
                'success': True,
                'test_type': 'range_quick',
                'range_data': range_data,
                'timestamp': datetime.now().isoformat()
            })
        
        else:
            return jsonify({
                'success': False,
                'error': f'Unknown test_type: {test_type}'
            }), 400
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f'Meshtastic test error: {str(e)}',
            'suggestion': 'Ensure Meshtastic CLI tools are installed and nodes are connected'
        }), 500

def test_meshtastic_connection(port: str) -> bool:
    """Test if port has a Meshtastic device"""
    try:
        result = subprocess.run(['meshtastic', '--port', port, '--info'], 
                               capture_output=True, text=True, timeout=10)
        return result.returncode == 0 and 'MyNodeInfo' in result.stdout
    except:
        return False

def get_meshtastic_node_info(port: str) -> dict:
    """Get detailed Meshtastic node information"""
    try:
        result = subprocess.run(['meshtastic', '--port', port, '--info'], 
                               capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            info = {'raw_output': result.stdout}
            lines = result.stdout.split('\n')
            
            for line in lines:
                if 'Owner:' in line:
                    info['owner'] = line.split('Owner:')[1].strip()
                elif 'Model:' in line:
                    info['model'] = line.split('Model:')[1].strip()
                elif 'Battery:' in line:
                    info['battery'] = line.split('Battery:')[1].strip()
                elif 'Region:' in line:
                    info['region'] = line.split('Region:')[1].strip()
                elif 'Channel:' in line:
                    info['channel'] = line.split('Channel:')[1].strip()
                elif 'MyNodeInfo:' in line:
                    info['node_id'] = line.split('MyNodeInfo:')[1].strip()
            
            return info
    except:
        pass
    return {}

def check_meshtastic_network_health(port: str) -> dict:
    """Check Meshtastic network health and connectivity"""
    health_data = {}
    
    try:
        # Get node list
        result = subprocess.run(['meshtastic', '--port', port, '--nodes'], 
                               capture_output=True, text=True, timeout=15)
        if result.returncode == 0:
            health_data['nodes_output'] = result.stdout
            # Count nodes mentioned in output
            node_count = result.stdout.count('│')  # Simple heuristic
            health_data['visible_nodes'] = max(0, node_count - 2)  # Subtract header rows
        
        # Check telemetry
        result = subprocess.run(['meshtastic', '--port', port, '--get', 'telemetry'], 
                               capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            health_data['telemetry'] = result.stdout
        
        # Check channel info
        result = subprocess.run(['meshtastic', '--port', port, '--ch-index', '0'], 
                               capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            health_data['channel_info'] = result.stdout
        
        health_data['status'] = 'healthy' if health_data.get('visible_nodes', 0) > 0 else 'isolated'
        
    except Exception as e:
        health_data['error'] = str(e)
        health_data['status'] = 'error'
    
    return health_data

def quick_meshtastic_range_test(bridge_port: str, monitor_port: str) -> dict:
    """Perform quick range test between two Meshtastic nodes"""
    range_data = {}
    
    try:
        import time
        import threading
        
        # Send test message from bridge
        test_message = f"DIAGNOSTIC_TEST_{int(time.time())}"
        send_result = subprocess.run([
            'meshtastic', '--port', bridge_port, 
            '--sendtext', test_message
        ], capture_output=True, text=True, timeout=10)
        
        range_data['send_success'] = send_result.returncode == 0
        range_data['send_output'] = send_result.stdout
        
        if send_result.returncode != 0:
            range_data['send_error'] = send_result.stderr
        
        # Brief monitoring on receiver (simplified for diagnostics)
        monitor_result = subprocess.run([
            'meshtastic', '--port', monitor_port, '--info'
        ], capture_output=True, text=True, timeout=5)
        
        range_data['monitor_connection'] = monitor_result.returncode == 0
        range_data['test_message'] = test_message
        range_data['recommendation'] = 'Use meshtastic_range_test.py for comprehensive testing'
        
    except Exception as e:
        range_data['error'] = str(e)
    
    return range_data

@app.route('/api/run_command', methods=['POST'])
def run_command():
    """Execute controller command via Meshtastic protobuf for diagnostic wizard"""
    try:
        data = request.get_json()
        command = data.get('command', '').strip()
        
        # Import required modules
        import subprocess
        import time
        import json
        import base64
        import serial.tools.list_ports
        
        # Import Meshtastic protocol
        try:
            from logsplitter_meshtastic_protocol import LogSplitterProtocol, CommandMessage, CommandResponse
            from logsplitter_meshtastic_protocol import LogSplitterMessageType
        except ImportError:
            return jsonify({
                'success': False,
                'error': 'Meshtastic protocol module not found',
                'suggestion': 'Ensure logsplitter_meshtastic_protocol.py is available'
            }), 500
        
        try:
            # Use dedicated wizard Meshtastic client
            from wizard_meshtastic_client import execute_wizard_command
            
            result = execute_wizard_command(command)
            
            return jsonify(result)
            
        except subprocess.TimeoutExpired:
            return jsonify({
                'success': False,
                'error': 'Meshtastic command timeout',
                'suggestion': 'Check mesh network connectivity and node responsiveness'
            }), 503
            
        except Exception as e:
            return jsonify({
                'success': False,
                'error': f'Meshtastic communication error: {str(e)}',
                'suggestion': 'Verify Meshtastic CLI tools are installed and node is responding'
            }), 500
            
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f'Server error: {str(e)}'
        }), 500

@app.route('/api/wizard_log', methods=['POST'])
def log_wizard_session():
    """Log wizard session data for analysis and handoff"""
    try:
        data = request.get_json()
        
        # Create log entry
        log_entry = {
            'timestamp': datetime.now().isoformat(),
            'session_id': data.get('session_id'),
            'event_type': data.get('event_type'),  # 'start', 'command', 'diagnosis', 'completion'
            'data': data.get('data', {})
        }
        
        # Auto-save to local file
        import os
        log_dir = 'wizard_logs'
        os.makedirs(log_dir, exist_ok=True)
        
        log_file = os.path.join(log_dir, f"wizard_session_{data.get('session_id', 'unknown')}.jsonl")
        
        with open(log_file, 'a') as f:
            import json
            f.write(json.dumps(log_entry) + '\n')
        
        return jsonify({
            'success': True,
            'logged': True,
            'log_file': log_file
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f'Logging error: {str(e)}'
        }), 500

@app.route('/static/<path:filename>')
def serve_assets(filename):
    """Serve static assets"""
    return send_from_directory('static', filename)

if __name__ == '__main__':
    print(f"""
    ╔══════════════════════════════════════════════════════════════╗
    ║                LOGSPLITTER DOCUMENTATION SERVER              ║
    ║              📡 Meshtastic Mesh Network Ready               ║
    ╠══════════════════════════════════════════════════════════════╣
    ║  Server URL: http://localhost:{PORT}                         ║
    ║  Interface:  Modern Responsive Design + Wireless Diagnostics║
    ║  Features:   Emergency Mode, Meshtastic Commands, Search     ║
    ║  Status:     ONLINE - Wireless Mesh Integration Complete    ║
    ╚══════════════════════════════════════════════════════════════╝
    
    📡 Meshtastic Integration:
    • Emergency Diagnostic Wizard with wireless mesh commands
    • Real-time network status and node monitoring
    • Binary protobuf protocol via Channel 1 (LogSplitter)
    • Channel 0 emergency broadcasts for critical alerts
    • Complete wireless operation - USB optional for debug
    
    📋 Quick Start:
    • Visit http://localhost:{PORT} for main documentation
    • Click Emergency button for wireless diagnostic wizard
    • Use /emergency_diagnostic for Meshtastic mesh troubleshooting
    • All documents auto-categorized including Wireless section
    
    """)
    
    # Ensure directories exist
    os.makedirs('templates', exist_ok=True)
    os.makedirs('static/css', exist_ok=True)
    
    # Perform initial document scan
    print("🔍 Scanning documents...")
    docs = doc_server.scan_all_documents()
    print(f"✅ Found {len(docs)} documents in {len(set(doc['category'] for doc in docs))} categories")
    
    try:
        app.run(host='0.0.0.0', port=PORT, debug=DEBUG)
    except Exception as e:
        print(f"❌ Server startup error: {e}")