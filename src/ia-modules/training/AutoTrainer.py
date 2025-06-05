import time
from train_model import NonceTrainer
from utils.nonce_logger import NonceLogger

class AutoTrainer:
    """
    Clase para ejecutar ciclos automáticos de entrenamiento de modelos de nonces.
    """
    def __init__(self, interval_sec: int = 3600):
        """
        Inicializa el AutoTrainer.

        Args:
            interval_sec (int): Intervalo (en segundos) entre ciclos de entrenamiento.
        """
        self.interval = interval_sec
        self.logger = NonceLogger("AutoTrainer")
        self.trainer = NonceTrainer()

    def run_training_cycle(self):
        """
        Ejecuta un ciclo de entrenamiento y evaluación del modelo.
        """
        self.logger.info("📊 Inicio del ciclo de entrenamiento de modelo de nonces...")
        try:
            # Entrenar el modelo
            training_successful = self.trainer.train()

            if training_successful:
                # Evaluar métricas
                metrics = self.trainer.evaluate()
                if metrics:
                    self.logger.info(f"✅ Entrenamiento exitoso. Métricas: {metrics}")
                    # Guardar el modelo si el entrenamiento fue exitoso
                    self.trainer.save_model()
                else:
                    self.logger.warn("⚠️ Entrenamiento completado, pero no se obtuvieron métricas.")
            else:
                self.logger.warn("⚠️ El entrenamiento falló. Verifica los datos y parámetros.")
        except Exception as e:
            self.logger.error(f"❌ Error durante el ciclo de entrenamiento: {e}")

    def run(self):
        """
        Ejecuta el bucle principal de entrenamiento automático.
        """
        self.logger.info("⏳ Iniciando ciclo automático de entrenamiento.")
        while True:
            self.run_training_cycle()
            self.logger.info(f"⏸️ Pausa de {self.interval} segundos antes del próximo ciclo.")
            time.sleep(self.interval)

if __name__ == "__main__":
    # Parámetro configurable: intervalo de entrenamiento en segundos
    INTERVAL_SEC = 3600  # 1 hora
    trainer = AutoTrainer(interval_sec=INTERVAL_SEC)
    trainer.run()