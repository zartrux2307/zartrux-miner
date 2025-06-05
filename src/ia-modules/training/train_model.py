import os
import joblib
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from logging.handlers import RotatingFileHandler
from sklearn.compose import ColumnTransformer
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
from sklearn.model_selection import TimeSeriesSplit, RandomizedSearchCV
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import RobustScaler, FunctionTransformer
from sklearn.inspection import partial_dependence
import optuna
import shap
from nonce_logger import NonceLogger
from utils.nonce_loader import NonceLoader
from evaluation.nonce_stats import NonceStats

@dataclass
class ModelConfig:
    data_path: str = os.path.join("data", "nonce_training_data.parquet")
    model_path: str = os.path.join("models", "nonce_model_v1.joblib")
    test_size: float = 0.15
    random_state: int = 42
    n_jobs: int = -1
    feature_params: Dict = None
    hyperparam_space: Dict = None

class NonceFeatureEngineer:
    """Clase para ingeniería avanzada de características de nonces"""
    
    def __init__(self, config: ModelConfig):
        self.config = config
        self.logger = NonceLogger("FeatureEngineer")
        
        # Configuración de características
        self.feature_params = config.feature_params or {
            'hash_algorithms': ['sha256', 'blake2b'],
            'entropy_bins': 10,
            'window_sizes': [4, 8, 16]
        }

    def _calculate_entropy(self, s: str) -> float:
        """Calcula entropía de Shannon con suavizado de Laplace"""
        counts = np.bincount([ord(c) for c in s])
        counts += 1  # Suavizado
        prob = counts / counts.sum()
        return -np.sum(prob * np.log2(prob))

    def _hex_metrics(self, hex_str: str) -> Dict:
        """Calcula métricas estadísticas de la representación hexadecimal"""
        byte_values = [int(hex_str[i:i+2], 16) for i in range(0, len(hex_str), 2)]
        return {
            'byte_mean': np.mean(byte_values),
            'byte_std': np.std(byte_values),
            'byte_skew': stats.skew(byte_values),
            'byte_kurtosis': stats.kurtosis(byte_values)
        }

    def _rolling_features(self, hex_str: str, window_size: int) -> Dict:
        """Características de ventana móvil para patrones temporales"""
        return {
            f'rolling_mean_{window_size}': np.convolve(hex_str, np.ones(window_size)/window_size, mode='valid').mean(),
            f'rolling_std_{window_size}': np.convolve(hex_str, np.ones(window_size)/window_size, mode='valid').std()
        }

    def extract_features(self, nonce_str: str) -> Dict:
        """Pipeline completo de extracción de características"""
        try:
            hex_str = nonce_str[2:] if nonce_str.startswith('0x') else nonce_str
            features = {
                'length': len(nonce_str),
                'hex_length': len(hex_str),
                'entropy': self._calculate_entropy(nonce_str)
            }
            
            # Métricas estadísticas
            features.update(self._hex_metrics(hex_str))
            
            # Características de ventana móvil
            for ws in self.feature_params['window_sizes']:
                if len(hex_str) >= ws:
                    features.update(self._rolling_features(hex_str, ws))
            
            # Hash features
            for algo in self.feature_params['hash_algorithms']:
                hash_func = hashlib.new(algo)
                hash_func.update(nonce_str.encode())
                digest = hash_func.hexdigest()
                features[f'{algo}_first'] = int(digest[:2], 16)
                features[f'{algo}_mid'] = int(digest[len(digest)//2-1:len(digest)//2+1], 16)
                features[f'{algo}_last'] = int(digest[-2:], 16)
            
            return features
            
        except Exception as e:
            self.logger.error(f"Error procesando nonce {nonce_str[:8]}...: {str(e)}")
            return {}

class NonceTrainer:
    """Clase mejorada para entrenamiento de modelos de predicción de nonces"""
    
    def __init__(self, config: ModelConfig = ModelConfig()):
        self.config = config
        self.logger = NonceLogger("NonceTrainer")
        self.model = None
        self.features = None
        self.study = None
        self._initialize_pipeline()

    def _initialize_pipeline(self):
        """Configuración del pipeline de procesamiento"""
        self.preprocessor = ColumnTransformer([
            ('scaler', RobustScaler(), slice(None)),
            ('nonce_stats', FunctionTransformer(NonceStats.add_statistical_features), [0])
        ])
        
        self.model_pipeline = Pipeline([
            ('preprocessor', self.preprocessor),
            ('model', HistGradientBoostingRegressor(
                random_state=self.config.random_state,
                categorical_features=False
            ))
        ])
        
        self.hyperparam_space = self.config.hyperparam_space or {
            'model__learning_rate': [0.01, 0.05, 0.1],
            'model__max_iter': [200, 500],
            'model__max_depth': [3, 5, 7],
            'model__l2_regularization': [0.0, 0.1, 0.5]
        }

    def load_data(self) -> Tuple[pd.DataFrame, pd.Series]:
        """Carga y procesa los datos con validación mejorada"""
        try:
            df = NonceLoader.load_parquet(self.config.data_path)
            
            if 'nonce' not in df.columns or 'score' not in df.columns:
                raise ValueError("Dataset no contiene las columnas requeridas")
                
            feature_engineer = NonceFeatureEngineer(self.config)
            df['features'] = df['nonce'].progress_apply(feature_engineer.extract_features)
            self.features = pd.json_normalize(df['features'])
            
            return self.features, df['score']
            
        except Exception as e:
            self.logger.error(f"Error cargando datos: {str(e)}")
            raise

    def _objective(self, trial, X: pd.DataFrame, y: pd.Series) -> float:
        """Función objetivo para optimización con Optuna"""
        params = {
            'learning_rate': trial.suggest_float('learning_rate', 0.01, 0.2, log=True),
            'max_iter': trial.suggest_categorical('max_iter', [200, 500, 1000]),
            'max_depth': trial.suggest_int('max_depth', 3, 7),
            'l2_regularization': trial.suggest_float('l2_regularization', 0.0, 1.0)
        }
        
        model = HistGradientBoostingRegressor(**params, random_state=self.config.random_state)
        pipeline = Pipeline([
            ('preprocessor', self.preprocessor),
            ('model', model)
        ])
        
        cv = TimeSeriesSplit(n_splits=5)
        scores = []
        for train_idx, test_idx in cv.split(X):
            X_train, X_test = X.iloc[train_idx], X.iloc[test_idx]
            y_train, y_test = y.iloc[train_idx], y.iloc[test_idx]
            
            pipeline.fit(X_train, y_train)
            scores.append(pipeline.score(X_test, y_test))
            
        return np.mean(scores)

    def train(self, use_optuna: bool = True) -> bool:
        """Entrenamiento con optimización avanzada de hiperparámetros"""
        try:
            X, y = self.load_data()
            
            if use_optuna:
                self.study = optuna.create_study(direction='maximize')
                self.study.optimize(lambda trial: self._objective(trial, X, y), n_trials=50)
                
                best_params = self.study.best_params
                self.model_pipeline.set_params(**{
                    f'model__{k}': v for k, v in best_params.items()
                })
            
            self.model_pipeline.fit(X, y)
            self.logger.info("Entrenamiento completado exitosamente")
            return True
            
        except Exception as e:
            self.logger.error(f"Error durante el entrenamiento: {str(e)}")
            return False

    def evaluate(self) -> Dict:
        """Evaluación detallada con múltiples métricas y visualizaciones"""
        X, y = self.load_data()
        preds = self.model_pipeline.predict(X)
        
        metrics = {
            'MAE': mean_absolute_error(y, preds),
            'MSE': mean_squared_error(y, preds),
            'RMSE': np.sqrt(mean_squared_error(y, preds)),
            'R2': r2_score(y, preds)
        }
        
        # Visualización de importancia de características
        self._plot_feature_importance(X)
        # Análisis de dependencia parcial
        self._plot_partial_dependence(X)
        # Interpretabilidad con SHAP
        self._shap_analysis(X)
        
        return metrics

    def _plot_feature_importance(self, X: pd.DataFrame):
        """Visualización de importancia de características"""
        importances = self.model_pipeline.named_steps['model'].feature_importances_
        features = self.preprocessor.get_feature_names_out()
        
        plt.figure(figsize=(12, 8))
        sns.barplot(x=importances, y=features, palette="viridis")
        plt.title("Importancia de Características")
        plt.savefig("reports/feature_importance.png")
        plt.close()

    def _plot_partial_dependence(self, X: pd.DataFrame, top_n: int = 5):
        """Análisis de dependencia parcial para las características principales"""
        importances = self.model_pipeline.named_steps['model'].feature_importances_
        top_features = np.argsort(importances)[-top_n:]
        
        fig, ax = plt.subplots(figsize=(12, 8))
        partial_dependence(
            self.model_pipeline.named_steps['model'], X, top_features,
            kind='average', ax=ax
        )
        plt.savefig("reports/partial_dependence.png")
        plt.close()

    def _shap_analysis(self, X: pd.DataFrame, sample_size: int = 1000):
        """Análisis SHAP para interpretabilidad del modelo"""
        explainer = shap.Explainer(self.model_pipeline.named_steps['model'])
        X_sample = self.preprocessor.transform(X.sample(sample_size))
        shap_values = explainer.shap_values(X_sample)
        
        plt.figure(figsize=(12, 8))
        shap.summary_plot(shap_values, X_sample, show=False)
        plt.savefig("reports/shap_summary.png")
        plt.close()

    def save_model(self) -> bool:
        """Guarda el modelo con metadata completa"""
        try:
            os.makedirs(os.path.dirname(self.config.model_path), exist_ok=True)
            joblib.dump({
                'pipeline': self.model_pipeline,
                'features': list(self.features.columns),
                'hyperparams': self.model_pipeline.get_params(),
                'metrics': self.evaluate(),
                'optuna_study': self.study if self.study else None
            }, self.config.model_path)
            
            self.logger.info(f"Modelo guardado en {self.config.model_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error guardando modelo: {str(e)}")
            return False

if __name__ == "__main__":
    trainer = NonceTrainer()
    if trainer.train(use_optuna=True):
        metrics = trainer.evaluate()
        trainer.logger.info(f"Métricas finales: {metrics}")
        trainer.save_model()