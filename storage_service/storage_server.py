import flask
from flask import Flask, request, url_for, redirect
from werkzeug.utils import secure_filename
import os
from pymongo import MongoClient
from flask import stream_with_context, jsonify

import sys
sys.path.append(".")
import storage_service

app = Flask(__name__)

objectPath = "."
storage_service.initBlockStore(objectPath)

@app.route("/<objectName>/<version>/<blockId>/<offset>/<objectSize>", methods=['GET'])
def handleGet(objectName, version, blockId, offset, objectSize):
  version = int(version)
  offset = int(offset)
  objectSize = int(objectSize)
  
  clientCtx = storage_service.ClientContext()
  objectData = clientCtx.read(blockId, offset, objectSize)
  return flask.Response(response=objectData, status="200")

@app.route('/', methods=['POST'])
def handlePost():
  objectKey = request.headers.get("objectKey", "")
  version = int(request.headers.get("objectVersion", "0"))

  if not request or not version or not request.data:
    return ("Invalid request", "400")
  
  objectData = request.data
  objectSize = len(objectData)
  
  clientCtx = storage_service.ClientContext()
  blockId, offset, written = clientCtx.write(objectData)
  if blockId == "":
    return ("Out of storage", "500")

  print ("Stored the object with key: ", objectKey)
  return jsonify({"blockId": blockId, "offset": offset})

if __name__ == '__main__':
  app.run(debug=False)
