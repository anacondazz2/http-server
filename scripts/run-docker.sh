echo "Stopping containers with image http-server..."

containers=$(docker ps -q --filter ancestor=http-server)
if [ -n "$containers" ]; then
  docker stop $containers
  docker rm $containers
fi

SKIP_BUILD=0
ATTACH=0
EXEC=0

for arg in "$@"; do
  case $arg in 
    --skip-build) SKIP_BUILD=1 ;;
    --attach) ATTACH=1 ;;
    --exec) EXEC=1 ;;
  esac
done

if [ "$SKIP_BUILD" -eq 0 ]; then
  echo "Building image..."
  docker build -t http-server:latest .
  docker image prune -f
fi 

echo "Running new container with image http-server..."
container_id=$(docker run -d -p 8081:8081 http-server:latest)

if [ "$ATTACH" -eq 1 ]; then
  echo "Attaching to container $container_id..."
  docker attach "$container_id"
fi

if [ "$EXEC" -eq 1 ]; then
  echo "Opening shell into container $container_id..."
  docker exec -it "$container_id" /bin/bash
fi

