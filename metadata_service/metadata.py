from pymongo import MongoClient 
class Metadata:
    def __init__(self, host):
        self.client = MongoClient(host)
        self.objLocation = self.client.object.location
        self.objLocation.create_index([("key", 1), ("version", -1)])

    def beginTransaction(self, key, version):
        self.objLocation.insert_one({"key": key, "version": version, "status": "tentative", "hosts": []})
    
    def addLocation(self, key, version, pod, host, blockId, offset, size):
        objectKey = {"key": key, "version": version}
        location = {"pod": pod, "host": host, "blockId": blockId, "offset": offset, "size": size}
        self.objLocation.update(objectKey, {"$push": {"hosts": location}})

    def commitTransaction(self, key, version):
        objectKey = {"key": key, "version": version}
        self.objLocation.update_one(objectKey, {"$set": {"status": "online"}})

    def get(self, key, version):
        objectKey = {"key": key, "version": version, "status": "online"}
        return self.objLocation.find_one(objectKey)

    def getAll(self):
        objectsInfo = list(self.objLocation.find())
        for objectInfo in objectsInfo:
            del objectInfo["_id"]
        
        return objectsInfo
        
