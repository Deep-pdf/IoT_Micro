import os
import requests

from flask import Flask, request, jsonify
from dotenv import load_dotenv


# Load environment variables from .env
load_dotenv()

API_KEY = os.getenv("GEMINI_API_KEY")

if not API_KEY:
    raise RuntimeError(
        "GEMINI_API_KEY not found. "
        "Create a .env file and add your Gemini API key."
    )


# Gemini model
MODEL = "gemini-3.6-flash"

# Gemini REST API endpoint
GEMINI_URL = (
    f"https://generativelanguage.googleapis.com/"
    f"v1beta/models/{MODEL}:generateContent"
)


app = Flask(__name__)


# --------------------------------------------------
# HOME / STATUS
# --------------------------------------------------

@app.route("/", methods=["GET"])
def home():
    return jsonify({
        "status": "online",
        "service": "ESP32 AI Bridge",
        "model": MODEL
    })


# --------------------------------------------------
# ASK GEMINI
# --------------------------------------------------

@app.route("/ask", methods=["POST"])
def ask():

    data = request.get_json(silent=True)

    if not data:
        return jsonify({
            "error": "Invalid JSON"
        }), 400

    question = data.get("question")

    if not question:
        return jsonify({
            "error": "Missing 'question'"
        }), 400

    print("\n================================")
    print("QUESTION FROM ESP32")
    print("================================")
    print(question)

    headers = {
        "x-goog-api-key": API_KEY,
        "Content-Type": "application/json"
    }

    payload = {
        "contents": [
            {
                "parts": [
                    {
                        "text": question
                    }
                ]
            }
        ]
    }

    try:

        response = requests.post(
            GEMINI_URL,
            headers=headers,
            json=payload,
            timeout=30
        )

        print("\nGemini HTTP Status:", response.status_code)

        if response.status_code != 200:

            print("Gemini API Error:")
            print(response.text)

            return jsonify({
                "error": "Gemini API request failed"
            }), 500

        result = response.json()

        # Extract Gemini response
        answer = (
            result["candidates"][0]
            ["content"]["parts"][0]["text"]
        )

        print("\n================================")
        print("GEMINI RESPONSE")
        print("================================")
        print(answer)

        return jsonify({
            "answer": answer
        })

    except requests.exceptions.Timeout:

        return jsonify({
            "error": "Gemini request timed out"
        }), 504

    except requests.exceptions.RequestException as error:

        print("Network error:")
        print(error)

        return jsonify({
            "error": "Network error while contacting Gemini"
        }), 500

    except (KeyError, IndexError, TypeError):

        print("Unexpected Gemini response:")
        print(response.text)

        return jsonify({
            "error": "Could not parse Gemini response"
        }), 500


# --------------------------------------------------
# START SERVER
# --------------------------------------------------

if __name__ == "__main__":

    print("")
    print("========================================")
    print("          ESP32 AI BRIDGE")
    print("========================================")
    print("")
    print("Model:", MODEL)
    print("Server starting...")
    print("")
    print("Local:")
    print("http://127.0.0.1:8080")
    print("")
    print("For ESP32:")
    print("http://PHONE_IP:8080")
    print("")
    print("Press CTRL+C to stop.")
    print("========================================")
    print("")

    app.run(
        host="0.0.0.0",
        port=8080,
        debug=False
    )