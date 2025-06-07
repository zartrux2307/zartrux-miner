"""
Launcher principal para el sistema de IA de Zartrux-Miner
Coordina todos los módulos de IA, modelos analíticos y servicios de inferencia
"""

import os
import sys
import logging
import signal
import subprocess
from threading import Event
from concurrent.futures import ThreadPoolExecutor
from typing import Dict, List
import zmq
import psutil

# Módulos internos
from utils.config_manager import get_ia_config, get_hub_config
from utils.nonce_loader import load_realtime_nonces
from bridge.ethical_nonce_adapter import EthicalNonceFilter
from training.AutoTrainer import ModelTrainer
from analytics.TimeSeriesAnalyzer import TemporalPatternDetector
from evaluation.nonce_stats import StatisticalAnalyzer

logger = logging.getLogger('ZartruxAILauncher')

class AIOrchestrator:
    def __init__(self):
        self.shutdown_event = Event()
        self.services: List[subprocess.Popen] = []
        self.executor = ThreadPoolExecutor(max_workers=8)
        self.context = zmq.Context()
        
        # Configuraciones
        self.ia_config = get_ia_config()
        self.hub_config = get_hub_config()
        self.distributed_mode = self._check_distributed_mode()
        
        # Componentes principales
        self.nonce_filter = EthicalNonceFilter()
        self.model_trainer = ModelTrainer()
        self.pattern_detector = TemporalPatternDetector()
        self.stats_analyzer = StatisticalAnalyzer()

    def _check_distributed_mode(self) -> bool:
        """Determina si estamos en modo distribuido"""
        config_path = os.path.join('config', 'distributed_mode.json')
        if os.path.exists(config_path):
            with open(config_path) as f:
                return json.load(f).get('enabled', False)
        return False

    def _start_core_services(self):
        """Inicia servicios esenciales de IA"""
        services = {
            'inference_server': ['python', 'bridge/predict_nonce_server.py'],
            'model_trainer': ['python', 'training/train_model.py'],
            'analytics_engine': ['python', 'analytics/ClusterAnalyzer.py']
        }

        for name, cmd in services.items():
            try:
                proc = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True
                )
                self.services.append(proc)
                logger.info(f"Servicio {name} iniciado con PID {proc.pid}")
            except Exception as e:
                logger.error(f"Error iniciando {name}: {str(e)}")

    def _connect_to_hub(self):
        """Conecta con el Hub central en modo distribuido"""
        hub_socket = self.context.socket(zmq.DEALER)
        hub_socket.connect(self.hub_config['hub_endpoint'])
        logger.info("Conectado al Hub Zartrux")
        return hub_socket

    def _start_data_pipelines(self):
        """Inicia flujos de procesamiento de datos en tiempo real"""
        # Pipeline de procesamiento de nonces
        self.executor.submit(self._nonce_processing_loop)
        
        # Pipeline de entrenamiento de modelos
        self.executor.submit(self._model_training_loop)
        
        # Pipeline de análisis predictivo
        self.executor.submit(self._predictive_analysis_loop)

    def _nonce_processing_loop(self):
        """Procesamiento en tiempo real de nonces"""
        nonce_stream = load_realtime_nonces()
        
        while not self.shutdown_event.is_set():
            try:
                batch = next(nonce_stream)
                filtered = self.nonce_filter.filter_batch(batch)
                
                # Procesamiento paralelo
                self.executor.submit(self.pattern_detector.analyze, filtered)
                self.executor.submit(self.stats_analyzer.calculate_stats, filtered)
                
                # Envío a inferencia
                if self.distributed_mode:
                    self._send_to_hub(filtered)
                else:
                    self._local_inference(filtered)
                    
            except StopIteration:
                logger.warning("Flujo de nonces interrumpido")
                break
            except Exception as e:
                logger.error(f"Error en procesamiento: {str(e)}")

    def _model_training_loop(self):
        """Ciclo de entrenamiento automático de modelos"""
        while not self.shutdown_event.is_set():
            try:
                self.model_trainer.run_incremental_training()
                self.shutdown_event.wait(
                    self.ia_config['training_interval']
                )
            except Exception as e:
                logger.error(f"Error en entrenamiento: {str(e)}")

    def _predictive_analysis_loop(self):
        """Análisis predictivo continuo"""
        predictor = HashRatePredictor()
        while not self.shutdown_event.is_set():
            try:
                predictor.update_model()
                predictor.generate_forecast()
                self.shutdown_event.wait(300)  # Cada 5 minutos
            except Exception as e:
                logger.error(f"Error en predicción: {str(e)}")

    def _local_inference(self, data):
        """Ejecuta inferencia en modo local"""
        # Implementar lógica de inferencia local
        pass

    def _send_to_hub(self, data):
        """Envía datos al hub en modo distribuido"""
        # Implementar lógica de distribución
        pass

    def _cleanup_resources(self):
        """Limpieza de recursos y finalización segura"""
        logger.info("Deteniendo servicios...")
        
        # Detener subprocesos
        self.executor.shutdown(wait=False)
        
        # Terminar procesos hijos
        for proc in self.services:
            try:
                parent = psutil.Process(proc.pid)
                for child in parent.children(recursive=True):
                    child.terminate()
                parent.terminate()
            except psutil.NoSuchProcess:
                continue
        
        # Liberar recursos ZMQ
        self.context.term()

    def run(self):
        """Ejecuta el launcher principal"""
        signal.signal(signal.SIGINT, self._handle_shutdown)
        signal.signal(signal.SIGTERM, self._handle_shutdown)

        logger.info("Iniciando sistema de IA Zartrux...")
        
        if self.distributed_mode:
            self.hub_connection = self._connect_to_hub()
        else:
            self._start_core_services()
        
        self._start_data_pipelines()
        
        logger.info("Sistema IA operativo. Esperando datos...")
        self.shutdown_event.wait()  # Espera hasta señal de apagado
        self._cleanup_resources()

    def _handle_shutdown(self, signum, frame):
        """Maneja la señal de apagado"""
        logger.info(f"Recibida señal {signum}. Apagando...")
        self.shutdown_event.set()

def main():
    # Configuración básica de logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    
    orchestrator = AIOrchestrator()
    orchestrator.run()

if __name__ == "__main__":
    main()