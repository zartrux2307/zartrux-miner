import json
import os
import logging
import time
from http import HTTPStatus
from concurrent.futures import ThreadPoolExecutor
from functools import lru_cache
from pathlib import Path
from typing import List, Dict
from fastapi import FastAPI, HTTPException, Depends, Security
from fastapi.security import APIKeyHeader
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn

# Módulos internos
from evaluation.nonce_quality_filter import NonceQualityFilter
from evaluation.nonce_stats import NonceStats
from ia_modules.bridge.predict_nonce_inference import PredictNonceInference
from utils.nonce_logger import NonceLogger

# Configuración
app = FastAPI(title="Zartrux Nonce Server", version="2.1.0")
API_KEY_NAME = "X-API-KEY"
api_key_header = APIKeyHeader(name=API_KEY_NAME, auto_error=False)

# Constantes
CONFIG = {
    "port": 4444,
    "host": "0.0.0.0",
    "nonces_file": "ia-modules/bridge/nonces_ready.json",
    "backup_dir": "backup_nonces/",
    "max_backups": 50,
    "rate_limit": "100/minute",
    "allowed_api_keys": ["zartrux-miner-key"],
    "cache_size": 1000
}

# Logging configurado para minería
logger = NonceLogger("NonceServer")
logging.getLogger("uvicorn.error").propagate = False

# Modelos de datos
class NonceResponse(BaseModel):
    nonces: List[str]
    stats: Dict[str, float]
    metadata: Dict[str, str]

class ErrorResponse(BaseModel):
    error: str
    code: int

# Middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET"],
    allow_headers=[API_KEY_NAME]
)

# Caché y recursos compartidos
executor = ThreadPoolExecutor(max_workers=4)

# Seguridad y autenticación
def validate_api_key(api_key: str = Security(api_key_header)):
    if api_key not in CONFIG['allowed_api_keys']:
        raise HTTPException(
            status_code=HTTPStatus.UNAUTHORIZED,
            detail="Invalid API Key"
        )
    return api_key

def backup_rotation():
    """Mantiene solo los últimos N backups"""
    backup_path = Path(CONFIG['backup_dir'])
    backups = sorted(backup_path.glob("*.json"), key=os.path.getmtime)
    if len(backups) > CONFIG['max_backups']:
        for old_backup in backups[:-CONFIG['max_backups']]:
            old_backup.unlink()

@lru_cache(maxsize=CONFIG['cache_size'])
def load_and_filter_nonces() -> tuple[List[str], Dict[str, float]]:
    """Carga y filtra nonces con múltiples capas de validación"""
    try:
        with open(CONFIG['nonces_file'], 'r') as f:
            nonces = json.load(f)
        
        # Validación básica
        if not isinstance(nonces, list) or not all(isinstance(n, str) for n in nonces):
            raise ValueError("Formato de nonces inválido")
        
        # Filtrado en paralelo
        future = executor.submit(
            NonceQualityFilter.multithread_filter,
            nonces,
            CONFIG['rate_limit']
        )
        filtered = future.result()
        
        # Inferencia IA
        inference = PredictNonceInference()
        scored_nonces = inference.score_nonces(filtered)
        
        # Análisis estadístico
        stats = NonceStats.analisis_completo([score for _, score in scored_nonces])
        
        return [n for n, _ in scored_nonces], stats
        
    except Exception as e:
        logger.error(f"Error procesando nonces: {str(e)}")
        return [], {}

@app.get("/nonces", 
         response_model=NonceResponse,
         responses={404: {"model": ErrorResponse}, 500: {"model": ErrorResponse}})
async def get_optimized_nonces(api_key: str = Depends(validate_api_key)):
    """Endpoint principal para obtener nonces optimizados"""
    try:
        start_time = time.monotonic()
        
        # Cargar y procesar
        nonces, stats = load_and_filter_nonces()
        
        if not nonces:
            raise HTTPException(
                status_code=HTTPStatus.NO_CONTENT,
                detail="No hay nonces disponibles"
            )
        
        # Rotación de backups
        backup_path = Path(CONFIG['backup_dir']) / f"nonces_{int(time.time())}.json"
        backup_path.parent.mkdir(exist_ok=True)
        os.rename(CONFIG['nonces_file'], backup_path)
        backup_rotation()
        
        # Log de rendimiento
        logger.performance_log(
            operation="serve_nonces",
            duration=time.monotonic() - start_time,
            item_count=len(nonces)
        
        return {
            "nonces": nonces[:100],  # Limitar respuesta
            "stats": stats,
            "metadata": {
                "model_version": PredictNonceInference.MODEL_VERSION,
                "source": backup_path.name,
                "cache_status": "HIT" if nonces else "MISS"
            }
        }
        
    except FileNotFoundError:
        raise HTTPException(
            status_code=HTTPStatus.NOT_FOUND,
            detail="Archivo de nonces no encontrado"
        )
    except Exception as e:
        logger.critical(f"Error crítico: {str(e)}")
        raise HTTPException(
            status_code=HTTPStatus.INTERNAL_SERVER_ERROR,
            detail="Error interno del servidor"
        )

@app.on_event("shutdown")
def cleanup():
    """Limpieza al apagar el servidor"""
    executor.shutdown(wait=False)
    logger.info("Recursos liberados, servidor apagado")

if __name__ == "__main__":
    uvicorn.run(
        app,
        host=CONFIG['host'],
        port=CONFIG['port'],
        log_config=None,
        timeout_keep_alive=30
    )