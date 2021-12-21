An object store purpose build to store BLOB.

The object store provides a simple architecture to store objects.

# Under development 

<br/><br/>
# Requirements
**NOTE: The code is currently tested in Ubuntu 20**:
1. C++17 or higher
2. Python3+
3. Python Pip
4. MongoDB database server
<br/><br/>
# Building
**Install following packages using pip**
1. flask
2. gevent
3. gunicorn
4. pybind11̌
5. pymongo
6. virtualenv
7. Refer to **requirements.txt** for any issues

**Run the following commands to build the storage service. This should build a shared lib.**

```cd storage_service ; ./build```
<br/><br/>
# Deploying for testing

**Run MongoDB server. The code for now assumes running in default port.**

**Run API server - runs at port 50000 by default**

```cd api_server ; ./run_api_server```

**Run storage node1 - runs at port 50001 by default**

```cd storage_service/pod1/node1 ; ./run_storage_service```

**Run storage node2 - runs at port 50002 by default**

```cd storage_service/pod1/node2 ; ./run_storage_service```
<br/><br/>
# Storing and getting objects
**Use POST request to store object**

```curl --location --request POST '<api_server_ip:port>' --form 'file=@"<absolute_file_path>"'```

**Use GET requst to retrieve the object**

```curl --location --request GET '<api_server_ip:port>/<object_file_name_only>'```



