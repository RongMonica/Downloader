
from flask import Flask, jsonify, request
app = Flask(__name__)

@app.route("/", methods=["GET"])
def home():
    return "Hello, Flask!\n"

@app.route("/ping", methods=["GET"])
def ping():
    return jsonify(ok=True, msg="pong")

@app.route("/echo", methods=["POST"])
def echo():
    data = request.get_json(silent=True) or {}
    return jsonify(received=data)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=True)
