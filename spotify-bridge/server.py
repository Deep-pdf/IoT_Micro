"""
Spotify Bridge HTTP Server for ESP32 micro_IoT remote control.
"""
import os
from dotenv import load_dotenv
from flask import Flask, request, jsonify, redirect, render_template_string

# Load environment variables from .env if present
load_dotenv(os.path.join(os.path.dirname(__file__), ".env"))

from spotify_auth import auth_manager
from spotify_api import spotify_client

app = Flask(__name__)

INDEX_HTML = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Spotify Bridge</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: #121212;
            color: #ffffff;
            margin: 0;
            padding: 40px 20px;
            display: flex;
            justify-content: center;
        }
        .container {
            max-width: 520px;
            width: 100%;
            background: #181818;
            padding: 30px;
            border-radius: 12px;
            box-shadow: 0 8px 24px rgba(0,0,0,0.5);
            border: 1px solid #282828;
        }
        h1 {
            color: #1DB954;
            margin-top: 0;
            font-size: 24px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .status-box {
            padding: 12px 16px;
            border-radius: 8px;
            margin: 20px 0;
            font-size: 14px;
            font-weight: 500;
        }
        .connected {
            background-color: rgba(29, 185, 84, 0.15);
            color: #1DB954;
            border: 1px solid #1DB954;
        }
        .disconnected {
            background-color: rgba(255, 75, 75, 0.15);
            color: #ff5555;
            border: 1px solid #ff5555;
        }
        .btn {
            display: inline-block;
            background-color: #1DB954;
            color: #000000;
            font-weight: bold;
            padding: 12px 24px;
            border-radius: 24px;
            text-decoration: none;
            cursor: pointer;
            transition: transform 0.1s ease, background-color 0.2s ease;
            border: none;
        }
        .btn:hover {
            background-color: #1ed760;
            transform: scale(1.02);
        }
        .btn-outline {
            background-color: transparent;
            color: #ffffff;
            border: 1px solid #555555;
            margin-top: 8px;
        }
        .btn-outline:hover {
            background-color: #282828;
            border-color: #888888;
        }
        .controls {
            display: flex;
            gap: 10px;
            margin-top: 20px;
            flex-wrap: wrap;
        }
        .endpoint-list {
            margin-top: 25px;
            background: #202020;
            padding: 15px;
            border-radius: 8px;
            font-size: 13px;
        }
        .endpoint-list a {
            color: #1DB954;
            text-decoration: none;
        }
        .endpoint-list a:hover {
            text-decoration: underline;
        }
        code {
            background: #2a2a2a;
            padding: 2px 6px;
            border-radius: 4px;
            color: #f1f1f1;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎵 Spotify Bridge</h1>
        <p>Local bridge server for ESP32 micro_IoT remote control.</p>

        {% if not configured %}
            <div class="status-box disconnected">
                ⚠️ Spotify credentials missing in <code>.env</code> file.
            </div>
            <p style="font-size: 14px; color: #b3b3b3;">
                Please configure <code>SPOTIFY_CLIENT_ID</code> and <code>SPOTIFY_CLIENT_SECRET</code> in <code>spotify-bridge/.env</code>, then restart this server.
            </p>
        {% elif authenticated %}
            <div class="status-box connected">
                ✅ Connected to Spotify Account
            </div>
            <p style="font-size: 14px; color: #b3b3b3;">
                ESP32 can now fetch track state and control playback.
            </p>
            <div class="controls">
                <button class="btn" onclick="sendPost('/spotify/playpause')">⏯ Play / Pause</button>
                <button class="btn btn-outline" onclick="sendPost('/spotify/previous')">⏮ Prev</button>
                <button class="btn btn-outline" onclick="sendPost('/spotify/next')">⏭ Next</button>
                <a href="/login" class="btn btn-outline">🔄 Re-authorize</a>
            </div>
        {% else %}
            <div class="status-box disconnected">
                🔒 Not Authorized with Spotify
            </div>
            <p style="font-size: 14px; color: #b3b3b3;">
                Click below to log in with your Spotify account and grant remote playback permissions.
            </p>
            <a href="/login" class="btn">Login with Spotify</a>
        {% endif %}

        <div class="endpoint-list">
            <strong>Endpoints:</strong>
            <ul>
                <li><a href="/spotify/state" target="_blank"><code>GET /spotify/state</code></a> - Live JSON state</li>
                <li><code>POST /spotify/playpause</code> - Toggle Play/Pause</li>
                <li><code>POST /spotify/next</code> - Skip track</li>
                <li><code>POST /spotify/previous</code> - Previous track</li>
                <li><a href="/status" target="_blank"><code>GET /status</code></a> - Bridge health status</li>
            </ul>
        </div>
    </div>

    <script>
        function sendPost(url) {
            fetch(url, { method: 'POST' })
                .then(r => r.json())
                .then(data => alert('Response: ' + JSON.stringify(data)))
                .catch(e => alert('Error: ' + e));
        }
    </script>
</body>
</html>
"""

@app.route("/")
def index():
    configured = auth_manager.is_configured()
    authenticated = auth_manager.is_authenticated() if configured else False
    return render_template_string(INDEX_HTML, configured=configured, authenticated=authenticated)

@app.route("/login")
def login():
    if not auth_manager.is_configured():
        return jsonify({
            "ok": False,
            "error": "SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET are not configured in .env",
        }), 500

    auth_url = auth_manager.generate_auth_url()
    return redirect(auth_url)

@app.route("/callback")
def callback():
    error = request.args.get("error")
    if error:
        return f"<h1>Spotify Authorization Error</h1><p>{error}</p><a href='/'>Return to home</a>", 400

    code = request.args.get("code")
    state = request.args.get("state")

    if not code or not state:
        return "<h1>Invalid Request</h1><p>Missing code or state parameter.</p>", 400

    if not auth_manager.validate_state(state):
        return "<h1>State Validation Failed</h1><p>CSRF state token mismatch. Please try logging in again from /login.</p>", 400

    try:
        auth_manager.exchange_code_for_token(code)
        return """
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <title>Authorized!</title>
            <style>
                body { background-color: #121212; color: #ffffff; font-family: sans-serif; text-align: center; padding-top: 50px; }
                h1 { color: #1DB954; }
                a { color: #1DB954; text-decoration: none; font-weight: bold; }
            </style>
        </head>
        <body>
            <h1>🎉 Spotify Authorization Successful!</h1>
            <p>Your ESP32 micro_IoT remote can now connect to this bridge.</p>
            <p><a href="/">Go to Bridge Dashboard</a> | <a href="/spotify/state">View Current State</a></p>
        </body>
        </html>
        """
    except Exception as e:
        return f"<h1>Token Exchange Error</h1><p>{str(e)}</p><a href='/'>Return to home</a>", 500

@app.route("/status", methods=["GET"])
def status():
    configured = auth_manager.is_configured()
    authenticated = auth_manager.is_authenticated() if configured else False
    return jsonify({
        "ok": True,
        "bridge": "ESP32 Spotify Remote Bridge",
        "configured": configured,
        "authenticated": authenticated,
    })

@app.route("/spotify/state", methods=["GET"])
def get_spotify_state():
    state = spotify_client.get_state()
    return jsonify(state)

@app.route("/spotify/play", methods=["POST", "GET"])
def play_track():
    res = spotify_client.play()
    return jsonify(res)

@app.route("/spotify/pause", methods=["POST", "GET"])
def pause_track():
    res = spotify_client.pause()
    return jsonify(res)

@app.route("/spotify/playpause", methods=["POST", "GET"])
def toggle_playpause():
    res = spotify_client.toggle_play_pause()
    return jsonify(res)

@app.route("/spotify/next", methods=["POST", "GET"])
def next_track():
    res = spotify_client.next_track()
    return jsonify(res)

@app.route("/spotify/previous", methods=["POST", "GET"])
def previous_track():
    res = spotify_client.previous_track()
    return jsonify(res)

if __name__ == "__main__":
    host = os.getenv("SPOTIFY_BRIDGE_HOST", "0.0.0.0")
    port = int(os.getenv("SPOTIFY_BRIDGE_PORT", "8888"))
    print(f"🎵 Starting Spotify Bridge on http://{host}:{port}")
    print(f"👉 Local authorization URL: http://127.0.0.1:{port}/login")
    app.run(host=host, port=port, debug=False)
