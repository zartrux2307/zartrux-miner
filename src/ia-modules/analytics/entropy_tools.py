import math
from collections import Counter


class ShannonEntropyCalculator:
    """Utility to compute Shannon entropy for strings."""

    @staticmethod
    def calculate(data: str) -> float:
        if not data:
            return 0.0
        counts = Counter(data)
        total = float(len(data))
        return -sum((c / total) * math.log2(c / total) for c in counts.values())