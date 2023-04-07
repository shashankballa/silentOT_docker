# SilentOT from libOTe with docker

### Build Instructions 

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