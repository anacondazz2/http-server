## Http Server
This is a simple multi-threaded web server that supports GET, POST, PUT, and DELETE requests.  
- Requested paths are sanitized (must begin with server's `webroot`) to prevent directory traversal attacks.  
- Clients can browse server contents and make changes using POST and DELETE  
- Can run in a Docker container and behind a cloudflare tunnel  
- No http library was used  

## Run
Running without a container:  
```bash
./scripts/run-local.sh
```  
And the host will be listening on port 8081.  
  
Running inside a container:  
```bash
./scripts/run-docker.sh
```
And the host will be listening on port 8081 and redirect to port 8081 on the container.  
Use `--skip-build`, `--attach`, and `--exec` options if needed.  
  
Using cloudflare:  
Have the `cloudflared` binary installed and edit the configuration in `./scripts/tunnel.sh` and `./scripts/tunnel-nohup.sh`.  
Use either script to activate the reverse proxy in either the foreground or background.  

## Cloudflare
Cloudflare's tunneling provides additional features such as blocking ill-formed request paths and reverse proxying to my machine even when its public IP changes.  

So to see the server's actual 404 response to the path `../../../etc/passwd` for example, try running locally without cloudflare, as cloudflare will block the request automatically.  
