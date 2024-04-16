
# SilentOT with Docker using libOTe

This repository contains a Dockerized implementation of the Silent Oblivious Transfer (OT) protocol from the libOTe library, aimed at facilitating development and debugging environments. Follow these instructions to build and run the project within a Docker container.

## 1. Quick Start

1. **Clone the Repository**

    Clone this repository to your local machine using SSH:

    ```
    git clone git@github.com:shashankballa/silentOT_docker.git
    ```

2. **Navigate to the Project Directory**
    
    Change into the project directory:
    
    ```
    cd silentOT_docker/
    ```

3. **Initialize Submodules**

    Initialize and update the git submodules:

    ```
    git submodule update --init --recursive
    ```
### 1.1 Build and run locally

4. **Build and install libOTe locally**

    Enter the directory, build, install and exit:
    
    ```
    cd libOTe

    python3 build.py --all --boost --sodium

    sudo python3 build.py --install

    cd ..
    ```

5. **Build source files**

    Create the build directory, enter and build:

    ```
    mkdir build
    
    cd build

    cmake ..

    make
    ```

6. **Run the executable**

    Run `main` with desired options:

    ```
    ./main -v 1 -nn 10
    ```


### 1.2 Build and run with Docker

4. **Build Docker Image**
    
    Build a docker image with tag `silentot:test`:
    
    ```
    docker build -t silentot:test .
    ```

5. **Launch the Container**

    Use Docker Compose to start the container:

    ```
    docker compose up
    ```
    
    If the command `docker compose` returns a "no such command" error, try using `docker-compose` instead.

#### 1.2.1 Debugging with GDB 

6. **Modify `docker-compose.yml` to enable debugging**

    Comment the line `command: ./build/main ...` and uncomment the line `tty: true`.

7. **Attach to the Container's Shell**

    In a **new terminal**, attach to the container's shell:

    ```
    docker exec -it silentot-test bash
    ```

8. **Run with GDB**

    In the **container's shell**, run executable under GDB:

    ```
    gdb --args ./build/main -v 1 -nn 20
    ```
    
    * To start the debug session, type `run`.
    * In case of a crash, GDB will indicate where the issue occurred. Utilize GDB commands like `bt` (backtrace) to inspect the state and flow of the program.
    * To exit GDB and the container's shell after debugging, `exit`.

#### 1.2.2 Shut down the Container

To stop and remove the Docker container and resources, run:

```
docker compose down
```

## Support

If you encounter any problems or have suggestions, please open an issue or submit a pull request. For detailed assistance or questions, consider reaching out directly to Shashank Balla at sballa@ucsd.edu.