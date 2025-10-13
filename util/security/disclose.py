import sys
import json 
import base64
import hashlib

def disclose(base64JSON, attributes):

    # Decode and parse the JSON object
    json_bytes = base64.b64decode(base64JSON)
    json_str = json_bytes.decode('utf-8')
    json_obj = json.loads(json_str)

    # Parse CSV string
    values = [v.strip() for v in attributes.split(',') if v.strip()]

    disclosures = []
    for item in json_obj:
        if not item[0] in values:
            disclosure_sha256 = hashlib.sha256()
            disclosure_sha256.update(json.dumps(item).encode('utf-8'))
            disclosures.append(base64.b64encode(disclosure_sha256.digest()).decode())
        else:
            disclosures.append(item)
    print (base64.b64encode(json.dumps(disclosures).encode()).decode())



def main():
    if len(sys.argv) != 3:
        print("Usage: python3 disclose.py <base64_json> <string with comma separated attributes>")
        sys.exit(1)

    base64JSON = sys.argv[1]
    attributes = sys.argv[2]
    disclose(base64JSON, attributes)
    
    

if __name__ == "__main__":
    main()