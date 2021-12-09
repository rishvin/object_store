import flask
from flask import Flask, request, copy_current_request_context, jsonify
from werkzeug.utils import secure_filename
import gevent
import requests
import gevent.monkey
gevent.monkey.patch_all()
import sys
import json

sys.path.append("../metadata_service")
from metadata import Metadata 

storageServers = ["http://0.0.0.0:50002", "http://0.0.0.0:50001"]

metadataServer = Metadata("localhost:27017")

app = Flask(__name__)

def getPod():
  return ("pod1", storageServers)

@app.route("/", methods=["POST"])
def handlePost():
  if "file" not in request.files:
    return ("Invalid request", "400")

  file = request.files["file"]
  if file.filename == "":
    return ("Invalid object name", "400")

  obj = file.read()

  pod, storageServers = getPod()
  metadataServer.beginTransaction(file.filename, 0)

  @copy_current_request_context
  def asyncPost(url):
    filename = secure_filename(file.filename)
    headers={"objectVersion": "1", "objectKey": filename}
    resp = requests.post(url, data=obj, headers=headers)
    print ("Got resp ", resp) 
    if resp.status_code == 200:
      print ("About to print the metadata")
      metadata = json.loads(resp.content.decode('utf-8'))
      print(metadata)
      metadataServer.addLocation(file.filename,
                                 0,
                                 pod,
                                 url,
                                 metadata["blockId"],
                                 metadata["offset"],
                                 len(obj))
    
    return resp
   
  threads = []
  for url in storageServers:
    threads.append(gevent.spawn(asyncPost, url))
  gevent.joinall(threads)

  for thread in threads:
    resp = thread.value
    if resp is None:
      return ("Service down", "503")

  metadataServer.commitTransaction(file.filename, 0)

  return ("Object stored", "200")

@app.route("/<objectKey>", methods=["GET"])
def handleGet(objectKey):
  objectVersion = 0
  metadata = metadataServer.get(objectKey, objectVersion)
  if not metadata or not metadata["hosts"]:
    return ("Object not found", "404")
  
  host = metadata["hosts"][0]

  url = "{}/{}/{}/{}/{}/{}".format(host["host"],
                                   objectKey,
                                   objectVersion,
                                   host["blockId"],
                                   host["offset"],
                                   host["size"])
  
  resp = requests.get(url)
  if resp.status_code != 200:
    return (resp.content, resp.status_code)

  response = flask.Response(response=resp.content, status="202")
  response.headers.set("Content-Disposition:", "attachment", filename="%s" % objectKey)
  return response

@app.route('/metadata', methods=["GET"])
def handleMetadataRequest():
  metadata = metadataServer.getAll()
  metadata = json.dumps(metadata, indent=2)
  return metadata

if __name__ == '__main__':
  app.run(debug=False)
