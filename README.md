## Overview ##
A cross platform messaging CLI that allows users to send text messages to other instances of the app on a different network. This is achieved using the client-server model.
I've used a domain and Cloudflare's DNS to route traffic across networks to a host server. The client instance must connect to the domain through a cloudflared tunnel and steps to do so are show below. Thanks to https://domain.digitalplat.org/ for providing a free domain and thanks to Cloudflare for distilling complicated network management down for beginners!

Installation and Running on Linux:

## Running the Client Image ##
Prerequisites: 
1. Docker Engine or Docker Desktop must be installed. Install from here - https://docs.docker.com/engine/install/
2. Cloudflare must be installed and you must have a cloudflare account. Install from here - https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/
3. You must setup a cloudflared tunnel as well. That can be done here - https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/do-more-with-tunnels/local-management/create-local-tunnel/
4. Download the client image from here - https://hub.docker.com/repository/docker/urdhva/cpp-client/general

Run the client image with the following command - 
```
docker run --network host -it --rm --device /dev/snd -v "$(pwd)/downloads:/root/Downloads" cpp-client:linux-v4
```
Replace linux-v4 with the latest version

Enable and start the cloudflare tunneling service:
```
sudo systemctl enable cloudflared
sudo systemctl start cloudflared
```

Create a tunnel and connect it to the intermediary server hosted at tochatserver.mikansi.qzz.io 
The command will look like:
```
cloudflared access tcp --hostname tochatserver.mikansi.qzz.io --url localhost:8081
```

You should be connected to the server if it is running.


## Running the Server Image ##
1. Docker Engine or Docker Desktop must be installed. Install from here - https://docs.docker.com/engine/install/
2. Cloudflare must be installed and you must have a cloudflare account. Install from here - https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/
3. You must setup a cloudflared tunnel as well. That can be done here - https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/do-more-with-tunnels/local-management/create-local-tunnel/
4. Download the server image from here - https://hub.docker.com/repository/docker/urdhva/host-server/general

Cloudflare configuration file:
Create a configuration file for cloudflared (usually in /etc/cloudflared/ or /home/<username>/.cloudflared/). It should look similar to this -
```
 tunnel: <your_tunnel_id>
credentials-file: /home/deepu/.cloudflared/2c8b97bf-57ef-4127-bd9a-791c52d5e673.json

ingress:
  - hostname: web.mickanzi.dpdns.org
    service: http://localhost:8000
  - hostname: testchatroom.mickanzi.dpdns.org
    service: tcp://localhost:8080
  - service: http_status:404
```

To run your cloudflared tunnel, run the following command - 
```
cloudflared tunnel run <your_tunnel_id>
```

After the tunnel is setup and running, you can run the server image. The command will loook something like this - 
```
docker run  --network host -it cpp-server:v4
```
Replace the version number with the latest version. 
