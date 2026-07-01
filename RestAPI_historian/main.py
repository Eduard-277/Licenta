import asyncio
import json
import warnings
import re
from datetime import datetime, time, timedelta
from typing import List, Dict

import httpx
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse
from pydantic import BaseModel

# Latest Google GenAI SDK
from google import genai
from google.genai import types

# =============================================================================
# 1. CONFIGURATION
# =============================================================================
TIMEBASE_URL = "https://localhost:4512"
GEMINI_API_KEY = "" 

client = genai.Client(api_key=GEMINI_API_KEY)
MODEL_ID = "gemini-3.1-flash-lite" 

LATEST_VALUES: Dict[str, dict] = {}

MONITORED_TAGS = [
    {"elementId": "default:Car/Controller/dpad/down_req", "unit": ""},
    {"elementId": "default:Car/Controller/dpad/left_req", "unit": ""},
    {"elementId": "default:Car/Controller/dpad/right_req", "unit": ""},
    {"elementId": "default:Car/Controller/dpad/up_req", "unit": ""},
    {"elementId": "default:Car/Controller/electromagnet_req", "unit": ""},
    {"elementId": "default:Car/Controller/load_req", "unit": ""},
    {"elementId": "default:Car/Controller/remote_req", "unit": ""},
    {"elementId": "default:Car/Controller/speed_req", "unit": ""},
    {"elementId": "default:Car/Controller/status", "unit": ""},
    {"elementId": "default:Car/Controller/unload_req", "unit": ""},
    {"elementId": "default:Car/conn_req_timeout", "unit": "s"},
    {"elementId": "default:Car/fault", "unit": ""},
    {"elementId": "default:Car/movement", "unit": ""},
    {"elementId": "default:Crane/Arduino/calibration", "unit": ""},
    {"elementId": "default:Crane/Arduino/weight", "unit": "g"},
    {"elementId": "default:Crane/Control/down", "unit": ""},
    {"elementId": "default:Crane/Control/left", "unit": ""},
    {"elementId": "default:Crane/Control/right", "unit": ""},
    {"elementId": "default:Crane/Control/up", "unit": ""},
    {"elementId": "default:Crane/Detectors/left", "unit": ""},
    {"elementId": "default:Crane/Detectors/right", "unit": ""},
    {"elementId": "default:Crane/Motors/Horizontal/Specified/distance", "unit": "pulses"},
    {"elementId": "default:Crane/Motors/Horizontal/Specified/movement", "unit": ""},
    {"elementId": "default:Crane/Motors/Horizontal/actual_position", "unit": "pulses"},
    {"elementId": "default:Crane/Motors/Horizontal/continuous", "unit": ""},
    {"elementId": "default:Crane/Motors/Horizontal/limits", "unit": ""},
    {"elementId": "default:Crane/Motors/Horizontal/power", "unit": ""},
    {"elementId": "default:Crane/Motors/Horizontal/velocity", "unit": "pulses/s"},
    {"elementId": "default:Crane/Motors/Vertical/Specified/distance", "unit": "pulses"},
    {"elementId": "default:Crane/Motors/Vertical/Specified/movement", "unit": ""},
    {"elementId": "default:Crane/Motors/Vertical/actual_position", "unit": "pulses"},
    {"elementId": "default:Crane/Motors/Vertical/continuous", "unit": ""},
    {"elementId": "default:Crane/Motors/Vertical/limits", "unit": ""},
    {"elementId": "default:Crane/Motors/Vertical/power", "unit": ""},
    {"elementId": "default:Crane/Motors/Vertical/velocity", "unit": "pulses/s"},
    {"elementId": "default:Crane/allow_remote", "unit": ""},
    {"elementId": "default:Crane/cargo", "unit": ""},
    {"elementId": "default:Crane/electromagnet", "unit": ""},
    {"elementId": "default:Crane/fault", "unit": ""},
    {"elementId": "default:Crane/process_status", "unit": ""},
    {"elementId": "default:Crane/speed_mode", "unit": ""}
]

# =============================================================================
# 2. AI TOOLING
# =============================================================================

async def get_multi_tag_history_data(element_ids: List[str], start_time: str, end_time: str):
    """Internal helper to fetch data from the historian API."""
    if not start_time.endswith('Z'): start_time += 'Z'
    if not end_time.endswith('Z'): end_time += 'Z'
    payload = {"elementIds": element_ids, "startTime": start_time, "endTime": end_time}
    async with httpx.AsyncClient(verify=False) as client_http:
        try:
            r = await client_http.post(f"{TIMEBASE_URL}/i3x/objects/history", json=payload)
            return r.json()
        except Exception as e: return {"error": str(e)}

# =============================================================================
# 3. FASTAPI SETUP & FRONTEND
# =============================================================================
app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

HTML_PAGE = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Historical & Informational Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/hammerjs@2.0.8"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.1"></script>
    <style>
        /* Reset & Base */
        body {{ margin: 0; display: flex; height: 100vh; overflow: hidden; font-family: 'Segoe UI', Tahoma, sans-serif; background: #000; color: #fff; }}
        
        /* Sidebar Menu - Matching Ignition docked view Size: 300 */
        .sidebar {{ width: 300px; background: #555555; display: flex; flex-direction: column; border-right: 2px solid #333; box-sizing: border-box; }}
        .sidebar .menu-header {{ background: #000; color: #fff; padding: 25px 30px; font-size: 55px; font-weight: bold; text-align: left; font-family: Arial }}
        
        /* Sidebar Buttons Container - Distributes space evenly exactly like perspective flex containers */
        .sidebar-buttons {{ display: flex; flex-direction: column; justify-content: space-evenly; align-items: center; flex: 1; padding: 10px 0 30px 0; }}
        
        /* Sidebar Buttons */
        .sidebar .menu-btn {{ 
            display: flex;
            align-items: center;
            justify-content: center;
            width: 65%; /* Constrains the width to match the mockups */
            height: 55px; /* Explicit height matching the SCADA buttons */
            background: #808080; 
            color: #fff; 
            text-decoration: none; 
            font-size: 24px; 
            font-weight: bold; 
            border-radius: 3px;
            box-shadow: 1px 1px 4px rgba(0,0,0,0.4);
            transition: background 0.2s; 
        }}
        .sidebar .menu-btn:hover {{ background: #999999; }}
        
        /* Main Layout */
        .main-content {{ flex: 1; display: flex; flex-direction: column; background: #000; min-width: 0; }}
        .top-header {{ background: #000; color: #fff; padding: 29px 30px; font-size: 40px; font-weight: bold; font-style: italic; letter-spacing: 1px; border-bottom: 2px solid #222; }}
        .dashboard-area {{ flex: 1; background: #d9d9d9; padding: 25px; overflow: hidden; display: flex; flex-direction: column; }}
        
        /* Layout inside dashboard */
        .main-layout {{ display: flex; gap: 25px; height: 100%; min-height: 0; }}
        .data-section {{ flex: 3; overflow-y: auto; height: 100%; padding-right: 15px; box-sizing: border-box; }}
        
        /* Data Folders - Stronger borders and shadows */
        .folder-section {{ background: #595959; border-radius: 4px; border: 2px solid #222; padding: 12px; margin-bottom: 15px; box-shadow: 3px 3px 6px rgba(0,0,0,0.4); }}
        .folder-header {{ font-weight: bold; color: #fff; text-transform: uppercase; font-size: 15px; letter-spacing: 0.5px; cursor: pointer; display: flex; align-items: center; user-select: none; padding: 10px; border-radius: 2px; transition: background 0.2s; background: rgba(0,0,0,0.2); border-bottom: 1px solid #333; }}
        .folder-header:hover {{ background: rgba(0,0,0,0.3); }}
        .folder-header::before {{ content: '▼'; margin-right: 10px; font-size: 12px; transition: transform 0.2s; display: inline-block; color: #fff; }}
        .collapsed > .folder-header::before {{ transform: rotate(-90deg); }}
        .folder-content {{ display: block; margin-top: 15px; overflow: hidden; padding: 0 5px; }}
        .collapsed > .folder-content {{ display: none; }}
        .sub-folder {{ border-left: 5px solid #999; margin-left: 10px; padding-left: 12px; background: #4f4f4f; border-top: 2px solid #222; border-right: 2px solid #222; border-bottom: 2px solid #222;}}
        .tag-grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 15px; padding-bottom: 8px; }}
        
        /* Cards - Thicker border and better contrast */
        .card {{ background: #e6e6e6; padding: 12px; border-radius: 4px; border: 2px solid #888; cursor: pointer; transition: 0.2s; box-shadow: 2px 2px 4px rgba(0,0,0,0.2); }}
        .card:hover {{ border-color: #e60000; background: #fff; transform: translateY(-1px); box-shadow: inset 1px 1px 3px rgba(0,0,0,0.05), 0 2px 5px rgba(230,0,0,0.2); }}
        .tag-label {{ font-size: 12px; color: #000; font-weight: bold; margin-bottom: 5px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; text-transform: uppercase; }}
        .val {{ font-size: 24px; font-weight: bold; color: #000; }}
        
        /* AI Section - Matching dark panels */
        .ai-section {{ flex: 1; background: #595959; padding: 20px; border-radius: 0; box-shadow: 2px 2px 5px rgba(0,0,0,0.2); display: flex; flex-direction: column; height: 100%; box-sizing: border-box; border: 1px solid #444; }}
        .ai-section h3 {{ color: #fff; text-transform: uppercase; margin-top: 0; border-bottom: 1px solid #777; padding-bottom: 10px; }}
        #chat {{ flex: 1; overflow-y: auto; background: #e6e6e6; padding: 15px; border-radius: 2px; border: inset 2px #b3b3b3; margin-bottom: 15px; font-size: 14px; line-height: 1.5; color: #000; }}
        .thinking {{ display: none; font-size: 12px; color: #fff; font-style: italic; margin-bottom: 10px; font-weight: bold; }}
        
        /* Chat message styling */
        .msg-user {{ background: #ccc; padding: 10px; border-radius: 4px; border: 1px solid #aaa; margin-bottom: 10px; font-weight: 500; }}
        .msg-ai {{ background: #fff; color: #000; padding: 12px; border-radius: 4px; margin-bottom: 15px; border-left: 5px solid #e60000; box-shadow: 0 1px 4px rgba(0,0,0,0.15); }}
        
        input {{ width: 100%; padding: 15px; border: 2px solid #777; border-radius: 2px; box-sizing: border-box; outline: none; font-size: 16px; background: #fff; color: #000; }}
        button {{ width: 100%; padding: 15px; background: #4f4f4f; color: white; border: 2px outset #777; cursor: pointer; margin-top: 15px; border-radius: 2px; font-weight: bold; font-size: 18px; text-transform: uppercase; transition: background 0.2s; box-shadow: 2px 2px 5px rgba(0,0,0,0.2); }}
        button:hover {{ background: #666; }}
        button:active {{ border-style: inset; box-shadow: inset 2px 2px 5px rgba(0,0,0,0.3); }}
        
        /* Modal for History */
        #historyModal {{ display: none; position: fixed; z-index: 2000; left: 0; top: 0; width: 100%; height: 100%; background: rgba(0, 0, 0, 0.7); backdrop-filter: blur(2px); }}
        .modal-content {{ background: #d9d9d9; margin: 3% auto; padding: 25px; border-radius: 2px; border: 2px solid #5a5a5a; width: 90%; max-width: 1100px; height: 80vh; display: flex; flex-direction: column; position: relative; color: #000; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }}
        .modal-content h2 {{ color: #000; margin-top: 0; border-bottom: 2px solid #aaa; padding-bottom: 10px; text-transform: uppercase; }}
        .chart-controls {{ display: flex; gap: 15px; align-items: center; margin-bottom: 20px; background: #e6e6e6; padding: 12px; border-radius: 2px; border: 1px solid #aaa; font-weight: bold; }}
        .chart-controls input {{ width: auto; padding: 8px; font-size: 14px; font-weight: normal; }}
        .chart-controls button {{ margin: 0; padding: 10px 20px; width: auto; font-size: 14px; background: #4f4f4f; border: 2px outset #777; }}
        .chart-controls button:hover {{ background: #666; }}
        .chart-controls button:active {{ border-style: inset; }}
    </style>
</head>
<body>
    
    <div class="sidebar">
        <div class="menu-header">Menu</div>
        <div class="sidebar-buttons">
            <a href="http://localhost:8088/data/perspective/client/Licenta/crane" class="menu-btn">Crane</a>
            <a href="http://localhost:8088/data/perspective/client/Licenta/car" class="menu-btn">Car</a>
            <a href="http://localhost:8088/data/perspective/client/Licenta/service" class="menu-btn">Service</a>
            <a href="/" class="menu-btn">Historian</a>
            <a href="http://localhost:8088/data/perspective/client/Licenta/alarms" class="menu-btn">Alarms</a>
        </div>
    </div>

    <div class="main-content">
        <div class="top-header">Historian</div>
        
        <div class="dashboard-area">
            <div class="main-layout">
                <div id="data-root" class="data-section"></div>
                
                <div class="ai-section">
                    <h3>Gemini Intelligence</h3>
                    <div id="chat"></div>
                    <div id="thinking" class="thinking">AI is calculating...</div>
                    <input id="msg" placeholder="QUERY FOR PROCESS DATA..." onkeypress="if(event.key === 'Enter') askAI()">
                    <button onclick="askAI()">Query System</button>
                </div>
            </div>
        </div>
    </div>

    <div id="historyModal">
        <div class="modal-content">
            <h2 id="modalTitle">Tag History</h2>
            <div class="chart-controls">
                <label style="color:#000; font-weight:bold;">Start:</label>
                <input type="datetime-local" id="startDate">
                <label style="color:#000; font-weight:bold;">End:</label>
                <input type="datetime-local" id="endDate">
                <button onclick="loadHistoryChart()">Update Chart</button>
                <button onclick="resetZoom()">Reset Zoom</button>
            </div>
            <div style="flex: 1; position: relative; min-height: 0;">
                <canvas id="historyChart"></canvas>
            </div>
        </div>
    </div>
    <script>
        const fullTags = {json.dumps(MONITORED_TAGS)};
        const unitsMap = {{}};
        fullTags.forEach(t => unitsMap[t.elementId] = t.unit);

        let myChart = null, currentTag = "";

        function buildTree(tags) {{
            const tree = {{}};
            tags.forEach(tagObj => {{
                const id = tagObj.elementId;
                const path = id.replace('default:', '').split('/');
                let current = tree;
                path.forEach((part, index) => {{
                    if (index === path.length - 1) {{
                        current[part] = {{ _id: id, _unit: tagObj.unit, _isLeaf: true }};
                    }} else {{
                        if (!current[part]) current[part] = {{}};
                        current = current[part];
                    }}
                }});
            }});
            return tree;
        }}

        function renderTree(node, container) {{
            const contentWrapper = document.createElement('div');
            contentWrapper.className = 'folder-content';
            container.appendChild(contentWrapper);

            for (const key in node) {{
                if (node[key]._isLeaf) {{
                    const tag = node[key];
                    const safeId = tag._id.replace(/[^a-zA-Z0-9]/g, "_");
                    const card = document.createElement('div');
                    card.id = safeId; card.className = 'card';
                    card.onclick = () => openHistory(tag._id);
                    card.innerHTML = `
                        <div class="tag-label" title="${{tag._id}}">${{key}}</div>
                        <div style="display:flex; align-items:baseline; gap:4px;">
                            <div class="val">---</div>
                            <div style="font-size:12px; color:#555; font-weight:bold;">${{tag._unit || ''}}</div>
                        </div>`;
                    
                    let grid = contentWrapper.querySelector(':scope > .tag-grid');
                    if(!grid) {{
                        grid = document.createElement('div');
                        grid.className = 'tag-grid';
                        contentWrapper.appendChild(grid);
                    }}
                    grid.appendChild(card);
                }} else {{
                    const folder = document.createElement('div');
                    folder.className = 'folder-section sub-folder';
                    const header = document.createElement('div');
                    header.className = 'folder-header';
                    header.innerText = key;
                    header.onclick = (e) => {{ e.stopPropagation(); folder.classList.toggle('collapsed'); }};
                    folder.appendChild(header);
                    contentWrapper.appendChild(folder);
                    renderTree(node[key], folder);
                }}
            }}
        }}

        renderTree(buildTree(fullTags), document.getElementById('data-root'));

        const ws = new WebSocket(`ws://${{location.host}}/ws`);
        ws.onmessage = (e) => {{
            JSON.parse(e.data).forEach(tag => {{
                const el = document.getElementById(tag.elementId.replace(/[^a-zA-Z0-9]/g, "_"));
                if(el) el.querySelector('.val').innerText = tag.value !== null ? tag.value : '0';
            }});
        }};

        function openHistory(tagId) {{
            currentTag = tagId;
            document.getElementById('modalTitle').innerText = tagId.split('/').pop();
            document.getElementById('historyModal').style.display = 'block';

            const now = new Date();
            const yest = new Date(now.getTime() - 86400000);

            const formatLocal = (date) => {{
                const offset = date.getTimezoneOffset() * 60000;
                return new Date(date.getTime() - offset).toISOString().slice(0, 19);
            }};

            document.getElementById('endDate').value = formatLocal(now);
            document.getElementById('startDate').value = formatLocal(yest);

            loadHistoryChart();
        }}

        function resetZoom() {{ if(myChart) myChart.resetZoom(); }}
        window.onclick = (e) => {{ if(e.target == document.getElementById('historyModal')) document.getElementById('historyModal').style.display='none'; }};

        async function loadHistoryChart() {{
            const startInput = document.getElementById('startDate').value
            const endInput = document.getElementById('endDate').value;

            const start = new Date(startInput).toISOString();
            const end = new Date(endInput).toISOString();
            
            if(myChart) {{
                myChart.destroy();
                myChart = null;
            }}

            const resp = await fetch(`/get-history?elementId=${{encodeURIComponent(currentTag)}}&start=${{start}}&end=${{end}}`);
            const json = await resp.json();
            let raw = json[currentTag]?.data || [];
            const ctx = document.getElementById('historyChart').getContext('2d');
            const unit = unitsMap[currentTag] || "";

            myChart = new Chart(ctx, {{
                type: 'line',
                data: {{
                    labels: raw.map(p => {{
                        const d = new Date(p.timestamp);
                        const datePart = d.getDate().toString().padStart(2, '0') + "." + (d.getMonth() + 1).toString().padStart(2, '0');
                        const timePart = d.toLocaleTimeString([], {{ hour: '2-digit', minute: '2-digit', second: '2-digit' }});
                        return [datePart, timePart]; 
                    }}),
                    datasets: [{{ 
                        label: currentTag + (unit ? " (" + unit + ")" : ""),
                        data: raw.map(p => p.value), 
                        borderColor: '#e60000', /* SCADA red */
                        backgroundColor: '#e60000',
                        tension: 0.1, 
                        fill: false
                    }}]
                }},
                options: {{ 
                    responsive: true, 
                    maintainAspectRatio: false,
                    plugins: {{ 
                        zoom: {{ 
                            zoom: {{ wheel: {{ enabled: true }}, mode: 'x' }}, 
                            pan: {{ enabled: true, mode: 'x' }} 
                        }} 
                    }},
                    scales: {{ 
                        x: {{ ticks: {{ autoSkip: true, maxTicksLimit: 10, maxRotation: 0, color: '#000' }} }},
                        y: {{ 
                            ticks: {{ color: '#000' }},
                            title: {{ display: unit !== "", text: unit, font: {{ weight: 'bold' }}, color: '#000' }} 
                        }}
                    }}
                }}
            }});
        }}

        function formatAIText(text) {{
            text = text.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
            text = text.replace(/\\n/g, '<br>');
            return text;
        }}

        async function askAI() {{
            const m = document.getElementById('msg'), chat = document.getElementById('chat'), think = document.getElementById('thinking');
            if(!m.value) return;
            
            const txt = m.value;
            const localTime = new Date().toLocaleString();
            
            chat.innerHTML += `<div class="msg-user"><b>User:</b> ${{txt}}</div>`;
            m.value = '';
            think.style.display = 'block';
            chat.scrollTop = chat.scrollHeight;

            try {{
                const resp = await fetch('/ask-agent', {{ 
                    method: 'POST', 
                    headers: {{'Content-Type':'application/json'}}, 
                    body: JSON.stringify({{
                        message: txt,
                        client_time: localTime 
                    }}) 
                }});
                const data = await resp.json();
                chat.innerHTML += `<div class="msg-ai"><b>AI:</b> ${{formatAIText(data.reply)}}</div>`;
            }} finally {{
                think.style.display = 'none';
                chat.scrollTop = chat.scrollHeight;
            }}
        }}
    </script>
</body>
</html>
"""

# =============================================================================
# 4. BACKEND POLLERS & AI AGENT
# =============================================================================
active_websockets: List[WebSocket] = []

async def historian_snapshot_loop():
    tag_ids = [t["elementId"] for t in MONITORED_TAGS]
    while True:
        try:
            async with httpx.AsyncClient(verify=False, timeout=10) as client_http:
                resp = await client_http.post(f"{TIMEBASE_URL}/i3x/objects/value", json={"elementIds": tag_ids, "maxDepth": 1})
                if resp.status_code == 200:
                    data = resp.json()
                    formatted = []
                    for eid, content in data.items():
                        pts = content.get("data", [])
                        val = pts[-1].get("value") if pts else content.get("value")
                        u = {"elementId": eid, "value": val}
                        LATEST_VALUES[eid] = u
                        formatted.append(u)
                    for ws in active_websockets[:]:
                        try: await ws.send_text(json.dumps(formatted))
                        except: active_websockets.remove(ws)
            await asyncio.sleep(0.1)
        except: await asyncio.sleep(5)

@app.get("/", response_class=HTMLResponse)
async def home(): return HTML_PAGE

@app.get("/get-history")
async def get_history_api(elementId: str, start: str, end: str):
    return await get_multi_tag_history_data([elementId], start, end)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept(); active_websockets.append(websocket)
    if LATEST_VALUES: await websocket.send_text(json.dumps(list(LATEST_VALUES.values())))
    try: 
        while True: await asyncio.sleep(10)
    except WebSocketDisconnect: active_websockets.remove(websocket)

class ChatRequest(BaseModel): 
    message: str
    client_time: str

# GLOBAL VARIABLE TO STORE CONVERSATION MEMORY
GLOBAL_CHAT_HISTORY: List[types.Content] = []

@app.post("/ask-agent")
async def ask_agent(request: ChatRequest):

    live_state = json.dumps(LATEST_VALUES, indent=2)

    # --- PYTHON TIME PARSING ---
    romania_offset = timedelta(hours=3)
    utc_now = datetime.utcnow()
    ro_now = utc_now + romania_offset

    ro_today_start = datetime.combine(ro_now.date(), time.min)
    ro_yesterday_start = ro_today_start - timedelta(days=1)
    ro_yesterday_end = ro_today_start - timedelta(seconds=1)
    ro_last_hour_start = ro_now - timedelta(hours=1)

    bound_today_start = ro_today_start.isoformat()
    bound_yesterday_start = ro_yesterday_start.isoformat()
    bound_yesterday_end = ro_yesterday_end.isoformat()
    bound_last_hour = ro_last_hour_start.isoformat()
    bound_now = ro_now.isoformat()

    dynamic_time_bounds = ""
    match = re.search(r'(\d+)\s+hours?\s+ago', request.message.lower())
    if match:
        hours_ago = int(match.group(1))
        target_ro = ro_now - timedelta(hours=hours_ago)
        target_str = target_ro.isoformat()
        start_str = (target_ro - timedelta(hours=24)).isoformat()
        
        dynamic_time_bounds = f"""
- The user asked about '{hours_ago} hours ago'. Use these EXACT bounds:
  start_time = "{start_str}"
  end_time = "{target_str}"
"""
    # ---------------------------

    system_instruction = f"""
# ROLE
You are an Industrial AI Controller for a factory consisting of a Car and a Crane.
Current User Time (Romania): {request.client_time}

=================================================
TIME HANDLING (CRITICAL)
=================================================
1. The Database, the User, and You all operate in **Romania Local Time**.
2. DO NOT add or subtract hours from any timestamps. 
3. DO NOT convert anything to UTC.
4. Present the timestamps EXACTLY as they appear in the data.

=================================================
PRE-CALCULATED TIME BOUNDS (MANDATORY USE)
=================================================
To prevent calculation errors, use the following pre-calculated boundaries when calling `get_tags_history`:
- If the user asks for "today": start_time = "{bound_today_start}", end_time = "{bound_now}"
- If the user asks for "yesterday": start_time = "{bound_yesterday_start}", end_time = "{bound_yesterday_end}"
- If the user asks for "last hour": start_time = "{bound_last_hour}", end_time = "{bound_now}"
{dynamic_time_bounds}

=================================================
ARBITRARY DATES & TIMES
=================================================
If the user asks for a specific date or time (e.g., "May 30th", "at 14:00") that is NOT listed in the pre-calculated bounds above:
1. Look at the Current User Time to determine the correct year and month.
2. Format your tool arguments as strict ISO8601 strings in LOCAL TIME (YYYY-MM-DDTHH:MM:SS).
3. For a whole day (e.g., "May 30"):
   start_time = "2026-05-30T00:00:00"
   end_time = "2026-05-30T23:59:59"
4. For a specific time (e.g., "14:00"):
   Create a window around that time to ensure you capture the event, for example:
   start_time = "2026-05-31T13:00:00"
   end_time = "2026-05-31T15:00:00"

=================================================
DATA SOURCE PRIORITY
=================================================
Current status questions: Use LIVE_TELEMETRY first.
Historical questions: MUST call get_tags_history.
Questions containing: today, yesterday, last, previous, before, after, during, between, count, how many, unloaded, loaded, movement, direction, maximum, minimum, average, trend REQUIRE historian lookup.

=================================================
HISTORIAN TAG MAP
=================================================
Cargo: default:Crane/cargo
Weight: default:Crane/Arduino/weight
Car movement: default:Car/movement
Crane movement: default:Crane/Control/up, default:Crane/Control/down, default:Crane/Control/left, default:Crane/Control/right
Remote: default:Crane/allow_remote, default:Car/remote_req
Speed: default:Car/controller/speed_req, default:Crane/speed_mode

=================================================
CONNECTIVITY LOGIC
=================================================
CONNECTED: Crane allow_remote == 1 AND Car remote_req == 1
PENDING: Car remote_req == 1 AND Crane allow_remote == 0

=================================================
CAR TAG DEFINITIONS
=================================================
    - car/movement: 0:IDLE, 1:FWD, 2:REV, 3:LEFT, 4:RIGHT
    - car/controller/status: 0:Disconnected, 1:Connected
    - car/controller/speed_req: 1:Slow, 2:Variable(Weight), 4:Fast
    - car/fault: 0:OK, not 0:Error
    - status: 0:Controller disconnected, 1:Controller connected
    - load_req/unload_req: 1:Automatic crane load/unload requested
    - conn_req_timeout: timer in seconds.

=================================================
CRANE TAG DEFINITIONS
=================================================
    - allow_remote: 0:Accepting, 1:Linked to Car, 2:Locked
    - cargo: 0:Empty, 1:Loaded
    - process_status: 0:Idle, >0:Automatic Sequence Running
    - fault bits: bit0:Arduino Lost, bit1:Dropped Cargo, bit3:Overweight, bit4:Unfinished Calib, bit5:Remote Issue
    - limits: (Horizontal 1:Left, 2:Right) (Vertical 1:Down, 2:Up)
    - speed_mode: 1:slow, 2:variable with weight, 4: fast
    - electromagnet: 0:Off, 1:On
    - detectors (Left:1 -> crane reached horizontal left limit, Right:1 -> crane reached horizontal right limit)
    - calibration: 0: no weight sensor calibration ongoing, not 0: calibration for weight sensor ongoing)
    - control (down/left/right/up: 1:indicates that the crane moved in that direction)

=================================================
CARGO COUNTING RULES
=================================================
Cargo tag: 0 = Empty, 1 = Loaded
LOAD EVENT: 0 -> 1
UNLOAD EVENT: 1 -> 0
"How many cargos unloaded?" -> Count 1 -> 0 transitions
"How many cargos loaded?" -> Count 0 -> 1 transitions
"How many maneuvers?" -> Count 1 -> 0 transitions

=================================================
CRANE DIRECTION RULES
=================================================
To determine direction:
1. Retrieve all four control tags.
2. Find most recent timestamp where value == 1.
3. Compare timestamps. Latest timestamp wins.
Return: UP, DOWN, LEFT, or RIGHT.

=================================================
CAR DIRECTION RULES
=================================================
To determine last movement:
1. Retrieve history of Car/movement.
2. Search backwards. Ignore value 0.
3. First non-zero value is the answer.

=================================================
WEIGHT ANALYSIS
=================================================
Questions regarding heaviest cargo, max weight, or largest load MUST use `default:Crane/Arduino/weight`.

=================================================
FALLBACK & NO DATA RULES
=================================================
1. If the historian returns an empty list or "no data", DO NOT say you couldn't summarize it.
2. Instead, look at the LIVE_TELEMETRY block provided in the prompt.
3. Inform the user: "I couldn't find historical records for that period, but the current/last known value is [VALUE]."
"""

    user_content_str = f"""
LIVE_TELEMETRY:
{live_state}

USER_QUERY:
{request.message}
"""

    current_message = types.Content(
        role="user",
        parts=[types.Part.from_text(text=user_content_str)]
    )

    conversation_payload = GLOBAL_CHAT_HISTORY + [current_message]

    try:
        tools = types.Tool(
            function_declarations=[
                {
                    "name": "get_tags_history",
                    "description": "Retrieve historical telemetry data. Arguments MUST be in Romania Local Time (YYYY-MM-DDTHH:MM:SS).",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "element_ids": {"type": "array", "items": {"type": "string"}},
                            "start_time": {"type": "string", "description": "ISO8601 String in Romania Local Time"},
                            "end_time": {"type": "string", "description": "ISO8601 String in Romania Local Time"}
                        },
                        "required": ["element_ids", "start_time", "end_time"]
                    }
                }
            ]
        )

        try:
            response = client.models.generate_content(
                model=MODEL_ID,
                contents=conversation_payload,
                config=types.GenerateContentConfig(
                    system_instruction=system_instruction,
                    tools=[tools]
                )
            )
        except Exception as api_err:
            print(f"GenAI API Error during first pass: {api_err}")
            return {"reply": "Sorry, I am having trouble connecting right now. Please try again."}

        if not response or not response.candidates:
            return {"reply": "I received an empty response. Please try asking your question differently."}

        candidate = response.candidates[0]
        
        if not candidate.content or not candidate.content.parts:
            return {"reply": "I couldn't process that request properly. Could you rephrase it?"}

        part = candidate.content.parts[0]
        final_text = ""

        if hasattr(part, "function_call") and part.function_call:
            fn = part.function_call
            
            try:
                # --- PYTHON MIDDLEMAN: Robust Time Parser ---
                # Safely parse the AI's string, assuming it might not format perfectly
                st_str = fn.args.get("start_time", "").replace("Z", "")
                et_str = fn.args.get("end_time", "").replace("Z", "")
                
                # If the AI just gave YYYY-MM-DD, append midnight
                if len(st_str) == 10: st_str += "T00:00:00"
                if len(et_str) == 10: et_str += "T23:59:59"

                try:
                    st_local = datetime.fromisoformat(st_str)
                    st_utc = (st_local - romania_offset).isoformat() + "Z"
                except Exception as e:
                    print(f"Error parsing start_time '{st_str}': {e}. Defaulting to 24h ago.")
                    st_utc = (utc_now - timedelta(hours=24)).isoformat() + "Z"

                try:
                    et_local = datetime.fromisoformat(et_str)
                    et_utc = (et_local - romania_offset).isoformat() + "Z"
                except Exception as e:
                    print(f"Error parsing end_time '{et_str}': {e}. Defaulting to now.")
                    et_utc = utc_now.isoformat() + "Z"

                # ------------------------------------------------------------

                history_data = await get_multi_tag_history_data(
                    fn.args.get("element_ids", []),
                    st_utc,
                    et_utc
                )
                
                # --- PYTHON MIDDLEMAN: Convert DB's UTC to Local Time for the AI ---
                if isinstance(history_data, dict) and "error" not in history_data:
                    for tag_key, tag_info in history_data.items():
                        if "data" in tag_info and isinstance(tag_info["data"], list):
                            for pt in tag_info["data"]:
                                if "timestamp" in pt:
                                    try:
                                        ts = datetime.fromisoformat(pt["timestamp"].replace("Z", "+00:00"))
                                        ts_local = ts + romania_offset
                                        pt["timestamp"] = ts_local.strftime("%Y-%m-%d %H:%M:%S")
                                    except Exception as parse_err:
                                        print(f"Timestamp parse error: {parse_err}")

            except Exception as hist_err:
                print(f"Historian/Time Conversion Error: {hist_err}")
                history_data = {"error": "Failed to retrieve data from historian."}

            if not history_data or (isinstance(history_data, dict) and not any(tag.get('data') for tag in history_data.values() if isinstance(tag, dict))):
                history_tool_output = {
                    "result": "No historical records found for this time range. IMPORTANT: Please use the LIVE_TELEMETRY provided in the previous message to tell the user the last known value instead."
                }
            else:
                history_tool_output = {"result": history_data}

            try:
                final_response = client.models.generate_content(
                    model=MODEL_ID,
                    contents=conversation_payload + [
                        candidate.content,
                        types.Content(
                            role="tool",
                            parts=[
                                types.Part.from_function_response(
                                    name=fn.name,
                                    response=history_tool_output
                                )
                            ]
                        )
                    ],
                    config=types.GenerateContentConfig(
                        system_instruction=system_instruction
                    )
                )
                
                if final_response and final_response.text:
                    final_text = final_response.text
                else:
                    # FALLBACK: If the AI still fails to generate text, we manually extract the live value
                    # to ensure the user NEVER sees the "couldn't summarize" message.
                    target_tag = fn.args.get("element_ids", [""])[0]
                    live_val = LATEST_VALUES.get(target_tag, {}).get("value", "Unknown")
                    final_text = f"I couldn't find historical data for that period. The last recorded value for this tag is: **{live_val}**."
                    
            except Exception as api_err2:
                print(f"GenAI API Error during second pass: {api_err2}")
                # Dynamic fallback on API crash
                final_text = "I encountered an error accessing the history, but checking the live system, the current value is available in the dashboard above."
        else:
            if response.text:
                final_text = response.text
            else:
                final_text = "I processed your request but have no text to return."

        GLOBAL_CHAT_HISTORY.append(
            types.Content(role="user", parts=[types.Part.from_text(text=request.message)])
        )
        GLOBAL_CHAT_HISTORY.append(
            types.Content(role="model", parts=[types.Part.from_text(text=final_text)])
        )

        return {"reply": final_text}

    except Exception as e:
        print(f"ask_agent critical error: {e}")
        return {"reply": f"System Error: {str(e)}"}

@app.on_event("startup")
async def start(): asyncio.create_task(historian_snapshot_loop())

if __name__ == "__main__":
    import uvicorn
    warnings.filterwarnings("ignore", message="Unverified HTTPS request")
    uvicorn.run(app, host="0.0.0.0", port=8000)