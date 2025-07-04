#base image uses a compiler, necessary build tools
FROM gcc:latest AS builder

#sets working directory inside the container
WORKDIR /app

# This is used when we are using an operating system. Since we are using a gcc environment
# this shouldn't be a problem
# RUN apt-get update && apt-get install -y \
    # g++ 

# the period indicates the current directory
COPY main.cpp .

RUN g++ -o ruby main.cpp

FROM debian:stable-slim

#working directory for the app
WORKDIR /app 

#copy source destination
COPY --from=builder /app/ruby .

CMD ["./ruby"]

#for running the api when the container starts??

