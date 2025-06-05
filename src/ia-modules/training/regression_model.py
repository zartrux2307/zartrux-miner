import numpy as np
import pandas as pd
import joblib
import mlflow
from typing import Dict, Optional, Union, List
from sklearn.linear_model import LinearRegression, Lasso, Ridge
from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
from sklearn.model_selection import TimeSeriesSplit, RandomizedSearchCV
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler, PolynomialFeatures
from utils.nonce_loader import NonceLoader
from utils.data_preprocessing import DataPreprocessor
from utils.config_manager import ConfigManager
import logging
from pathlib import Path

class RegressionModel:
    """Modelo de regresión avanzado para predecir métricas de minería."""
    
    def __init__(self, config: Optional[ConfigManager] = None):
        self.config = config or ConfigManager()
        self.loader = NonceLoader(self.config)
        self.preprocessor = DataPreprocessor()
        self.logger = logging.getLogger(self.__class__.__name__)
        self.model = None
        self._initialize_model()

    def _initialize_model(self):
        """Inicializa el modelo basado en la configuración"""
        model_type = self.config.get('regression_model', 'gradient_boosting')
        
        models = {
            'linear': LinearRegression(),
            'lasso': Lasso(alpha=0.1),
            'ridge': Ridge(alpha=0.1),
            'random_forest': RandomForestRegressor(n_estimators=100),
            'gradient_boosting': GradientBoostingRegressor(n_estimators=150)
        }
        
        self.model = Pipeline([
            ('scaler', StandardScaler()),
            ('poly', PolynomialFeatures(degree=2)),
            ('regressor', models.get(model_type, GradientBoostingRegressor()))
        ])

    def load_training_data(self) -> pd.DataFrame:
        """Carga y prepara datos para entrenamiento"""
        df = self.loader.load_training_data()
        features = self.config.get('regression_features', [
            'nonce', 'block_timestamp', 'difficulty', 'hash_score'
        ])
        return self.preprocessor.process(df[features + ['target'])

    def train(self, test_size: float = 0.2):
        """Entrena el modelo con validación temporal y optimización de hiperparámetros"""
        try:
            df = self.load_training_data()
            X = df.drop('target', axis=1)
            y = df['target']
            
            # División temporal
            tscv = TimeSeriesSplit(n_splits=5)
            
            # Búsqueda de hiperparámetros
            param_dist = self.config.get('hyperparameters', {
                'regressor__n_estimators': [100, 150, 200],
                'regressor__learning_rate': [0.01, 0.1, 0.2]
            })
            
            search = RandomizedSearchCV(
                self.model,
                param_dist,
                n_iter=10,
                cv=tscv,
                scoring='neg_mean_squared_error'
            )
            
            with mlflow.start_run():
                search.fit(X, y)
                self.model = search.best_estimator_
                
                # Log de métricas
                mlflow.log_params(search.best_params_)
                mlflow.log_metric("best_score", search.best_score_)
                mlflow.sklearn.log_model(self.model, "model")
            
            self._save_model()
            return self.evaluate(X, y)
            
        except Exception as e:
            self.logger.error(f"Error en entrenamiento: {str(e)}")
            raise

    def evaluate(self, X: pd.DataFrame, y: pd.Series) -> Dict:
        """Evalúa el modelo con métricas completas"""
        preds = self.model.predict(X)
        return {
            'mae': mean_absolute_error(y, preds),
            'mse': mean_squared_error(y, preds),
            'rmse': np.sqrt(mean_squared_error(y, preds)),
            'r2': r2_score(y, preds),
            'feature_importance': self.get_feature_importance()
        }

    def get_feature_importance(self) -> Dict:
        """Obtiene importancia de características para modelos soportados"""
        try:
            if hasattr(self.model.named_steps['regressor'], 'feature_importances_'):
                features = self.model[:-1].get_feature_names_out()
                importance = self.model.named_steps['regressor'].feature_importances_
                return dict(zip(features, importance))
            return {}
        except Exception as e:
            self.logger.warning(f"No se pudo obtener importancia: {str(e)}")
            return {}

    def predict(self, input_data: Union[pd.DataFrame, List]) -> np.ndarray:
        """Realiza predicciones sobre nuevos datos"""
        if isinstance(input_data, list):
            input_data = pd.DataFrame([input_data])
        
        processed_data = self.preprocessor.transform(input_data)
        return self.model.predict(processed_data)

    def _save_model(self):
        """Guarda el modelo entrenado con metadatos"""
        model_path = Path(self.config['models_path']) / "regression_model.joblib"
        joblib.dump({
            'model': self.model,
            'features': self.model[:-1].get_feature_names_out(),
            'config': self.config.get('regression_config')
        }, model_path)
        self.logger.info(f"Modelo guardado en {model_path}")

    @classmethod
    def load(cls, config: ConfigManager):
        """Carga un modelo pre-entrenado"""
        model_path = Path(config['models_path']) / "regression_model.joblib"
        try:
            data = joblib.load(model_path)
            instance = cls(config)
            instance.model = data['model']
            return instance
        except Exception as e:
            logging.error(f"Error cargando modelo: {str(e)}")
            raise

    def plot_predictions(self, y_true: pd.Series, y_pred: pd.Series):
        """Genera visualización de predicciones vs valores reales"""
        plt.figure(figsize=(10, 6))
        plt.scatter(y_true, y_pred, alpha=0.3)
        plt.plot([y_true.min(), y_true.max()], [y_true.min(), y_true.max()], '--r')
        plt.xlabel("Valores Reales")
        plt.ylabel("Predicciones")
        plt.title("Comparación de Predicciones vs Valores Reales")
        plt.grid(True)
        
        if self.config.get('save_plots', True):
            plot_path = Path(self.config['reports_path']) / "regression_plot.png"
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            plt.close()
        else:
            plt.show()

if __name__ == "__main__":
    # Ejemplo de uso
    config = ConfigManager(config_path="config/ia_config.json")
    
    try:
        # Entrenamiento
        model = RegressionModel(config)
        metrics = model.train()
        print(f"Métricas del modelo: {metrics}")
        
        # Predicción
        sample_data = {
            'nonce': [123456],
            'block_timestamp': [pd.Timestamp.now().timestamp()],
            'difficulty': [150.5],
            'hash_score': [0.85]
        }
        prediction = model.predict(pd.DataFrame(sample_data))
        print(f"Predicción: {prediction[0]}")
        
    except Exception as e:
        print(f"Error: {str(e)}")