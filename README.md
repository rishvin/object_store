An object store purpose build to store BLOB.

The object store provides a simple architecture to store objects.

<br/><br/>
# Requirements:
1. C++17 or higher
2. Python3+
3. Python Pip
4. MongoDB database package
<br/><br/>
# Building
**Install following packages using pip**
1. Flask
2. gevent
3. gunicorn
4. pybind11
5. pymongo

**Run the following commands to build the storage service. This should build a shared lib.**

cd storage_service ; ./build
<br/><br/>
# Quickly test-running the system

**Run MongoDB server. The code for now assumes running in default port.**

**Run API server**

cd api_server ; ./run_api_server

**Run storage node1**

cd storage_service/pod1/node1 ; ./run_storage_service

**Run storage node1**

cd storage_service/pod1/node2 ; ./run_storage_service


