# 📡 ZARTRUX - Modo Distribuido (ZARTRUX Distributed Mode)

## 🧠 Descripción General

El modo distribuido de ZARTRUX permite conectar múltiples nodos mineros con un **Hub Central** que coordina el análisis IA, sincroniza modelos, balancea el rendimiento y ofrece una vista global de la operación minera.

---

## ⚙️ Componentes

### 🔹 Nodo Minero (`zartrux-miner`)
- Ejecuta minería real con CPU.
- Puede operar en:
  - Modo IA local.
  - Modo IA remota (proxy al Hub).
- Se comunica vía ZMQ con el Hub.

### 🔹 Hub Central (`zartrux_hub.py`)
- Orquesta la red distribuida.
- Expone APIs HTTP (métricas) y ZMQ (modelos, configuración).
- Lleva el registro de los nodos conectados.

### 🔹 Módulos Clave del Hub:
- `config_broadcaster.py`: Distribuye configuraciones.
- `metrics_server.py`: API de monitoreo.
- `model_sync_manager.py`: Distribución de modelos IA.
- `clients_registry.json`: Lista activa de mineros.

---

## 🧩 Flujo de Trabajo

1. Cada nodo ejecuta `zartrux_launcher.py`.
2. El launcher detecta si hay un Hub activo.
3. Si existe:
    - Se conecta vía ZMQ (`ZartruxHubClient.cpp`).
    - Envía métricas y solicita modelo IA.
4. Si no:
    - Usa IA local como fallback.
5. El Hub centraliza las métricas y puede distribuir configuraciones o modelos optimizados.

---

## 🔐 Seguridad

- La comunicación puede ser cifrada con ZeroMQ + CURVE.
- Los modelos distribuidos pueden firmarse (ver módulo `ConfigSigner`).
- Las conexiones pueden ser autenticadas vía whitelist (configurable).

---

## 🧪 Testing

- Prueba local: `scripts/run_hub.sh` + `scripts/run_node.sh`.
- Validar conexiones: revisar `clients_registry.json`.
- Comprobar rendimiento desde Prometheus o Grafana conectado al `metrics_server.py`.

---

## 📁 Configuración

### `config/hub_config.json`
```json
{
  "host": "127.0.0.1",
  "zmq_port": 5555,
  "rest_port": 8080,
  "model_path": "models/ethical_nonce_model.joblib",
  "broadcast_interval_sec": 30
}
