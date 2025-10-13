# generate_ec_keys.py
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization

# Generate EC private key
private_key = ec.generate_private_key(ec.SECP256R1())

# Serialize to PEM (PKCS#8 for private, SubjectPublicKeyInfo for public)
private_pem = private_key.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.PKCS8,
    encryption_algorithm=serialization.NoEncryption()
)

public_pem = private_key.public_key().public_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PublicFormat.SubjectPublicKeyInfo
)

# Save to files
with open("ec_private.pem", "wb") as f:
    f.write(private_pem)

with open("ec_public.pem", "wb") as f:
    f.write(public_pem)

print("Keys written to ec_private.pem and ec_public.pem")