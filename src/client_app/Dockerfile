# Use a base image with C++ development tools. Alpine is a good choice for smaller image size.
FROM alpine:latest

# Install g++ and make. These are the tools necessary to compile the C++ source code.
# The --no-cache flag cleans up the temporary files after the installation.
RUN apk update && apk add --no-cache g++ make

# Set the working directory inside the container. This is where the application's files will reside.
WORKDIR /app

# Copy the source code into the container's working directory.
COPY . .

# Compile the C++ program. We use 'g++' to compile the source file 'client.cpp' into an executable 'client'.
# The '-pthread' flag is essential for linking the POSIX threads library, which the code uses.
RUN g++ -o client client.cpp -pthread

# Set the entrypoint to the compiled executable. This is the command that will run when the container starts.
ENTRYPOINT ["./client"]

# Specify the port the container will expose. While this client-side application does not *listen* on a port,
# it is good practice to document the port it interacts with.
EXPOSE 8080