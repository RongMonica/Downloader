from flask import Flask, jsonify, request, send_from_directory
import os

app = Flask(__name__)

# Serve downloadable files from the user's repository folder.
FILES_DIRECTORY = os.path.expanduser("~/Rong_coding/Github/Practices/files")

@app.get("/")
def home():
    return "Welcome! Try /ping or /download/<filename>\n"

@app.get("/ping")
def ping():
    return jsonify(ok=True, msg="pong")

@app.post("/echo")
def echo():
    data = request.get_json(silent=True) or {}
    return jsonify(received=data)

@app.get("/download/<path:filename>")
def download_file(filename):
    directory = "/home/l/Rong_coding/Github/Practice/files"
    return send_from_directory(directory, filename, as_attachment=True)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=True)
