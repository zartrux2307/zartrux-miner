import pandas as pd
import numpy as np
import os
import time
import joblib
import warnings
import threading
from typing import Dict, Optional, Tuple, List
from dataclasses import dataclass
from datetime import datetime, timedelta
from sklearn.preprocessing import RobustScaler
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.model_selection import TimeSeriesSplit
from sklearn.pipeline import make_pipeline
from sklearn.metrics import mean_absolute_error, r2_score
from alibi_detect import KSDrift
from tqdm.auto import tqdm
import mlflow

# ========== CONFIGURACIÓN ==========
MODEL_PATH = "models/hashrate_predictor.joblib"
PREDICTION_LOG = "logs/hashrate_predictions.csv"
PROMETHEUS_EXPORT = "logs/metrics.prom"

warnings.filterwarnings('ignore')
mlflow.set_tracking_uri("http://localhost:5000")


@dataclass
class PredictorConfig:
    min_entries: int = 100
    max_entries: int = 10000
    retrain_interval: int = 3600
    drift_threshold: float = 0.05
    model_type: str = "gradient_boosting"
    features: Tuple[str] = (
        'elapsed', 'hour', 'day_of_week',
        'rolling_avg_1h', 'rolling_std_1h'
    )


class HashRatePredictor:
    def __init__(self, csv_path: str = "logs/nonces_hash.csv", config: Optional[PredictorConfig] = None):
        self.csv_path = csv_path
        self.config = config or PredictorConfig()
        self.df = pd.DataFrame()
        self.model = None
        self.last_train_time = None
        self.model_metrics = {}
        self.drift_detector = None
        self._initialize_directories()

    def _initialize_directories(self):
        """Crea los directorios necesarios si no existen"""
        os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
        os.makedirs(os.path.dirname(PREDICTION_LOG), exist_ok=True)

    def load_and_preprocess(self):
        """Carga y preprocesa los datos de minería"""
        if not os.path.exists(self.csv_path):
            raise FileNotFoundError(f"Archivo no encontrado: {self.csv_path}")

        chunks = []
        with tqdm(desc="Cargando datos", unit="chunk") as pbar:
            for chunk in pd.read_csv(self.csv_path, chunksize=1000, parse_dates=['timestamp']):
                chunks.append(chunk)
                pbar.update(1)

        self.df = pd.concat(chunks).sort_values('timestamp').tail(self.config.max_entries)
        
        # Feature engineering
        self._create_features()
        self._handle_missing_data()

    def _create_features(self):
        """Genera características temporales y estadísticas móviles"""
        min_timestamp = self.df['timestamp'].min()
        self.df['elapsed'] = (self.df['timestamp'] - min_timestamp).dt.total_seconds()
        self.df['hour'] = self.df['timestamp'].dt.hour
        self.df['day_of_week'] = self.df['timestamp'].dt.dayofweek
        self.df['rolling_avg_1h'] = self.df['hashrate'].rolling(6).mean()
        self.df['rolling_std_1h'] = self.df['hashrate'].rolling(6).std()

    def _handle_missing_data(self):
        """Manejo de datos faltantes y outliers"""
        cols = self.config.features + ('hashrate',)
        self.df.dropna(subset=cols, inplace=True)
        
        # Eliminar outliers usando IQR
        q1 = self.df['hashrate'].quantile(0.25)
        q3 = self.df['hashrate'].quantile(0.75)
        iqr = q3 - q1
        self.df = self.df[(self.df['hashrate'] > q1 - 1.5*iqr) & 
                         (self.df['hashrate'] < q3 + 1.5*iqr)]

    def train_model(self) -> Dict[str, float]:
        """Entrena el modelo de predicción de hashrate"""
        if len(self.df) < self.config.min_entries:
            raise ValueError("No hay suficientes datos para entrenar")

        X = self.df[self.config.features]
        y = self.df['hashrate']
        
        model = make_pipeline(
            RobustScaler(),
            GradientBoostingRegressor(
                n_estimators=150,
                max_depth=5,
                random_state=42,
                validation_fraction=0.1,
                n_iter_no_change=5
            )
        )

        tscv = TimeSeriesSplit(n_splits=5)
        scores = []
        
        for train_idx, test_idx in tscv.split(X):
            X_train, X_test = X.iloc[train_idx], X.iloc[test_idx]
            y_train, y_test = y.iloc[train_idx], y.iloc[test_idx]
            
            model.fit(X_train, y_train)
            preds = model.predict(X_test)
            
            scores.append({
                'mae': mean_absolute_error(y_test, preds),
                'r2': r2_score(y_test, preds)
            })

        model.fit(X, y)
        self.model = model
        self.last_train_time = datetime.now()
        
        # Métricas del modelo
        self.model_metrics = {
            'mae_mean': np.mean([s['mae'] for s in scores]),
            'r2_mean': np.mean([s['r2'] for s in scores]),
            'last_trained': self.last_train_time.isoformat(),
            'samples_used': len(self.df)
        }

        # Detector de drift de datos
        self.drift_detector = KSDrift(
            X.values, 
            p_val=self.config.drift_threshold,
            window_size=min(1000, len(X)//10)
        )

        # Registro en MLflow
        with mlflow.start_run(run_name=f"HashRateModel-{datetime.now().strftime('%Y%m%d%H%M')}"):
            mlflow.log_metrics(self.model_metrics)
            mlflow.log_params({
                "model_type": self.config.model_type,
                "features": str(self.config.features),
                "drift_threshold": self.config.drift_threshold
            })
            mlflow.sklearn.log_model(model, "model")

        # Guardado del modelo
        joblib.dump({
            'model': model,
            'metrics': self.model_metrics,
            'features': self.config.features,
            'last_trained': self.last_train_time,
            'drift_detector': self.drift_detector
        }, MODEL_PATH)

        return self.model_metrics

    def _load_saved_model(self):
        """Carga un modelo previamente entrenado"""
        if os.path.exists(MODEL_PATH):
            saved = joblib.load(MODEL_PATH)
            self.model = saved['model']
            self.last_train_time = saved['last_trained']
            self.model_metrics = saved['metrics']
            self.drift_detector = saved.get('drift_detector')
        else:
            raise FileNotFoundError("No se encontró modelo entrenado")

    def check_for_drift(self) -> bool:
        """Verifica si hay drift en los datos actuales"""
        if self.drift_detector is None or self.df.empty:
            return False
            
        current_data = self.df[self.config.features].values[-1000:]
        drift_preds = self.drift_detector.predict(current_data)
        return drift_preds['data']['is_drift'] == 1

    def predict(self, minutes_ahead: int = 30) -> Dict[str, float]:
        """Realiza una predicción del hashrate futuro"""
        if self.model is None:
            self._load_saved_model()

        if self.df.empty:
            raise ValueError("El DataFrame está vacío. No se puede realizar la predicción.")

        last = self.df.iloc[-1]
        future_time = last['timestamp'] + timedelta(minutes=minutes_ahead)

        # Construcción de características futuras
        future_features = pd.DataFrame([{
            'elapsed': (future_time - self.df['timestamp'].min()).total_seconds(),
            'hour': future_time.hour,
            'day_of_week': future_time.dayofweek,
            'rolling_avg_1h': last['rolling_avg_1h'],
            'rolling_std_1h': last['rolling_std_1h']
        }])[self.config.features]

        # Predicción y formato del resultado
        prediction = self.model.predict(future_features)[0]
        result = {
            'timestamp': future_time.isoformat(),
            'predicted_hashrate': prediction,
            'confidence_interval': self._calculate_confidence(prediction)
        }

        # Registro y exportación
        self._log_prediction(result)
        self._export_prometheus_metrics(result)
        
        return result

    def _calculate_confidence(self, prediction: float) -> Tuple[float, float]:
        """Calcula intervalo de confianza usando las métricas del modelo"""
        mae = self.model_metrics.get('mae_mean', 0)
        return (prediction - mae, prediction + mae)

    def _log_prediction(self, result: Dict):
        """Registra la predicción en archivo CSV"""
        header = not os.path.exists(PREDICTION_LOG)
        log_entry = pd.DataFrame([result])
        log_entry.to_csv(PREDICTION_LOG, mode='a', header=header, index=False)

    def _export_prometheus_metrics(self, result: Dict):
        """Exporta métricas en formato Prometheus"""
        metrics = [
            f"predicted_hashrate {result['predicted_hashrate']} {time.time_ns()}",
            f"confidence_lower {result['confidence_interval'][0]} {time.time_ns()}",
            f"confidence_upper {result['confidence_interval'][1]} {time.time_ns()}"
        ]
        
        with open(PROMETHEUS_EXPORT, 'a') as f:
            f.write("\n".join(metrics) + "\n")

    def start_auto_update(self):
        """Inicia el auto-entrenamiento periódico en un hilo separado"""
        def update_loop():
            while True:
                time.sleep(self.config.retrain_interval)
                try:
                    self.load_and_preprocess()
                    if self.check_for_drift():
                        self.train_model()
                except Exception as e:
                    print(f"Error en auto-actualización: {str(e)}")

        thread = threading.Thread(target=update_loop, daemon=True)
        thread.start()