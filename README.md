# HTTP Server Repository Guide

Welcome to the `HTTP server` repository. This guide will help you set up and run the provided scripts.

## **Table of Contents**

1. [Cloning the Repository](#cloning-the-repository)
2. [Prerequisites](#Prerequisites)
3. [Running the `generate-cmakelists.sh` Script](#running-the-generate-cmakelistssh-script)
4. [Running the `change-compiler.sh` Script](#running-the-change-compilersh-script)
5. [Running the `build.sh` Script](#running-the-buildsh-script)
6. [Run the server](#run-the-server)

## **Cloning the Repository**

Clone the repository using the following command:

```bash
git clone https://github.com/oceaan-pendharkar/4981A1 
```

Navigate to the cloned directory:

```bash
cd 4981A1
```

Ensure the scripts are executable:

```bash
chmod +x *.sh
```

## **Prerequisites**

- to ensure you have all of the required tools installed, run:
```bash
./check-env.sh
```

If you are missing tools follow these [instructions](https://docs.google.com/document/d/1ZPqlPD1mie5iwJ2XAcNGz7WeA86dTLerFXs9sAuwCco/edit?usp=drive_link).

## **Running the generate-cmakelists.sh Script**

You will need to create the CMakeLists.txt file:

```bash
./generate-cmakelists.sh
```

## **Running the change-compiler.sh Script**

Tell CMake which compiler you want to use:

```bash
./change-compiler.sh -c <compiler>
```

To the see the list of possible compilers:

```bash
cat supported_cxx_compilers.txt
```

## **Running the build.sh Script**

To build the program run:

```bash
./build.sh
```

To build the program with all compilers run:

```bash
./build-all.sh
```

Navigate to the cloned directory:

```bash
cd build
```
## **Run the server**

To start the server, run:

```bash
./main
```
Once the server is running, you can view our homepage using the following options

### Browser 
Enter either into your browser's search bar:
```bash
localhost:8080
```

```bash
localhost:8080/index.html
```
### Command Line (Text only)
Open a new command line window and run:
```bash
localhost:8080/test.txt 
```