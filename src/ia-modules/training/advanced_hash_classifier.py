import pandas as pd
import numpy as np
import joblib
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, accuracy_score
from nonce_logger import NonceLogger

class HashClassifier:
    """
    Clasificador avanzado de nonces basado en Gradient Boosting.
    """
    def __init__(self):
        """
        Inicializa el clasificador con un logger y variables para el modelo y el codificador de etiquetas.
        """
        self.logger = NonceLogger("HashClassifier")
        self.model = None
        self.label_encoder = None

    def load_data(self, path: str) -> pd.DataFrame:
        """
        Carga y prepara los datos desde un archivo CSV.

        Args:
            path (str): Ruta al archivo de datos.

        Returns:
            pd.DataFrame: DataFrame con los datos cargados y procesados.
        """
        try:
            df = pd.read_csv(path)

            # Validación básica
            if df.empty:
                raise ValueError("Dataset vacío")
            if not {'nonce', 'label'}.issubset(df.columns):
                raise ValueError("Columnas faltantes en el dataset: 'nonce' o 'label'")

            # Codificación de etiquetas
            df['encoded'] = df['label'].astype('category').cat.codes
            self.label_encoder = dict(enumerate(df['label'].astype('category').cat.categories))

            self.logger.info(f"✅ Datos cargados exitosamente desde {path}.")
            return df
        except Exception as e:
            self.logger.error(f"❌ Error cargando datos: {str(e)}")
            raise

    def extract_features(self, nonce_str: str) -> dict:
        """
        Extrae características de un nonce dado.

        Args:
            nonce_str (str): Nonce en formato string.

        Returns:
            dict: Diccionario con características extraídas.
        """
        try:
            hex_str = hex(int(nonce_str))[2:] if not nonce_str.startswith('0x') else nonce_str[2:]
            features = {
                'length': len(nonce_str),
                'hex_len': len(hex_str),
                'last_byte': int(hex_str[-2:] or '0', 16)
            }
            return features
        except ValueError as e:
            self.logger.error(f"❌ Error al extraer características del nonce {nonce_str}: {e}")
            return {'length': 0, 'hex_len': 0, 'last_byte': 0}

    def train(self, data_path: str) -> bool:
        """
        Entrena el modelo con los datos proporcionados.

        Args:
            data_path (str): Ruta al archivo de datos.

        Returns:
            bool: True si el entrenamiento fue exitoso, False en caso contrario.
        """
        try:
            df = self.load_data(data_path)
            df['features'] = df['nonce'].astype(str).apply(self.extract_features)
            X = pd.DataFrame(df['features'].tolist())
            y = df['encoded']

            # División en conjunto de entrenamiento y prueba
            X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

            # Entrenamiento del modelo
            self.model = GradientBoostingClassifier()
            self.model.fit(X_train, y_train)

            # Evaluación en el conjunto de prueba
            y_pred = self.model.predict(X_test)
            accuracy = accuracy_score(y_test, y_pred)
            self.logger.info(f"✅ Modelo entrenado exitosamente con precisión: {accuracy:.2f}")
            return True
        except Exception as e:
            self.logger.error(f"❌ Error entrenando modelo: {str(e)}")
            return False

    def evaluate(self, X: pd.DataFrame, y: pd.Series) -> str:
        """
        Evalúa el modelo con los datos proporcionados.

        Args:
            X (pd.DataFrame): Características de entrada.
            y (pd.Series): Etiquetas reales.

        Returns:
            str: Reporte de clasificación.
        """
        if not self.model:
            self.logger.error("❌ El modelo no ha sido entrenado.")
            return None

        try:
            pred = self.model.predict(X)
            report = classification_report(y, pred, target_names=list(self.label_encoder.values()))
            self.logger.info("📊 Reporte de clasificación:\n" + report)
            return report
        except Exception as e:
            self.logger.error(f"❌ Error durante la evaluación: {e}")
            return None

    def save_model(self, model_path: str, encoder_path: str):
        """
        Guarda el modelo entrenado y el codificador de etiquetas.

        Args:
            model_path (str): Ruta para guardar el modelo.
            encoder_path (str): Ruta para guardar el codificador de etiquetas.
        """
        try:
            joblib.dump(self.model, model_path)
            joblib.dump(self.label_encoder, encoder_path)
            self.logger.info(f"✅ Modelo guardado en {model_path}.")
            self.logger.info(f"✅ Codificador guardado en {encoder_path}.")
        except Exception as e:
            self.logger.error(f"❌ Error guardando modelo: {e}")

if __name__ == "__main__":
    classifier = HashClassifier()
    DATA_PATH = "data/nonce_data.csv"
    MODEL_PATH = "models/hash_classifier_model.joblib"
    ENCODER_PATH = "models/label_encoder.joblib"

    if classifier.train(DATA_PATH):
        # Ejemplo de evaluación
        df = classifier.load_data(DATA_PATH)
        X = pd.DataFrame(df['nonce'].astype(str).apply(classifier.extract_features).tolist())
        report = classifier.evaluate(X, df['encoded'])

        if report:
            classifier.save_model(MODEL_PATH, ENCODER_PATH)