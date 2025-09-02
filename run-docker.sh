SKIP_BUILD=0

for arg in "$@"; do
  case $arg in 
    --skip-build) SKIP_BUILD=1 ;;
  esac
done

if [ "$SKIP_BUILD" -eq 0 ]; then
  echo "Building image..."
  docker build -t http-server:latest .
  docker image prune -f
fi 

echo "Stopping containers with image http-server..."
docker stop $(docker ps -q --filter ancestor=http-server:latest)
echo "Running new container with image http-server..."
docker run -p 80:8081 http-server:latest
