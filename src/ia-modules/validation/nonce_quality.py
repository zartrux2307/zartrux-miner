from typing import List
from utils.hex_validator import HexNonceValidator


class NonceQualityValidator:
    """Basic validator used by inference pipelines."""

    def __init__(self, min_length: int = 1, max_length: int = 64):
        self.validator = HexNonceValidator(min_length, max_length)

    def basic_validation(self, nonce: str) -> bool:
        return self.validator.is_valid(nonce)

    def full_validation(self, nonces: List[str]) -> List[str]:
        return [n for n in nonces if self.basic_validation(n)]