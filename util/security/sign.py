import sys
import json 
import base64
import hashlib
from jwcrypto import jwk, jws
from secrets import token_bytes
from cryptography.hazmat.primitives import serialization

def _get_disclosures(json_object, disclosures, prefix):
    if isinstance(json_object, dict):
        for key, value in json_object.items():
            claim =  prefix + "/" + key
            disclosures.append([claim,value])
            if isinstance(value, dict) or isinstance(value, list):
                _get_disclosures(value, disclosures, claim)
    elif isinstance(json_object, list):
        for key in range(len(json_object)):
            claim =  prefix + "/" + str(key)
            value = json_object[key]
            disclosures.append([claim,value])
            if isinstance(value, dict) or isinstance(value, list):
                _get_disclosures(value, disclosures, claim)
    return disclosures
    

def disclosures(json_object):
    claims = _get_disclosures(json_object, [], "")
    return claims

def sign(base64JSON):
    try:
        # Decode base64 to JSON string
        json_bytes = base64.b64decode(base64JSON)
        json_str = json_bytes.decode('utf-8')
        # Parse JSON
        json_object = json.loads(json_str)

        with open("ec_private.pem", "rb") as f:
            private_pem = f.read()
        # Create JWK from PEM
        key = jwk.JWK.from_pem(private_pem)
        
        all_disclosures = disclosures(json_object)
        proof_object = []
        for item in all_disclosures:
            salt = base64.b64encode(token_bytes(16)).decode()
            item.append(salt)
            disclosure_sha256 = hashlib.sha256()
            disclosure_sha256.update(json.dumps(item).encode('utf-8'))
            proof_object.append(base64.b64encode(disclosure_sha256.digest()).decode())

        proof = jws.JWS(json.dumps(proof_object).encode('utf-8'))
        proof.add_signature(key, None,{"alg": "ES256"})
        object_signature= proof.serialize(compact=True)
        header, _, signature = object_signature.split('.')
        print(base64.b64encode(json.dumps(all_disclosures).encode()).decode(),header+".."+signature)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 sign.py <base64-encoded-json>")
        sys.exit(1)

    base64JSON = sys.argv[1]
    sign(base64JSON)
    

if __name__ == "__main__":
    main()