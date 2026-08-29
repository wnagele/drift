// Mock device server for the dashboard. Three roles in one:
//
//  1. E2E host: serves the production page (index.html with the webpack
//     bundle inlined, like the firmware does). The mock API/WebSocket
//     endpoints below also exist, but the Playwright tests intercept them
//     with page.route/page.routeWebSocket before the network, so the specs
//     never see the mocks.
//  2. Local visual check: `npm run dev`, then open http://127.0.0.1:8321 —
//     the dashboard runs against the shared fixtures exactly like it would
//     against a device: config from api/config.json, footer build info from
//     api/debug-info.json, and a /ws stream pushing api/status.json once a
//     second like the firmware's taskSendStatus.
//  3. WS_MODE env knob to eyeball the connection-box scenarios, named for
//     the state they produce:
//       connected (default) — healthy 1 Hz stream
//       no-data             — accepts the socket but never sends: the box
//                             shows Connecting → No data (no message yet)
//       disconnected        — drops the connection right away: Disconnected
//                             with its "since Ns ago" age
//       flaky               — pushes with random delays: mostly ~1 s like
//                             the firmware, but ~20% of gaps exceed the
//                             dash's 5 s staleness threshold, so the box
//                             flips between Connected and No data
const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

const PORT = process.env.PORT || 8321;
const WS_MODES = ['connected', 'no-data', 'disconnected', 'flaky'];
const WS_MODE = process.env.WS_MODE || 'connected';
if (!WS_MODES.includes(WS_MODE)) {
    console.error('WS_MODE must be one of: ' + WS_MODES.join(', '));
    process.exit(1);
}

const html = fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
const bundle = fs.readFileSync(path.join(__dirname, '..', 'bundle.js'), 'utf8');
const page = html.replace('%%%SCRIPT%%%', () => bundle);

// The shared fixtures the firmware unit tests, e2e NVS seed and dash tests
// all assert against (see AGENTS.md "The shared contract") — the mock device
// serves the same bytes.
const FIXTURES = path.join(__dirname, '..', '..', 'test', 'fixtures', 'api');

function fixture(name) {
    return fs.readFileSync(path.join(FIXTURES, name), 'utf8');
}

const server = http.createServer((req, res) => {
    const json = (body) => {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(body);
    };

    if (req.url === '/') {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(page);
    } else if (req.url === '/api/config' && req.method === 'GET') {
        json(fixture('config.json'));
    } else if (req.url === '/api/config' && req.method === 'POST') {
        // The real device returns 200 and reboots; the dash only shows the
        // "Config saved." toast. Drain the body so the request completes.
        req.resume();
        req.on('end', () => json('{}'));
    } else if (req.url === '/debug/info') {
        json(fixture('debug-info.json'));
    } else {
        res.writeHead(404);
        res.end();
    }
});

// -- Mock /ws: a 1 Hz status stream like taskSendStatus in main.cpp --------
//
// noServer + manual upgrade so the HTTP routes and the WebSocket share one
// port, the same layout the firmware's AsyncWebServer exposes.

const wss = new WebSocketServer({ noServer: true });

server.on('upgrade', (req, socket, head) => {
    const { pathname } = new URL(req.url, 'http://localhost');
    if (pathname !== '/ws') {
        socket.destroy();
        return;
    }
    wss.handleUpgrade(req, socket, head, (ws) => wss.emit('connection', ws, req));
});

wss.on('connection', (ws) => {
    const timers = [];
    let flakyTimer = null;

    const clearAll = () => {
        for (const t of timers)
            clearInterval(t);
        if (flakyTimer !== null)
            clearTimeout(flakyTimer);
    };

    if (WS_MODE === 'no-data') {
        // Accept the socket, never send: the box should show No data.
    } else if (WS_MODE === 'disconnected') {
        // Drop the connection right away: the box shows Disconnected with
        // its "since Ns ago" age.
        ws.close();
    } else if (WS_MODE === 'flaky') {
        // Self-rescheduling random delay: mostly firmware-like ~1 s pushes,
        // occasionally a gap past STALE_AFTER_MS (5 s in useStatusSocket.js)
        // so the connection box visibly flips to No data and back.
        const sendNext = () => {
            const delay = Math.random() < 0.2
                ? 5000 + Math.random() * 4000   // breach: 5–9 s silence
                : 700 + Math.random() * 600;    // normal: 0.7–1.3 s
            flakyTimer = setTimeout(() => {
                if (ws.readyState === ws.OPEN)
                    ws.send(fixture('status.json'));
                sendNext();
            }, delay);
        };
        sendNext();
    } else {
        // connected: the firmware's cadence — one push per second.
        timers.push(setInterval(() => {
            if (ws.readyState === ws.OPEN)
                ws.send(fixture('status.json'));
        }, 1000));
    }

    ws.on('close', clearAll);
});

server.listen(PORT, () => {
    console.log(`mock device on http://127.0.0.1:${PORT} (WS_MODE=${WS_MODE})`);
});
