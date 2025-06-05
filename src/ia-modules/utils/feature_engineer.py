import numpy as np
import pandas as pd
from datetime import datetime, timedelta
from sklearn.preprocessing import MinMaxScaler, StandardScaler
from sklearn.impute import SimpleImputer

class FeatureEngineer:
    def __init__(self, config_path='../config/config_manager.json'):
        """
        Inicializa el ingeniero de características.

        Args:
            config_path (str): Ruta al archivo de configuración.
        """
        self.config = self.load_config(config_path)
        self.scalers = {}

    def load_config(self, config_path):
        """
        Carga la configuración desde un archivo JSON.

        Args:
            config_path (str): Ruta al archivo de configuración.

        Returns:
            dict: Configuración cargada.
        """
        import json
        with open(config_path, 'r') as file:
            config = json.load(file)
        return config

    def load_nonce_data(self, data_path):
        """
        Carga los datos de nonces desde un archivo CSV.

        Args:
            data_path (str): Ruta al archivo CSV de nonces.

        Returns:
            pd.DataFrame: Datos de nonces cargados.
        """
        return pd.read_csv(data_path)

    def preprocess_data(self, df):
        """
        Preprocesa los datos de nonces.

        Args:
            df (pd.DataFrame): Datos de nonces.

        Returns:
            pd.DataFrame: Datos preprocesados.
        """
        # Convertir columnas de fechas a datetime
        date_columns = [col for col in df.columns if 'timestamp' in col.lower()]
        for col in date_columns:
            df[col] = pd.to_datetime(df[col])

        # Imputar valores faltantes
        imputer = SimpleImputer(strategy='mean')
        df_imputed = pd.DataFrame(imputer.fit_transform(df), columns=df.columns)

        # Escalar características
        for feature in self.config.get('features_to_scale', []):
            scaler_type = self.config.get('scaler_type', 'minmax')
            if scaler_type == 'minmax':
                scaler = MinMaxScaler()
            elif scaler_type == 'standard':
                scaler = StandardScaler()
            else:
                raise ValueError(f"Scaler type {scaler_type} not recognized.")

            df_imputed[feature] = scaler.fit_transform(df_imputed[[feature]])
            self.scalers[feature] = scaler

        return df_imputed

    def create_time_features(self, df):
        """
        Crea características temporales a partir de columnas de fecha.

        Args:
            df (pd.DataFrame): Datos de nonces.

        Returns:
            pd.DataFrame: Datos con características temporales añadidas.
        """
        date_columns = [col for col in df.columns if 'timestamp' in col.lower()]

        for col in date_columns:
            df[f'{col}_year'] = df[col].dt.year
            df[f'{col}_month'] = df[col].dt.month
            df[f'{col}_day'] = df[col].dt.day
            df[f'{col}_hour'] = df[col].dt.hour
            df[f'{col}_minute'] = df[col].dt.minute
            df[f'{col}_second'] = df[col].dt.second
            df[f'{col}_weekday'] = df[col].dt.weekday
            df[f'{col}_is_weekend'] = df[col].dt.weekday.isin([5, 6]).astype(int)

        return df

    def create_lag_features(self, df, feature, lag_values):
        """
        Crea características de retardo (lag) para una característica específica.

        Args:
            df (pd.DataFrame): Datos de nonces.
            feature (str): Nombre de la característica.
            lag_values (list): Valores de retardo a crear.

        Returns:
            pd.DataFrame: Datos con características de retardo añadidas.
        """
        for lag in lag_values:
            df[f'{feature}_lag_{lag}'] = df[feature].shift(lag)
        return df

    def create_rolling_features(self, df, feature, window_sizes, agg_funcs):
        """
        Crea características de ventana móvil para una característica específica.

        Args:
            df (pd.DataFrame): Datos de nonces.
            feature (str): Nombre de la característica.
            window_sizes (list): Tamaños de ventana móviles a crear.
            agg_funcs (list): Funciones de agregación a aplicar.

        Returns:
            pd.DataFrame: Datos con características de ventana móvil añadidas.
        """
        for window in window_sizes:
            for func in agg_funcs:
                df[f'{feature}_rolling_{window}_{func}'] = df[feature].rolling(window=window).agg(func)
        return df

    def save_preprocessed_data(self, df, output_path):
        """
        Guarda los datos preprocesados en un archivo CSV.

        Args:
            df (pd.DataFrame): Datos preprocesados.
            output_path (str): Ruta del archivo de salida.
        """
        df.to_csv(output_path, index=False)
        print(f"✅ Datos preprocesados guardados en {output_path}")

    def engineer_features(self, data_path, output_path):
        """
        Realiza la ingeniería de características completas.

        Args:
            data_path (str): Ruta al archivo CSV de entrada.
            output_path (str): Ruta al archivo CSV de salida.
        """
        # Cargar datos
        df = self.load_nonce_data(data_path)

        # Preprocesar datos
        df_preprocessed = self.preprocess_data(df)

        # Crear características temporales
        df_with_time_features = self.create_time_features(df_preprocessed)

        # Crear características de retardo
        lag_values = self.config.get('lag_values', [1, 2, 3])
        for feature in self.config.get('features_for_lag', []):
            df_with_time_features = self.create_lag_features(df_with_time_features, feature, lag_values)

        # Crear características de ventana móvil
        window_sizes = self.config.get('window_sizes', [5, 10, 15])
        agg_funcs = self.config.get('agg_funcs', ['mean', 'std'])
        for feature in self.config.get('features_for_rolling', []):
            df_with_time_features = self.create_rolling_features(df_with_time_features, feature, window_sizes, agg_funcs)

        # Guardar datos preprocesados
        self.save_preprocessed_data(df_with_time_features, output_path)

# Ejemplo de uso de la clase FeatureEngineer
if __name__ == "__main__":
    # Inicializar el ingeniero de características
    feature_engineer = FeatureEngineer()

    # Ruta al archivo de datos de nonces
    data_path = '../data/nonce_training_data.csv'

    # Ruta para guardar los datos preprocesados
    output_path = '../data/nonce_preprocessed.csv'

    # Realizar la ingeniería de características
    feature_engineer.engineer_features(data_path, output_path)