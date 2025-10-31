from flask import Flask, jsonify, request, send_from_directory, abort
import os

app = Flask(__name__)

# Serve downloadable files from the user's repository folder.
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
    file_path = os.path.join(directory, filename)
    if not os.path.isfile(file_path):
        abort(404)
    resp = send_from_directory(directory, filename, as_attachment=True, conditional = True)
    return resp

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=True)
