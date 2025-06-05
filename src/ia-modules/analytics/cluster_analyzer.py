import numpy as np
import pandas as pd
from sklearn.cluster import KMeans, DBSCAN, AgglomerativeClustering
from sklearn.preprocessing import StandardScaler
import matplotlib.pyplot as plt

class ClusterAnalyzer:
    def __init__(self, method='kmeans', n_clusters=5, eps=0.5, min_samples=5, linkage='ward'):
        """
        Inicializa el analizador de clustering.

        Args:
            method (str): Método de clustering ('kmeans', 'dbscan', 'agglomerative').
            n_clusters (int): Número de clusters para KMeans y AgglomerativeClustering.
            eps (float): Parámetro epsilon para DBSCAN.
            min_samples (int): Parámetro mínimo de muestras para DBSCAN.
            linkage (str): Enlace para AgglomerativeClustering ('ward', 'complete', 'average', 'single').
        """
        self.method = method
        self.n_clusters = n_clusters
        self.eps = eps
        self.min_samples = min_samples
        self.linkage = linkage
        self.scaler = StandardScaler()
        self.model = self._get_clustering_model()

    def _get_clustering_model(self):
        """
        Obtiene el modelo de clustering basado en el método especificado.

        Returns:
            object: Instancia del modelo de clustering.
        """
        if self.method == 'kmeans':
            return KMeans(n_clusters=self.n_clusters, random_state=42)
        elif self.method == 'dbscan':
            return DBSCAN(eps=self.eps, min_samples=self.min_samples)
        elif self.method == 'agglomerative':
            return AgglomerativeClustering(n_clusters=self.n_clusters, linkage=self.linkage)
        else:
            raise ValueError(f"Método de clustering no reconocido: {self.method}")

    def fit(self, X):
        """
        Ajusta el modelo de clustering a los datos.

        Args:
            X (pd.DataFrame or np.ndarray): Datos de entrada.
        """
        X_scaled = self.scaler.fit_transform(X)
        self.model.fit(X_scaled)

    def predict(self, X):
        """
        Predice las etiquetas de clúster para los datos de entrada.

        Args:
            X (pd.DataFrame or np.ndarray): Datos de entrada.

        Returns:
            np.ndarray: Etiquetas de clúster.
        """
        X_scaled = self.scaler.transform(X)
        return self.model.predict(X_scaled)

    def plot_clusters(self, X, labels=None, title="Clusters"):
        """
        Grafica los clusters en los datos.

        Args:
            X (pd.DataFrame or np.ndarray): Datos de entrada.
            labels (np.ndarray): Etiquetas de clúster. Si None, usa las predicciones del modelo.
            title (str): Título del gráfico.
        """
        if labels is None:
            labels = self.predict(X)

        plt.figure(figsize=(10, 6))
        plt.scatter(X[:, 0], X[:, 1], c=labels, cmap='viridis', marker='o')
        plt.title(title)
        plt.xlabel("Feature 1")
        plt.ylabel("Feature 2")
        plt.colorbar(label="Cluster Label")
        plt.grid()
        plt.show()

    def save_model(self, model_path):
        """
        Guarda el modelo de clustering en un archivo.

        Args:
            model_path (str): Ruta del archivo donde guardar el modelo.
        """
        joblib.dump((self.scaler, self.model), model_path)
        print(f"✅ Modelo de clustering guardado en {model_path}")

    def load_model(self, model_path):
        """
        Carga un modelo de clustering desde un archivo.

        Args:
            model_path (str): Ruta del archivo desde donde cargar el modelo.
        """
        self.scaler, self.model = joblib.load(model_path)
        print(f"✅ Modelo de clustering cargado desde {model_path}")

# Ejemplo de uso de la clase ClusterAnalyzer
if __name__ == "__main__":
    # Cargar datos de nonces desde el archivo CSV
    data_path = '../data/nonce_training_data.csv'
    df = pd.read_csv(data_path)

    # Seleccionar características para el análisis de clustering
    features = df.drop(columns=["nonce", "target"]).values

    # Inicializar el analizador de clustering
    cluster_analyzer = ClusterAnalyzer(method='kmeans', n_clusters=5)

    # Ajustar el modelo de clustering a los datos
    cluster_analyzer.fit(features)

    # Predecir las etiquetas de clúster
    labels = cluster_analyzer.predict(features)

    # Graficar los clusters
    cluster_analyzer.plot_clusters(features, labels, title="Clusters de Nonces")

    # Guardar el modelo entrenado
    model_path = '../models/cluster_model.joblib'
    cluster_analyzer.save_model(model_path)

    # Cargar el modelo entrenado
    cluster_analyzer.load_model(model_path)

    # Predecir las etiquetas de clúster con el modelo cargado
    loaded_labels = cluster_analyzer.predict(features)
    print(f"Etiquetas de Clúster Cargadas: {loaded_labels}")