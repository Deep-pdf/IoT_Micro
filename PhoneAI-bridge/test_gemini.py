import os
import requests

from dotenv import load_dotenv


load_dotenv()

API_KEY = os.getenv("GEMINI_API_KEY")

if not API_KEY:
    raise RuntimeError(
        "GEMINI_API_KEY not found in .env"
    )


MODEL = "gemini-3.6-flash"

URL = (
    f"https://generativelanguage.googleapis.com/"
    f"v1beta/models/{MODEL}:generateContent"
)


headers = {
    "x-goog-api-key": API_KEY,
    "Content-Type": "application/json"
}


data = {
    "contents": [
        {
            "parts": [
                {
                    "text": "Explain recursion in one simple sentence."
                }
            ]
        }
    ]
}


try:

    response = requests.post(
        URL,
        headers=headers,
        json=data,
        timeout=30
    )

    print("HTTP STATUS:", response.status_code)

    if response.status_code != 200:

        print("\nGemini returned an error:")
        print(response.text)

        exit()

    result = response.json()

    answer = (
        result["candidates"][0]
        ["content"]["parts"][0]["text"]
    )

    print("")
    print("==============================")
    print("       GEMINI RESPONSE")
    print("==============================")
    print("")
    print(answer)
    print("")


except requests.exceptions.RequestException as error:

    print("Connection error:")
    print(error)