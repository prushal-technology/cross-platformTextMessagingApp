#Stage 1: Build the application
FROM gcc:12.2.0 as builder

#sets working directory inside the container
WORKDIR /app

# This is used when we are using an operating system. Since we are using a gcc environment
# this shouldn't be a problem
# RUN apt-get update && apt-get install -y \
    # g++ 

# the period indicates the current directory
COPY server.cpp .

RUN g++ -std=c++17 -o  server_app server.cpp -lpthread -lrt 


#Stage 2: Creating a minimal image
FROM gcc:12.2.0 

#working directory for the app
WORKDIR /app 

#copy source destination
COPY --from=builder /app/server_app .

EXPOSE 8080

CMD ["./server_app"]