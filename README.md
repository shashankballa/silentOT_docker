# SilentOT from libOTe with docker

## Build Instructions 

1. Clone the git repository by running:
```
git clone git@github.com:shashankballa/silentOT_docker.git
```

2. Enter the Framework directory:
```
cd silentOT_docker/
```

3. Recursively clone all git submodules:
```
git submodule update --init --recursive
```

4. Build a docker image with tag `silentot:test`:
```
docker build -t silentot:test .
```

5. Run docker compose:
```
docker compose up
```

### Currently we are debugging

#### Run project with GDB

6. In a new terminal, attach to the container's shell:
```
docker exec -it silentot-test bash
```

7. In the container's shell, run executable under GDB: 
```
gdb --args ./build/main -v 1 -nn 25
```

8. Use GDB to Debug: Once GDB starts, you can run your program within GDB by typing `run`. GDB will show you where the crash happened, and you can use various GDB commands to inspect the state of your program at the time of the crash. Exit gdb and the container's shell after debugging.

#### Shut down the Container

8. Run:
```
docker compose down
```
