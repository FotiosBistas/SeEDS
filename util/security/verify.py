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

def _set_claim(json_object, keys, value):
    key = keys[0]
    if key not in json_object:
        json_object[key]={}
    if (len(keys)==1):
        json_object[key]=value
    else:
        keys.pop(0)
        _set_claim(json_object[key], keys, value)
  

def json_object(disclosures):
    output = {}
    for disclosure in disclosures:
        if isinstance(disclosure, list):
            claim = disclosure[0]
            value = disclosure[1]
            keys = claim.split("/")
            keys.pop(0) # remove $
            _set_claim(output,keys,value)
    return output

def verify(base64JSON, signature):
    try:
        json_bytes = base64.b64decode(base64JSON)
        json_str = json_bytes.decode('utf-8')
        json_obj = json.loads(json_str)
        
        proof_object = []
        for item in json_obj:
            if isinstance(item, list):
                disclosure_sha256 = hashlib.sha256()
                disclosure_sha256.update(json.dumps(item).encode('utf-8'))
                proof_object.append(base64.b64encode(disclosure_sha256.digest()).decode())
            else:
                proof_object.append(item)

        with open("ec_public.pem", "rb") as f:
            public_pem = f.read()
        # Create JWK from PEM
        key = jwk.JWK.from_pem(public_pem)
        claimed_proof = jws.JWS()
        claimed_proof.deserialize(signature)
        claimed_proof.verify(key, detached_payload=json.dumps(proof_object))
        json_bytes = base64.b64decode(base64JSON)
        json_str = json_bytes.decode('utf-8')
        json_obj = json.loads(json_str)
        print(json.dumps(json_object(json_obj)))
    except Exception as e:
        print("-1")
        sys.exit(1)

    

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 verify.py <base64_json> <signature>")
        sys.exit(1)

    base64JSON = sys.argv[1]
    signature = sys.argv[2]
    verify(base64JSON, signature)
    
    

if __name__ == "__main__":
    main()