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

### Currently we are debugging so do the following

#### Run project with GDB

6. In a new terminal, Attach to the container's shell:
```
docker exec -it silentot-test bash
```

7. Run executable under GDB: 
```
gdb --args ./build/main -v 1 -nn 25
```

#### Exit the Container

8. Run:
```
docker compose down
```
