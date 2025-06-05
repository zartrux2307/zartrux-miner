import os
import time
import json
import threading
from flask import Flask, render_template, send_from_directory, jsonify, request
from flask_cors import CORS
from flask_socketio import SocketIO, emit

app = Flask(__name__, static_folder='static')
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

STATUS_PATH = "zartrux_status.json"
LOG_PATH = "zartrux_console.log"
MINING_MODES = ['POOL', 'IA', 'HYBRID']
current_mode = "POOL"

def read_status():
    try:
        with open(STATUS_PATH, "r") as f:
            return json.load(f)
    except Exception as e:
        return {"error": str(e), "status": "error"}

def write_mode(mode):
    global current_mode
    current_mode = mode
    # Aquí debe invocarse el backend real para cambiar el modo
    # Por ejemplo, escribir en archivo/cmd/fifo o invocar script externo
    with open("mining_mode.txt", "w") as f:
        f.write(mode)

def read_console_log(last_n=100):
    if not os.path.exists(LOG_PATH):
        return []
    with open(LOG_PATH, "r", encoding="utf8") as f:
        lines = f.readlines()
        return lines[-last_n:]

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/status")
def api_status():
    return jsonify(read_status())

@app.route("/api/console")
def api_console():
    log = read_console_log(150)
    return jsonify({"log": log})

@app.route("/api/set_mode", methods=["POST"])
def set_mode():
    data = request.get_json()
    mode = data.get("mode", "POOL").upper()
    if mode not in MINING_MODES:
        return jsonify({"result": "error", "msg": "Modo inválido"}), 400
    write_mode(mode)
    socketio.emit('mode_changed', {"mode": mode})
    return jsonify({"result": "ok", "mode": mode})

@socketio.on('request_status')
def send_status_event():
    status = read_status()
    emit('status_update', status)

@socketio.on('request_console')
def send_console_event():
    log = read_console_log()
    emit('console_update', {"log": log})

def push_status_loop():
    last_status = None
    while True:
        status = read_status()
        if status != last_status:
            socketio.emit('status_update', status)
            last_status = status
        time.sleep(1)

def push_console_loop():
    last_log = []
    while True:
        log = read_console_log()
        if log != last_log:
            socketio.emit('console_update', {"log": log})
            last_log = log
        time.sleep(1)

def background_push():
    t1 = threading.Thread(target=push_status_loop, daemon=True)
    t2 = threading.Thread(target=push_console_loop, daemon=True)
    t1.start()
    t2.start()

if __name__ == "__main__":
    background_push()
    socketio.run(app, host="0.0.0.0", port=5000)
