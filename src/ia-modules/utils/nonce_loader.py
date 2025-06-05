import os
import pandas as pd
import json
import gzip
import bz2

class NonceLoader:
    def __init__(self, config_path='../config/config_manager.json'):
        """
        Inicializa el cargador de nonces.

        Args:
            config_path (str): Ruta al archivo de configuración.
        """
        self.config = self.load_config(config_path)

    def load_config(self, config_path):
        """
        Carga la configuración desde un archivo JSON.

        Args:
            config_path (str): Ruta al archivo de configuración.

        Returns:
            dict: Configuración cargada.
        """
        with open(config_path, 'r') as file:
            config = json.load(file)
        return config

    def load_csv(self, file_path, compression=None, **kwargs):
        """
        Carga un archivo CSV.

        Args:
            file_path (str): Ruta al archivo CSV.
            compression (str): Tipo de compresión ('gzip', 'bz2', None).
            **kwargs: Argumentos adicionales para pd.read_csv.

        Returns:
            pd.DataFrame: Datos cargados.
        """
        if compression == 'gzip':
            return pd.read_csv(file_path, compression='gzip', **kwargs)
        elif compression == 'bz2':
            return pd.read_csv(file_path, compression='bz2', **kwargs)
        else:
            return pd.read_csv(file_path, **kwargs)

    def load_json(self, file_path, **kwargs):
        """
        Carga un archivo JSON.

        Args:
            file_path (str): Ruta al archivo JSON.
            **kwargs: Argumentos adicionales para pd.read_json.

        Returns:
            pd.DataFrame: Datos cargados.
        """
        return pd.read_json(file_path, **kwargs)

    def load_jsonl(self, file_path, **kwargs):
        """
        Carga un archivo JSON Lines.

        Args:
            file_path (str): Ruta al archivo JSON Lines.
            **kwargs: Argumentos adicionales para pd.read_json.

        Returns:
            pd.DataFrame: Datos cargados.
        """
        return pd.read_json(file_path, lines=True, **kwargs)

    def load_parquet(self, file_path, **kwargs):
        """
        Carga un archivo Parquet.

        Args:
            file_path (str): Ruta al archivo Parquet.
            **kwargs: Argumentos adicionales para pd.read_parquet.

        Returns:
            pd.DataFrame: Datos cargados.
        """
        return pd.read_parquet(file_path, **kwargs)

    def load_log_files(self, log_dir, file_extension='*.csv', compression=None, **kwargs):
        """
        Carga múltiples archivos de logs desde un directorio.

        Args:
            log_dir (str): Directorio que contiene los archivos de logs.
            file_extension (str): Extensión de los archivos de logs (ej. '*.csv').
            compression (str): Tipo de compresión ('gzip', 'bz2', None).
            **kwargs: Argumentos adicionales para el método de carga.

        Returns:
            pd.DataFrame: Datos combinados de todos los archivos de logs.
        """
        import glob
        file_paths = glob.glob(os.path.join(log_dir, file_extension))
        dfs = []
        for file_path in file_paths:
            if file_path.endswith('.csv'):
                df = self.load_csv(file_path, compression=compression, **kwargs)
            elif file_path.endswith('.json'):
                df = self.load_json(file_path, **kwargs)
            elif file_path.endswith('.jsonl'):
                df = self.load_jsonl(file_path, **kwargs)
            elif file_path.endswith('.parquet'):
                df = self.load_parquet(file_path, **kwargs)
            else:
                raise ValueError(f"Archivo no soportado: {file_path}")
            dfs.append(df)
        return pd.concat(dfs, ignore_index=True)

    def load_data(self, data_path, data_format='csv', compression=None, **kwargs):
        """
        Carga datos desde una ruta especificada.

        Args:
            data_path (str): Ruta al archivo o directorio de datos.
            data_format (str): Formato de los datos ('csv', 'json', 'jsonl', 'parquet').
            compression (str): Tipo de compresión ('gzip', 'bz2', None).
            **kwargs: Argumentos adicionales para el método de carga.

        Returns:
            pd.DataFrame: Datos cargados.
        """
        if os.path.isfile(data_path):
            if data_format == 'csv':
                return self.load_csv(data_path, compression=compression, **kwargs)
            elif data_format == 'json':
                return self.load_json(data_path, **kwargs)
            elif data_format == 'jsonl':
                return self.load_jsonl(data_path, **kwargs)
            elif data_format == 'parquet':
                return self.load_parquet(data_path, **kwargs)
            else:
                raise ValueError(f"Formato de datos no soportado: {data_format}")
        elif os.path.isdir(data_path):
            return self.load_log_files(data_path, file_extension=f'*.{data_format}', compression=compression, **kwargs)
        else:
            raise FileNotFoundError(f"No se encontró el archivo o directorio: {data_path}")

# Ejemplo de uso de la clase NonceLoader
if __name__ == "__main__":
    # Inicializar el cargador de nonces
    nonce_loader = NonceLoader()

    # Ruta al archivo de datos de nonces
    data_path = '../data/nonce_training_data.csv'

    # Cargar datos desde el archivo CSV
    df = nonce_loader.load_data(data_path, data_format='csv')

    # Mostrar las primeras filas del DataFrame
    print(df.head())