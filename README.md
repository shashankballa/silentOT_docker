
# SilentOT with Docker using libOTe

This repository contains a Dockerized implementation of the Silent Oblivious Transfer (OT) protocol from the libOTe library, aimed at facilitating development and debugging environments. Follow these instructions to build and run the project within a Docker container.

## Quick Start

### Prerequisites

- Docker
- Docker Compose
- Git

### Setup and Build

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

### [*Temporary*] Debugging with GDB 

We are currently fixing few issues with our project, follow these steps to use GDB within the Docker container.

1. **Attach to the Container's Shell**

    In a **new terminal**, attach to the container's shell:

    ```
    docker exec -it silentot-test bash
    ```

7. **Run with GDB**

    In the **container's shell**, run executable under GDB:

    ```
    gdb --args ./build/main -v 1 -nn 20
    ```
    
    * To start the debug session, type `run`.
    * In case of a crash, GDB will indicate where the issue occurred. Utilize GDB commands like `bt` (backtrace) to inspect the state and flow of the program.
    * To exit GDB and the container's shell after debugging, `exit`.

### Shut down the Container

To stop and remove the Docker container and resources, run:

```
docker compose down
```

## Support

If you encounter any problems or have suggestions, please open an issue or submit a pull request. For detailed assistance or questions, consider reaching out directly to Shashank Balla at sballa@ucsd.edu.