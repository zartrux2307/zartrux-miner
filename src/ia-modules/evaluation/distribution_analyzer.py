import numpy as np
import pandas as pd
import scipy.stats as stats
import matplotlib.pyplot as plt
import seaborn as sns

class DistributionAnalyzer:
    def __init__(self):
        """
        Inicializa el analizador de distribución.
        """
        pass

    def calculate_skewness(self, data):
        """
        Calcula la asimetría de los datos.

        Args:
            data (pd.Series or np.ndarray): Datos de entrada.

        Returns:
            float: Coeficiente de asimetría.
        """
        return stats.skew(data)

    def calculate_kurtosis(self, data):
        """
        Calcula la curtosis de los datos.

        Args:
            data (pd.Series or np.ndarray): Datos de entrada.

        Returns:
            float: Coeficiente de curtosis.
        """
        return stats.kurtosis(data)

    def plot_histogram(self, data, title="Histograma"):
        """
        Grafica el histograma de los datos.

        Args:
            data (pd.Series or np.ndarray): Datos de entrada.
            title (str): Título del gráfico.
        """
        plt.figure(figsize=(10, 6))
        sns.histplot(data, kde=True, bins=30, color='blue', edgecolor='black')
        plt.title(title)
        plt.xlabel("Valor")
        plt.ylabel("Frecuencia")
        plt.grid(True)
        plt.show()

    def plot_qq_plot(self, data, title="QQ Plot"):
        """
        Grafica el QQ Plot de los datos.

        Args:
            data (pd.Series or np.ndarray): Datos de entrada.
            title (str): Título del gráfico.
        """
        plt.figure(figsize=(10, 6))
        stats.probplot(data, dist="norm", plot=plt)
        plt.title(title)
        plt.show()

    def analyze_distribution(self, data):
        """
        Analiza la distribución de los datos calculando asimetría y curtosis,
        y graficando histograma y QQ Plot.

        Args:
            data (pd.Series or np.ndarray): Datos de entrada.

        Returns:
            dict: Diccionario con los resultados del análisis.
        """
        skewness = self.calculate_skewness(data)
        kurtosis = self.calculate_kurtosis(data)

        # Graficar histograma
        self.plot_histogram(data, title=f"Histograma de {title}")

        # Graficar QQ Plot
        self.plot_qq_plot(data, title=f"QQ Plot de {title}")

        return {'skewness': skewness, 'kurtosis': kurtosis}

# Ejemplo de uso de la clase DistributionAnalyzer
if __name__ == "__main__":
    # Cargar datos de nonces desde el archivo CSV
    data_path = '../data/nonce_training_data.csv'
    df = pd.read_csv(data_path)

    # Seleccionar una columna específica para el análisis de distribución
    column_name = 'nonce'  # Cambiar según la columna de interés
    data = df[column_name].values

    # Inicializar el analizador de distribución
    distribution_analyzer = DistributionAnalyzer()

    # Analizar la distribución
    results = distribution_analyzer.analyze_distribution(data)
    print(f"Análisis de Distribución para '{column_name}': {results}")